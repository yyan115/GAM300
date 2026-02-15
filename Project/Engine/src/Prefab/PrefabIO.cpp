 #include "pch.h"
#include "Prefab/PrefabIO.hpp"
#include "Prefab/PrefabLinkComponent.hpp"

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
#include "Platform/IPlatform.h"
#include "WindowManager.hpp"
#include "Logging.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include <ECS/ECSRegistry.hpp>
#include <Serialization/Serializer.hpp>
#include <Graphics/Model/ModelFactory.hpp>

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

void EnsurePrefabLinkOn(ECSManager& ecs, Entity e, const std::string& canonicalPath)
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

    if (strcmp(typeName, "TagComponent") == 0) {
        ApplyReflectedComponent<TagComponent>(ecs, e, val, fromPrefabUpdate);
        return;
    }

    if (strcmp(typeName, "LayerComponent") == 0) {
        ApplyReflectedComponent<LayerComponent>(ecs, e, val, fromPrefabUpdate);
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
Entity SpawnPrefab(const rapidjson::Value& ents, ECSManager& ecs, bool isSerializing = false) {
    std::unordered_map<GUID_128, GUID_128> guidRemap;
    std::vector<Entity> newEntities;

    // First pass: Create entities and generate new GUIDs for each entity.
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) {
            ENGINE_LOG_WARN("[PrefabIO] Prefab member is not an object.");
            continue;
        }

        GUID_128 oldGUID = Serializer::DeserializeEntityGUID(entObj);

        // Generate a new GUID for the new entity instance.
        GUID_string newGUIDStr = GUIDUtilities::GenerateGUIDString();
        GUID_128 newGUID = GUIDUtilities::ConvertStringToGUID128(newGUIDStr);

        // Create a new entity with the new GUID.
        Entity newEntity = ecs.CreateEntityWithGUID(newGUID);

        // Store the old -> new GUID mapping.
        guidRemap[oldGUID] = newGUID;
        newEntities.push_back(newEntity);
    }


    // Second pass: Deserialize components for each new entity and fix parent/child references.
    for (size_t i = 0; i < newEntities.size(); ++i) {
        Entity entity = newEntities[i];
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) {
            ENGINE_LOG_WARN("[PrefabIO] Prefab member is not an object.");
            continue;
        }

        // Deserialize standard non-prefab components.
        // Pass true for skipSpawnChildren since all entities (including bone children)
        // were created in the first pass above and shouldn't be spawned again
        Serializer::DeserializeEntity(ecs, entObj, true, entity, true, !isSerializing);

		// Fix parent/child references based on the GUID remapping.
        const rapidjson::Value& comps = entObj["components"];
        if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
            const auto& parentCompJSON = comps["ParentComponent"];
            if (!ecs.HasComponent<ParentComponent>(entity)) {
                ecs.AddComponent<ParentComponent>(entity, ParentComponent{});
            }
            auto& parentComp = ecs.GetComponent<ParentComponent>(entity);
            Serializer::DeserializeParentComponent(parentComp, parentCompJSON, &guidRemap);
        }

        if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
            const auto& childrenCompJSON = comps["ChildrenComponent"];
            if (!ecs.HasComponent<ChildrenComponent>(entity)) {
                ecs.AddComponent<ChildrenComponent>(entity, ChildrenComponent{});
            }
            auto& childComp = ecs.GetComponent<ChildrenComponent>(entity);
            Serializer::DeserializeChildrenComponent(childComp, childrenCompJSON, &guidRemap);
        }
    }

	return newEntities.empty() ? static_cast<Entity>(-1) : newEntities[0];
}

