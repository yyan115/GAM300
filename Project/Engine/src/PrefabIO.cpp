#include "pch.h"
#include "PrefabIO.hpp"
#include "PrefabLinkComponent.hpp"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <sstream>
#include <type_traits> // std::void_t

#include "Reflection/ReflectionBase.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "ECS/NameComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include <ECS/ECSRegistry.hpp>

// ---------- helpers ----------
static inline bool IsZeroGUID(const GUID_128& g) { return g.high == 0 && g.low == 0; }

// Detect T::overrideFromPrefab (portable, no C++20 requires)
template <typename, typename = void> struct HasOverrideFlag : std::false_type {};
template <typename T>
struct HasOverrideFlag<T, std::void_t<decltype(std::declval<const T&>().overrideFromPrefab)>> : std::true_type {};
template <typename T> constexpr bool HasOverrideFlag_v = HasOverrideFlag<T>::value;
template <typename T> inline bool IsOverriddenFromPrefab(const T& t) {
    if constexpr (HasOverrideFlag_v<T>) return !!t.overrideFromPrefab;
    else                                return false;
}

static inline void EnsurePrefabLinkOn(ECSManager& ecs, Entity e, const std::string& canonicalPath)
{
    if (!ecs.IsComponentTypeRegistered<PrefabLinkComponent>()) return;

    if (!ecs.HasComponent<PrefabLinkComponent>(e))
        ecs.AddComponent<PrefabLinkComponent>(e, PrefabLinkComponent{});

    ecs.GetComponent<PrefabLinkComponent>(e).prefabPath = canonicalPath; // canonical, normalized
}

// Keep-position context for updates
namespace {
    struct PrefabSpawnContext {
        bool   active = false;
        bool   keepExistingPosition = false;
        Entity target = static_cast<Entity>(-1);
    };
    thread_local PrefabSpawnContext g_spawn;

    struct SpawnGuard {
        SpawnGuard(bool keepPos, Entity target) { g_spawn = { true, keepPos, target }; }
        ~SpawnGuard() { g_spawn = {}; }
    };
}

// Strip instance-only flags before saving
static void StripOverrides(rapidjson::Value& v)
{
    if (v.IsObject()) {
        if (v.HasMember("overrideFromPrefab"))
            v.RemoveMember("overrideFromPrefab");
        for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
            StripOverrides(it->value);
    }
    else if (v.IsArray()) {
        for (auto& e : v.GetArray()) StripOverrides(e);
    }
}

// ============ APPLY ============
template <typename T>
static void ApplyReflectedComponent(ECSManager& ecs,
    Entity e,
    const rapidjson::Value& json,
    bool fromPrefabUpdate)
{
    if (fromPrefabUpdate && ecs.HasComponent<T>(e)) {
        const T& cur = ecs.GetComponent<T>(e);
        if (IsOverriddenFromPrefab(cur)) return;
    }
    T value{};
    TypeResolver<T>::Get()->Deserialize(&value, json);
    if (ecs.HasComponent<T>(e)) ecs.GetComponent<T>(e) = value;
    else                        ecs.AddComponent<T>(e, value);
}

