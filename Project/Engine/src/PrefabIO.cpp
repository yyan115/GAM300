#include "pch.h"
#include "PrefabIO.hpp"
#include <rapidjson/document.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "Reflection/ReflectionBase.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "ECS/NameComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include <ECS/ECSRegistry.hpp>

// --- tiny file helper ---
static bool ReadFileToString(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

// ---------- reflection-driven apply ----------
template <typename T>
static void ApplyReflectedComponent(ECSManager& ecs, Entity e, const rapidjson::Value& json)
{
    T value{};
    TypeResolver<T>::Get()->Deserialize(&value, json);
    if (ecs.HasComponent<T>(e)) ecs.GetComponent<T>(e) = value;
    else                       ecs.AddComponent<T>(e, value);
}

// Map JSON member name -> “apply” function
using ApplyFn = void(*)(ECSManager&, Entity, const rapidjson::Value&);
static std::unordered_map<std::string, ApplyFn> s_apply =
{
    // Loaders (READ)
    { "NameComponent", [](ECSManager& ecs, Entity e, const rapidjson::Value& v) {
        ApplyReflectedComponent<NameComponent>(ecs, e, v);
    }},
    { "Transform", [](ECSManager& ecs, Entity e, const rapidjson::Value& v) {
        ApplyReflectedComponent<Transform>(ecs, e, v);
    }},
    { "ModelRenderComponent", [](ECSManager& ecs, Entity e, const rapidjson::Value& v) {
    ModelRenderComponent mrc{};
    TypeResolver<ModelRenderComponent>::Get()->Deserialize(&mrc, v);

    mrc.model = AssetManager::GetInstance().LoadByGUID<Model>(mrc.modelGUID);
    mrc.shader = AssetManager::GetInstance().LoadByGUID<Shader>(mrc.shaderGUID);

    if (ecs.HasComponent<ModelRenderComponent>(e))
        ecs.GetComponent<ModelRenderComponent>(e) = mrc;
    else
        ecs.AddComponent<ModelRenderComponent>(e, mrc);
    }},
};

ENGINE_API bool InstantiatePrefabFromFile(const std::string& prefabPath)
{
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    Entity intoEntity = ecs.CreateEntity();

    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(prefabPath, ec);
    const std::string tryPath = (ec ? std::filesystem::path(prefabPath) : canon).generic_string();

    if (!std::filesystem::exists(tryPath)) {
        std::cerr << "[PrefabIO] File does not exist: " << tryPath << "\n";
        return false;
    }

    std::ifstream f(tryPath, std::ios::binary);
    if (!f) {
        std::cerr << "[PrefabIO] Failed to open prefab: " << tryPath << "\n";
        return false;
    }
    const std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        std::cerr << "[PrefabIO] JSON parse error at offset "
            << doc.GetErrorOffset() << " in " << tryPath << "\n";
        return false;
    }
    if (!doc.IsObject()) {
        std::cerr << "[PrefabIO] Root is not an object: " << tryPath << "\n";
        return false;
    }
    if (doc.MemberCount() == 0) {
        std::cout << "[PrefabIO] Prefab has no components (empty): " << tryPath << "\n";
        return true;
    }

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        const std::string typeName = it->name.GetString();
        auto fn = s_apply.find(typeName);
        if (fn == s_apply.end()) {
            std::cerr << "[PrefabIO] No applier for component '" << typeName
                << "' in " << tryPath << "\n";
            continue;
        }
        try { fn->second(ecs, intoEntity, it->value); }
        catch (const std::exception& ex) {
            std::cerr << "[PrefabIO] Exception applying '" << typeName << "': " << ex.what() << "\n";
        }
        catch (...) {
            std::cerr << "[PrefabIO] Unknown error applying '" << typeName << "'\n";
        }
    }
    return true;
}

// Try the (obj, rapidjson::Value&) form if it exists; otherwise use (obj, std::ostream&)
template <typename T>
static void SerializeToRapidJsonObject(const T& comp,
    rapidjson::Value& outObj,
    rapidjson::Document::AllocatorType& alloc)
{
    outObj.SetObject();

    // ---- path A: if your resolver has Serialize(obj, rapidjson::Value&) ----
    // SFINAE/detection would be ideal, but keep it simple: if you *know*
    // your resolver only has the ostream overload, always use the code below.
#if 0
    TypeResolver<T>::Get()->Serialize(&comp, outObj);
    (void)alloc;
#else
    // ---- path B: your resolver uses std::ostream ----
    std::ostringstream oss;
    TypeResolver<T>::Get()->Serialize(&comp, oss);

    rapidjson::Document tmp;
    tmp.Parse(oss.str().c_str());
    if (tmp.HasParseError()) {
        // if the resolver writes only the "data" portion, still make it an object
        // to avoid crashing later
        outObj.SetObject();
        return;
    }

    // If tmp is an object (e.g., {"type":"NameComponent","data":{...}}) or just the inner data,
    // copy it into 'outObj'. We don't try to second-guess schema here.
    outObj.CopyFrom(tmp, alloc);
#endif
}

// ---------- WRITE (SAVE) helpers ----------
template <typename T>
static void TryWrite(ECSManager& ecs, Entity e, const char* typeName, rapidjson::Document& doc)
{
    if (!ecs.HasComponent<T>(e)) return;

    const T& comp = ecs.GetComponent<T>(e);

    rapidjson::Value outVal(rapidjson::kObjectType);
    SerializeToRapidJsonObject<T>(comp, outVal, doc.GetAllocator());  // <--- fix

    rapidjson::Value key(typeName, doc.GetAllocator());
    doc.AddMember(key, outVal, doc.GetAllocator());
}

ENGINE_API bool SaveEntityToPrefabFile(
    ECSManager& ecs,
    AssetManager& /*assets*/,
    Entity e,
    const std::string& outPath)
{
    // Build a document of components
    rapidjson::Document doc;
    doc.SetObject();

    // ---- write components you support ----
    // helper
    auto tryWrite = [&](auto tag) { using T = decltype(tag);
    // (we'll use the fixed TryWrite<T>() below)
        };

    TryWrite<NameComponent>(ecs, e, "NameComponent", doc);
    TryWrite<Transform>(ecs, e, "Transform", doc);
    TryWrite<ModelRenderComponent>(ecs, e, "ModelRenderComponent", doc);


    // If empty, warn (so you know why nothing appears)
    if (doc.ObjectEmpty()) {
        std::cerr << "[PrefabIO] Prefab has no components (empty): " << outPath << "\n";
        return false;
    }

    // Save to disk
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> wr(sb);
    doc.Accept(wr);

    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
    std::ofstream f(outPath, std::ios::binary);
    if (!f) return false;
    f.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
    return true;
}