ENGINE_API Entity InstantiatePrefabFromFile(const std::string& prefabPath, bool isSerializing)
{
    //ENGINE_LOG_INFO("[PrefabIO_v2] InstantiatePrefabFromFile called with: " + prefabPath);
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    IPlatform* platform = WindowManager::GetPlatform();

    // Normalize the path - strip leading "../../" for Android asset paths
    std::string assetPath = prefabPath;

    // Convert backslashes to forward slashes
    std::replace(assetPath.begin(), assetPath.end(), '\\', '/');

    // Strip leading "../" prefixes (common in editor-saved paths like "../../Resources/...")
    while (assetPath.substr(0, 3) == "../") {
        assetPath = assetPath.substr(3);
    }

    // Also handle paths that start with "Resources/" directly
    // Keep the path as-is if it already starts with Resources

    // For desktop, we may need the relative path with ../.. prefix
    std::string finalRelativePath;
#ifdef __ANDROID__
    finalRelativePath = assetPath;
#else
    // On desktop, try the original path first, then the normalized one
    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(prefabPath, ec);
    const std::string tryPath = (ec ? std::filesystem::path(prefabPath) : canon).generic_string();

    std::filesystem::path p(tryPath);
    std::error_code relEc;
    std::filesystem::path relativePath = std::filesystem::relative(p, relEc);

    if (!relEc) {
        finalRelativePath = relativePath.generic_string();
    }
    else {
        finalRelativePath = tryPath;
    }
#endif

    // Use platform->ReadAsset() for cross-platform file reading
    //ENGINE_LOG_INFO("[PrefabIO_v2] Trying to read: " + finalRelativePath);
    std::vector<uint8_t> buffer = platform->ReadAsset(finalRelativePath);
    if (buffer.empty()) {
        //ENGINE_LOG_WARN("[PrefabIO_v2] First path failed, trying: " + assetPath);
        // Try with the normalized asset path as fallback
        buffer = platform->ReadAsset(assetPath);
        if (buffer.empty()) {
            ENGINE_LOG_ERROR("[PrefabIO] Failed to read prefab: " + prefabPath);
            return MAX_ENTITIES;
        }
        finalRelativePath = assetPath; // Use the working path for PrefabLinkComponent
    }
    //ENGINE_LOG_INFO("[PrefabIO_v2] Successfully read " + std::to_string(buffer.size()) + " bytes");

    const std::string json(buffer.begin(), buffer.end());

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        std::cerr << "[PrefabIO] Invalid JSON in " << finalRelativePath << "\n"; return MAX_ENTITIES;
    }
    if (doc.MemberCount() == 0) { std::cout << "[PrefabIO] Prefab has no components (empty): " << finalRelativePath << "\n"; return MAX_ENTITIES; }
    if (!doc.HasMember("prefab_entities") || !doc["prefab_entities"].IsArray()) {
        ENGINE_LOG_WARN("[PrefabIO] Doc has no prefab_entities array.");
        return MAX_ENTITIES;
    }

    const rapidjson::Value& ents = doc["prefab_entities"];
    Entity prefab = SpawnPrefab(ents, ecs, isSerializing);
    EnsurePrefabLinkOn(ecs, prefab, finalRelativePath);

    // Ensure the BoneNameToEntityMap is populated if the prefab has a ModelRenderComponent.
    if (ecs.HasComponent<ModelRenderComponent>(prefab)) {
        auto& modelComp = ecs.GetComponent<ModelRenderComponent>(prefab);
        std::string entityName = ecs.GetComponent<NameComponent>(prefab).name;
        modelComp.boneNameToEntityMap[entityName] = prefab;
        ModelFactory::PopulateBoneNameToEntityMap(prefab, modelComp.boneNameToEntityMap, *modelComp.model, true);
    }

    // Ensure the root prefab object has no parent component.
    if (ecs.HasComponent<ParentComponent>(prefab)) {
        ecs.RemoveComponent<ParentComponent>(prefab);
    }

    return prefab;
}