// Central apply (resolveAssets = whether to resolve model/shader GUIDs)
static void ApplyOne(ECSManager& ecs,
    Entity e,
    const char* typeName,
    const rapidjson::Value& val,
    bool fromPrefabUpdate,
    bool resolveAssets)
{
    if (strcmp(typeName, "NameComponent") == 0) {
        ApplyReflectedComponent<NameComponent>(ecs, e, val, fromPrefabUpdate);
        return;
    }

    if (strcmp(typeName, "Transform") == 0) {
        if (fromPrefabUpdate && ecs.HasComponent<Transform>(e)) {
            const auto& cur = ecs.GetComponent<Transform>(e);
            if (IsOverriddenFromPrefab(cur)) return;
        }
        Transform incoming{};
        TypeResolver<Transform>::Get()->Deserialize(&incoming, val);
        if (ecs.HasComponent<Transform>(e)) ecs.GetComponent<Transform>(e) = incoming;
        else                                ecs.AddComponent<Transform>(e, incoming);
        return;
    }

    if (strcmp(typeName, "ModelRenderComponent") == 0) {
        if (fromPrefabUpdate && ecs.HasComponent<ModelRenderComponent>(e)) {
            const auto& cur = ecs.GetComponent<ModelRenderComponent>(e);
            if (IsOverriddenFromPrefab(cur)) return;
        }

        ModelRenderComponent mrc{};
        TypeResolver<ModelRenderComponent>::Get()->Deserialize(&mrc, val);

        if (resolveAssets) {
            mrc.model = IsZeroGUID(mrc.modelGUID) ? nullptr : AssetManager::GetInstance().LoadByGUID<Model>(mrc.modelGUID);
            mrc.shader = IsZeroGUID(mrc.shaderGUID) ? nullptr : AssetManager::GetInstance().LoadByGUID<Shader>(mrc.shaderGUID);
        }
        else {
            // sandbox/editor: don't kick off asset loads, keep inert
            mrc.model = nullptr;
            mrc.shader = nullptr;
        }

        if (ecs.HasComponent<ModelRenderComponent>(e)) ecs.GetComponent<ModelRenderComponent>(e) = mrc;
        else                                           ecs.AddComponent<ModelRenderComponent>(e, mrc);
        return;
    }

    std::cerr << "[PrefabIO] No applier for component '" << typeName << "'\n";
}

// ============ INSTANTIATE (new entity) ============
ENGINE_API bool InstantiatePrefabFromFile(const std::string& prefabPath)
{
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    Entity intoEntity = ecs.CreateEntity();

    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(prefabPath, ec);
    const std::string tryPath = (ec ? std::filesystem::path(prefabPath) : canon).generic_string();

    if (!std::filesystem::exists(tryPath)) { std::cerr << "[PrefabIO] File does not exist: " << tryPath << "\n"; return false; }

    // Ensure the instance carries a PrefabLinkComponent pointing to this prefab
    if (ecs.IsComponentTypeRegistered<PrefabLinkComponent>()) {
        if (!ecs.HasComponent<PrefabLinkComponent>(intoEntity))
            ecs.AddComponent<PrefabLinkComponent>(intoEntity, PrefabLinkComponent{});

        auto& link = ecs.GetComponent<PrefabLinkComponent>(intoEntity);
        link.prefabPath = tryPath; // canonical path (same normalization PrefabEditor uses)
    }

    EnsurePrefabLinkOn(ecs, intoEntity, tryPath);

    std::ifstream f(tryPath, std::ios::binary);
    if (!f) { std::cerr << "[PrefabIO] Failed to open prefab: " << tryPath << "\n"; return false; }
    const std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        std::cerr << "[PrefabIO] Invalid JSON in " << tryPath << "\n"; return false;
    }
    if (doc.MemberCount() == 0) { std::cout << "[PrefabIO] Prefab has no components (empty): " << tryPath << "\n"; return true; }

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        const char* typeName = it->name.GetString();
        try { ApplyOne(ecs, intoEntity, typeName, it->value, /*fromPrefabUpdate*/false, /*resolveAssets*/true); }
        catch (const std::exception& ex) { std::cerr << "[PrefabIO] Exception applying '" << typeName << "': " << ex.what() << "\n"; }
        catch (...) { std::cerr << "[PrefabIO] Unknown error applying '" << typeName << "'\n"; }
    }

    return true;
}