ENGINE_API Entity InstantiatePrefabIntoEntity(const std::string& prefabPath, Entity intoEntity) {
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    IPlatform* platform = WindowManager::GetPlatform();

    // Normalize the path - strip leading "../../" for Android asset paths
    std::string assetPath = prefabPath;

    // Convert backslashes to forward slashes
    std::replace(assetPath.begin(), assetPath.end(), '\\', '/');

    // Strip leading "../" prefixes
    while (assetPath.substr(0, 3) == "../") {
        assetPath = assetPath.substr(3);
    }

    std::string finalRelativePath;
#ifdef __ANDROID__
    finalRelativePath = assetPath;
#else
    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(prefabPath, ec);
    const std::string tryPath = (ec ? std::filesystem::path(prefabPath) : canon).generic_string();

    std::filesystem::path p(tryPath);
    std::error_code relEc;
    std::filesystem::path relativePath = std::filesystem::relative(p, relEc);

    if (!relEc) {
        finalRelativePath = relativePath.generic_string();
    }
    else {
        finalRelativePath = tryPath;
    }
#endif

    // Use platform->ReadAsset() for cross-platform file reading
    std::vector<uint8_t> buffer = platform->ReadAsset(finalRelativePath);
    if (buffer.empty()) {
        buffer = platform->ReadAsset(assetPath);
        if (buffer.empty()) {
            std::cerr << "[PrefabIO] Failed to read prefab: " << prefabPath << " (tried: " << finalRelativePath << ", " << assetPath << ")\n";
            return MAX_ENTITIES;
        }
        finalRelativePath = assetPath;
    }

    const std::string json(buffer.begin(), buffer.end());

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        std::cerr << "[PrefabIO] Invalid JSON in " << finalRelativePath << "\n"; return MAX_ENTITIES;
    }
    if (doc.MemberCount() == 0) { std::cout << "[PrefabIO] Prefab has no components (empty): " << finalRelativePath << "\n"; return MAX_ENTITIES; }
    if (!doc.HasMember("prefab_entities") || !doc["prefab_entities"].IsArray()) {
        ENGINE_LOG_WARN("[PrefabIO] Doc has no prefab_entities array.");
        return MAX_ENTITIES;
    }

    const rapidjson::Value& ents = doc["prefab_entities"];

    // Before deleting the existing prefab instance, store a copy of its transform to restore its original values later.
	Transform prevTransform = ecs.GetComponent<Transform>(intoEntity);
    std::string prevName = ecs.GetComponent<NameComponent>(intoEntity).name;

    // Capture Parent Info
    Entity parentEntity = static_cast<Entity>(-1);
    if (ecs.HasComponent<ParentComponent>(intoEntity)) {
        parentEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecs.GetComponent<ParentComponent>(intoEntity).parent);
    }

	// Delete the existing prefab instance entity and spawn a new one in its place.
    ecs.DestroyEntity(intoEntity);

    Entity prefab = SpawnPrefab(ents, ecs);
    EnsurePrefabLinkOn(ecs, prefab, finalRelativePath);

    // Ensure the BoneNameToEntityMap is populated if the prefab has a ModelRenderComponent.
    if (ecs.HasComponent<ModelRenderComponent>(prefab)) {
        auto& modelComp = ecs.GetComponent<ModelRenderComponent>(prefab);
        std::string entityName = ecs.GetComponent<NameComponent>(prefab).name;
        modelComp.boneNameToEntityMap[entityName] = prefab;
        ModelFactory::PopulateBoneNameToEntityMap(prefab, modelComp.boneNameToEntityMap, *modelComp.model, true);
    }

    // Ensure the root prefab object has no parent component.
    if (ecs.HasComponent<ParentComponent>(prefab)) {
        ecs.RemoveComponent<ParentComponent>(prefab);
    }

	// Restore the previous transform values to keep the same position and rotation.
    if (ecs.HasComponent<Transform>(prefab)) {
		auto& transform = ecs.GetComponent<Transform>(prefab);
        transform.localPosition = prevTransform.localPosition;
        transform.localRotation = prevTransform.localRotation;
		transform.isDirty = true; // Mark for update
    }

    // Restore the previous name.
    if (ecs.HasComponent<NameComponent>(prefab)) {
        auto& nameComp = ecs.GetComponent<NameComponent>(prefab);
        nameComp.name = prevName;
    }

    // Restore Parent Link
    if (parentEntity != static_cast<Entity>(-1)) {
        // 1. Add ParentComponent to the new entity
        if (!ecs.HasComponent<ParentComponent>(prefab)) {
            ecs.AddComponent<ParentComponent>(prefab, ParentComponent{});
        }
        ecs.GetComponent<ParentComponent>(prefab).parent = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(parentEntity);

        // 2. Add new entity GUID to the Parent's ChildrenComponent
        if (!ecs.HasComponent<ChildrenComponent>(parentEntity)) {
            ecs.AddComponent<ChildrenComponent>(parentEntity, ChildrenComponent{});
        }
        auto& parentChildren = ecs.GetComponent<ChildrenComponent>(parentEntity);
        GUID_128 newGUID = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(prefab);
        parentChildren.children.push_back(newGUID);
    }

    return prefab;
}

//// ============ INSTANTIATE INTO EXISTING (propagate/update) ============
//// NOTE: added 'resolveAssets' parameter (default it in the header).
//ENGINE_API bool InstantiatePrefabIntoEntity(
//    ECSManager& ecs,
//    AssetManager& /*assets*/,
//    const std::string& prefabPath,
//    Entity intoEntity,
//    bool keepExistingPosition,
//    bool resolveAssets)
//{
//    std::error_code ec;
//    std::filesystem::path canon = std::filesystem::weakly_canonical(prefabPath, ec);
//    const std::string tryPath = (ec ? std::filesystem::path(prefabPath) : canon).generic_string();
//
//    if (!std::filesystem::exists(tryPath)) { std::cerr << "[PrefabIO] File does not exist: " << tryPath << "\n"; return false; }
//
//    // Ensure the instance carries a PrefabLinkComponent pointing to this prefab
//    if (ecs.IsComponentTypeRegistered<PrefabLinkComponent>()) {
//        if (!ecs.HasComponent<PrefabLinkComponent>(intoEntity))
//            ecs.AddComponent<PrefabLinkComponent>(intoEntity, PrefabLinkComponent{});
//
//        auto& link = ecs.GetComponent<PrefabLinkComponent>(intoEntity);
//        link.prefabPath = tryPath; // canonical path (same normalization PrefabEditor uses)
//    }
//
//    EnsurePrefabLinkOn(ecs, intoEntity, tryPath);
//
//    std::string json;
//    {
//        std::ifstream f(tryPath, std::ios::binary);
//        if (!f) { std::cerr << "[PrefabIO] Failed to open prefab: " << tryPath << "\n"; return false; }
//        json.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
//    }
//
//    rapidjson::Document doc;
//    doc.Parse(json.c_str());
//    if (doc.HasParseError() || !doc.IsObject()) { std::cerr << "[PrefabIO] Invalid JSON in " << tryPath << "\n"; return false; }
//    if (doc.MemberCount() == 0) { std::cout << "[PrefabIO] Prefab empty: " << tryPath << "\n"; return true; }
//
//    Vector3D prevLocalPos{};
//    bool hadPrevTransform = false;
//    if (keepExistingPosition && ecs.HasComponent<Transform>(intoEntity)) {
//        prevLocalPos = ecs.GetComponent<Transform>(intoEntity).localPosition;
//        hadPrevTransform = true;
//    }
//
//    SpawnGuard guard(keepExistingPosition, intoEntity);
//
//    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
//        const char* typeName = it->name.GetString();
//        try { ApplyOne(ecs, intoEntity, typeName, it->value, /*fromPrefabUpdate*/true, resolveAssets); }
//        catch (const std::exception& ex) { std::cerr << "[PrefabIO] Exception applying '" << typeName << "': " << ex.what() << "\n"; }
//        catch (...) { std::cerr << "[PrefabIO] Unknown error applying '" << typeName << "'\n"; }
//    }
//
//    if (keepExistingPosition && hadPrevTransform && ecs.HasComponent<Transform>(intoEntity)) {
//        auto& t = ecs.GetComponent<Transform>(intoEntity);
//        t.localPosition = prevLocalPos;
//        t.isDirty = true;
//    }
//
//    return true;
//}

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

void SaveEntityRecursive(ECSManager& ecs, Entity e, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& prefabEntitiesArr) {
    auto entObj = Serializer::SerializeEntity(e, alloc);
    prefabEntitiesArr.PushBack(entObj, alloc);

	if (ecs.HasComponent<ChildrenComponent>(e)) {
        const auto& childrenComp = ecs.GetComponent<ChildrenComponent>(e);
        for (auto& child : childrenComp.children) {
            SaveEntityRecursive(ecs, EntityGUIDRegistry::GetInstance().GetEntityByGUID(child), alloc, prefabEntitiesArr);
        }
	}
}

ENGINE_API bool SaveEntityToPrefabFile(
    ECSManager& ecs,
    AssetManager& /*assets*/,
    Entity e,
    const std::string& outPath)
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value prefabEntitiesArr(rapidjson::kArrayType);

	// Recursively save the prefab entity and its children.
    SaveEntityRecursive(ecs, e, alloc, prefabEntitiesArr);

    doc.AddMember("prefab_entities", prefabEntitiesArr, alloc);

    if (doc.ObjectEmpty()) { std::cerr << "[PrefabIO] Prefab has no components (empty): " << outPath << "\n"; return false; }

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);

    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
    std::ofstream f(outPath, std::ios::binary);
    if (!f) return false;
    f.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
    return true;
}