// ============ INSTANTIATE INTO EXISTING (propagate/update) ============
// NOTE: added 'resolveAssets' parameter (default it in the header).
ENGINE_API bool InstantiatePrefabIntoEntity(
    ECSManager& ecs,
    AssetManager& /*assets*/,
    const std::string& prefabPath,
    Entity intoEntity,
    bool keepExistingPosition,
    bool resolveAssets)
{
    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(prefabPath, ec);
    const std::string tryPath = (ec ? std::filesystem::path(prefabPath) : canon).generic_string();

    if (!std::filesystem::exists(tryPath)) { std::cerr << "[PrefabIO] File does not exist: " << tryPath << "\n"; return false; }

    // Ensure the instance carries a PrefabLinkComponent pointing to this prefab
    if (ecs.IsComponentTypeRegistered<PrefabLinkComponent>()) {
        if (!ecs.HasComponent<PrefabLinkComponent>(intoEntity))
            ecs.AddComponent<PrefabLinkComponent>(intoEntity, PrefabLinkComponent{});

        auto& link = ecs.GetComponent<PrefabLinkComponent>(intoEntity);
        link.prefabPath = tryPath; // canonical path (same normalization PrefabEditor uses)
    }

    EnsurePrefabLinkOn(ecs, intoEntity, tryPath);

    std::string json;
    {
        std::ifstream f(tryPath, std::ios::binary);
        if (!f) { std::cerr << "[PrefabIO] Failed to open prefab: " << tryPath << "\n"; return false; }
        json.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError() || !doc.IsObject()) { std::cerr << "[PrefabIO] Invalid JSON in " << tryPath << "\n"; return false; }
    if (doc.MemberCount() == 0) { std::cout << "[PrefabIO] Prefab empty: " << tryPath << "\n"; return true; }

    Vector3D prevLocalPos{};
    bool hadPrevTransform = false;
    if (keepExistingPosition && ecs.HasComponent<Transform>(intoEntity)) {
        prevLocalPos = ecs.GetComponent<Transform>(intoEntity).localPosition;
        hadPrevTransform = true;
    }

    SpawnGuard guard(keepExistingPosition, intoEntity);

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        const char* typeName = it->name.GetString();
        try { ApplyOne(ecs, intoEntity, typeName, it->value, /*fromPrefabUpdate*/true, resolveAssets); }
        catch (const std::exception& ex) { std::cerr << "[PrefabIO] Exception applying '" << typeName << "': " << ex.what() << "\n"; }
        catch (...) { std::cerr << "[PrefabIO] Unknown error applying '" << typeName << "'\n"; }
    }

    if (keepExistingPosition && hadPrevTransform && ecs.HasComponent<Transform>(intoEntity)) {
        auto& t = ecs.GetComponent<Transform>(intoEntity);
        t.localPosition = prevLocalPos;
        t.isDirty = true;
    }

    return true;
}

// ============ SAVE ============
template <typename T>
static void SerializeToRapidJsonObject(const T& comp,
    rapidjson::Value& outObj,
    rapidjson::Document::AllocatorType& alloc)
{
    outObj.SetObject();
    std::ostringstream oss;
    TypeResolver<T>::Get()->Serialize(&comp, oss);
    rapidjson::Document tmp;
    tmp.Parse(oss.str().c_str());
    if (tmp.HasParseError()) { outObj.SetObject(); return; }
    outObj.CopyFrom(tmp, alloc);
}

template <typename T>
static void TryWrite(ECSManager& ecs, Entity e, const char* typeName, rapidjson::Document& doc)
{
    if (!ecs.HasComponent<T>(e)) return;

    const T& comp = ecs.GetComponent<T>(e);
    rapidjson::Value outVal(rapidjson::kObjectType);
    SerializeToRapidJsonObject<T>(comp, outVal, doc.GetAllocator());
    StripOverrides(outVal);

    rapidjson::Value key(typeName, doc.GetAllocator());
    doc.AddMember(key, outVal, doc.GetAllocator());
}

ENGINE_API bool SaveEntityToPrefabFile(
    ECSManager& ecs,
    AssetManager& /*assets*/,
    Entity e,
    const std::string& outPath)
{
    rapidjson::Document doc;
    doc.SetObject();

    TryWrite<NameComponent>(ecs, e, "NameComponent", doc);
    TryWrite<Transform>(ecs, e, "Transform", doc);
    TryWrite<ModelRenderComponent>(ecs, e, "ModelRenderComponent", doc);

    if (doc.ObjectEmpty()) { std::cerr << "[PrefabIO] Prefab has no components (empty): " << outPath << "\n"; return false; }

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> wr(sb);
    doc.Accept(wr);

    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
    std::ofstream f(outPath, std::ios::binary);
    if (!f) return false;
    f.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
    return true;
}