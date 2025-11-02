#include "pch.h"
#include "Serialization/Serializer.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "ECS/ECSRegistry.hpp"

// ---------- helpers ----------
auto readVec3FromArray = [](const rapidjson::Value& a, Vector3D& out) -> bool {
    if (!a.IsArray() || a.Size() < 3) return false;
    out.x = static_cast<float>(a[0].GetDouble());
    out.y = static_cast<float>(a[1].GetDouble());
    out.z = static_cast<float>(a[2].GetDouble());
    return true;
    };

auto getNumberFromValue = [](const rapidjson::Value& v, double& out) -> bool {
    if (v.IsNumber()) { out = v.GetDouble(); return true; }
    if (v.IsObject() && v.HasMember("data")) {
        const auto& d = v["data"];
        if (d.IsNumber()) { out = d.GetDouble(); return true; }
        if (d.IsObject() && d.HasMember("data") && d["data"].IsNumber()) { out = d["data"].GetDouble(); return true; }
    }
    return false;
    };

auto getBoolFromValue = [](const rapidjson::Value& v, bool& out) -> bool {
    if (v.IsBool()) { out = v.GetBool(); return true; }
    if (v.IsObject() && v.HasMember("data") && v["data"].IsBool()) { out = v["data"].GetBool(); return true; }
    return false;
    };

auto getStringFromValue = [](const rapidjson::Value& v, std::string& out) -> bool {
    if (v.IsString()) { out = v.GetString(); return true; }
    if (v.IsObject() && v.HasMember("data") && v["data"].IsString()) { out = v["data"].GetString(); return true; }
    return false;
    };

// read a Vector3D stored as either [x,y,z] or typed {"type":"Vector3D","data":[{...},{...},{...}]}
auto readVec3Generic = [](const rapidjson::Value& val, Vector3D& out)->bool {
    if (val.IsArray()) return readVec3FromArray(val, out);
    if (val.IsObject() && val.HasMember("data") && val["data"].IsArray()) {
        const auto& darr = val["data"];
        if (darr.Size() >= 3) {
            double x, y, z;
            if (getNumberFromValue(darr[0], x) && getNumberFromValue(darr[1], y) && getNumberFromValue(darr[2], z)) {
                out.x = static_cast<float>(x);
                out.y = static_cast<float>(y);
                out.z = static_cast<float>(z);
                return true;
            }
        }
    }
    return false;
    };

// read quaternion stored as typed object or plain array [w,x,y,z]
auto readQuatGeneric = [](const rapidjson::Value& val, double& outW, double& outX, double& outY, double& outZ)->bool {
    if (val.IsArray() && val.Size() >= 4) {
        outW = val[0].GetDouble();
        outX = val[1].GetDouble();
        outY = val[2].GetDouble();
        outZ = val[3].GetDouble();
        return true;
    }
    if (val.IsObject() && val.HasMember("data") && val["data"].IsArray()) {
        const auto& darr = val["data"];
        if (darr.Size() >= 4) {
            if (getNumberFromValue(darr[0], outW) && getNumberFromValue(darr[1], outX)
                && getNumberFromValue(darr[2], outY) && getNumberFromValue(darr[3], outZ)) {
                return true;
            }
        }
    }
    return false;
    };

// convert quaternion (w,x,y,z) -> Euler degrees (roll, pitch, yaw) in Vector3D (x=roll, y=pitch, z=yaw)
auto quatToEulerDeg = [](double w, double x, double y, double z) -> Vector3D {
    // safe pi
    const double PI = std::acos(-1.0);
    // roll (x-axis rotation)
    double sinr_cosp = 2.0 * (w * x + y * z);
    double cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
    double roll = std::atan2(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    double sinp = 2.0 * (w * y - z * x);
    double pitch;
    if (std::abs(sinp) >= 1)
        pitch = std::copysign(PI / 2.0, sinp); // use 90 degrees if out of range
    else
        pitch = std::asin(sinp);

    // yaw (z-axis rotation)
    double siny_cosp = 2.0 * (w * z + x * y);
    double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
    double yaw = std::atan2(siny_cosp, cosy_cosp);

    const double RAD2DEG = 180.0 / PI;
    return Vector3D{
        static_cast<float>(roll * RAD2DEG),
        static_cast<float>(pitch * RAD2DEG),
        static_cast<float>(yaw * RAD2DEG)
    };
    };

// ---------- end helpers ----------

void Serializer::SerializeScene(const std::string& scenePath) {
    namespace fs = std::filesystem;

    // Prepare JSON document
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value entitiesArr(rapidjson::kArrayType);

    // helper lambda to serialize a component instance (via reflection) into a rapidjson::Value
    auto serializeComponentToValue = [&](auto& compInstance) -> rapidjson::Value {
        using CompT = std::decay_t<decltype(compInstance)>;
        rapidjson::Value val; val.SetNull();

        try {
            TypeDescriptor* td = TypeResolver<CompT>::Get();
            std::stringstream ss;
            td->Serialize(&compInstance, ss);
            std::string s = ss.str();

            // parse the serialized string to a temporary document
            rapidjson::Document tmp;
            if (tmp.Parse(s.c_str()).HasParseError()) {
                // If parse fails, store the raw string instead
                rapidjson::Value strVal;
                strVal.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
                val = strVal;
            }
            else {
                // copy tmp into val using allocator
                val.CopyFrom(tmp, alloc);
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "[SaveScene] reflection serialize exception: " << ex.what() << "\n";
            // leave val as null
        }
        catch (...) {
            std::cerr << "[SaveScene] unknown exception during component serialization\n";
        }

        return val;
        };

    // Get ECS manager (guard in case there's no active manager)
    ECSManager* ecsPtr = nullptr;
    try {
        ecsPtr = &ECSRegistry::GetInstance().GetActiveECSManager();
    }
    catch (...) {
        std::cerr << "[SaveScene] no active ECSManager available; aborting save\n";
        return;
    }
    ECSManager& ecs = *ecsPtr;
    auto& guidRegistry = EntityGUIDRegistry::GetInstance();

    // Iterate entities
    for (auto entity : ecs.GetAllEntities())
    {
        rapidjson::Value entObj(rapidjson::kObjectType);

        // add entity id (assumes entity is integer-like)
        {
            rapidjson::Value idv;
            idv.SetUint64(static_cast<uint64_t>(entity)); // adapt if entity type differs
            entObj.AddMember("id", idv, alloc);
            // convert GUID to string
            GUID_string entityGUIDStr =
                GUIDUtilities::ConvertGUID128ToString(
                    guidRegistry.GetGUIDByEntity(static_cast<Entity>(entity)));

            // create a RapidJSON string value with allocator
            rapidjson::Value guidv;
            guidv.SetString(entityGUIDStr.c_str(),
                static_cast<rapidjson::SizeType>(entityGUIDStr.length()),
                alloc);

            entObj.AddMember("guid", guidv, alloc);
        }

        rapidjson::Value compsObj(rapidjson::kObjectType);

        // For each component type, if entity has it, serialize and attach under its name
        if (ecs.HasComponent<NameComponent>(entity)) {
            auto& c = ecs.GetComponent<NameComponent>(entity);

            // Build { "name": "<the name>" } object
            rapidjson::Value nameObj(rapidjson::kObjectType);
            rapidjson::Value nameStr;
            nameStr.SetString(c.name.c_str(),
                static_cast<rapidjson::SizeType>(c.name.size()),
                alloc);
            nameObj.AddMember(rapidjson::Value("name", alloc).Move(), nameStr, alloc);

            // Add under "NameComponent" key (ensure key uses allocator)
            compsObj.AddMember(rapidjson::Value("NameComponent", alloc).Move(),
                nameObj,
                alloc);
        }
        if (ecs.HasComponent<TagComponent>(entity)) {
            auto& c = ecs.GetComponent<TagComponent>(entity);
            rapidjson::Value tagObj(rapidjson::kObjectType);
            rapidjson::Value indexVal;
            indexVal.SetInt(c.tagIndex);
            tagObj.AddMember(rapidjson::Value("tagIndex", alloc).Move(), indexVal, alloc);
            compsObj.AddMember(rapidjson::Value("TagComponent", alloc).Move(), tagObj, alloc);
        }
        if (ecs.HasComponent<LayerComponent>(entity)) {
            auto& c = ecs.GetComponent<LayerComponent>(entity);
            rapidjson::Value layerObj(rapidjson::kObjectType);
            rapidjson::Value indexVal;
            indexVal.SetInt(c.layerIndex);
            layerObj.AddMember(rapidjson::Value("layerIndex", alloc).Move(), indexVal, alloc);
            compsObj.AddMember(rapidjson::Value("LayerComponent", alloc).Move(), layerObj, alloc);
        }
        if (ecs.HasComponent<Transform>(entity)) {
            auto& c = ecs.GetComponent<Transform>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("Transform", v, alloc);
        }
        if (ecs.HasComponent<ModelRenderComponent>(entity)) {
            auto& c = ecs.GetComponent<ModelRenderComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ModelRenderComponent", v, alloc);
        }
        if (ecs.HasComponent<SpriteRenderComponent>(entity)) {
            auto& c = ecs.GetComponent<SpriteRenderComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("SpriteRenderComponent", v, alloc);
        }
        if (ecs.HasComponent<TextRenderComponent>(entity)) {
            auto& c = ecs.GetComponent<TextRenderComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("TextRenderComponent", v, alloc);
        }
        if (ecs.HasComponent<ParticleComponent>(entity)) {
            auto& c = ecs.GetComponent<ParticleComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ParticleComponent", v, alloc);
        }
        //if (ecs.HasComponent<DebugDrawComponent>(entity)) {
        //    auto& c = ecs.GetComponent<DebugDrawComponent>(entity);
        //    rapidjson::Value v = serializeComponentToValue(c);
        //    compsObj.AddMember("DebugDrawComponent", v, alloc);
        //}
        if (ecs.HasComponent<ChildrenComponent>(entity)) {
            auto& c = ecs.GetComponent<ChildrenComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ChildrenComponent", v, alloc);
        }
        if (ecs.HasComponent<ParentComponent>(entity)) {
            auto& c = ecs.GetComponent<ParentComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ParentComponent", v, alloc);
        }

        if (ecs.HasComponent<AudioComponent>(entity)) {
            auto& c = ecs.GetComponent<AudioComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("AudioComponent", v, alloc);
        }
        if (ecs.HasComponent<LightComponent>(entity)) {
            auto& c = ecs.GetComponent<LightComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("LightComponent", v, alloc);
        }
        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
            auto& c = ecs.GetComponent<DirectionalLightComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("DirectionalLightComponent", v, alloc);
        }
        if (ecs.HasComponent<PointLightComponent>(entity)) {
            auto& c = ecs.GetComponent<PointLightComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("PointLightComponent", v, alloc);
        }
        if (ecs.HasComponent<SpotLightComponent>(entity)) {
            auto& c = ecs.GetComponent<SpotLightComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("SpotLightComponent", v, alloc);
        }
        if (ecs.HasComponent<RigidBodyComponent>(entity)) {
            auto& c = ecs.GetComponent<RigidBodyComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("RigidBodyComponent", v, alloc);
        }
        if (ecs.HasComponent<ColliderComponent>(entity)) {
            auto& c = ecs.GetComponent<ColliderComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ColliderComponent", v, alloc);
        }

        entObj.AddMember("components", compsObj, alloc);
        entitiesArr.PushBack(entObj, alloc);
    }

    doc.AddMember("entities", entitiesArr, alloc);

    // Serialize tags
    rapidjson::Value tagsArr(rapidjson::kArrayType);
    const auto& allTags = TagManager::GetInstance().GetAllTags();
    for (const auto& tag : allTags) {
        rapidjson::Value tagVal;
        tagVal.SetString(tag.c_str(), static_cast<rapidjson::SizeType>(tag.size()), alloc);
        tagsArr.PushBack(tagVal, alloc);
    }
    doc.AddMember("tags", tagsArr, alloc);

    // Serialize layers
    rapidjson::Value layersArr(rapidjson::kArrayType);
    const auto& allLayers = LayerManager::GetInstance().GetAllLayers();
    for (int i = 0; i < LayerManager::MAX_LAYERS; ++i) {
        if (!allLayers[i].empty()) {
            rapidjson::Value layerObj(rapidjson::kObjectType);
            rapidjson::Value indexVal(i);
            rapidjson::Value nameVal;
            nameVal.SetString(allLayers[i].c_str(), static_cast<rapidjson::SizeType>(allLayers[i].size()), alloc);
            layerObj.AddMember("index", indexVal, alloc);
            layerObj.AddMember("name", nameVal, alloc);
            layersArr.PushBack(layerObj, alloc);
        }
    }
    doc.AddMember("layers", layersArr, alloc);

    // Write to file (ensure parent directory exists; fallback to current directory if creation fails)
    {
        fs::path outPathP(scenePath);

        // Try to create parent directories if needed
        if (outPathP.has_parent_path()) {
            fs::path parent = outPathP.parent_path();
            std::error_code ec;
            if (!fs::exists(parent)) {
                if (!fs::create_directories(parent, ec)) {
                    std::cerr << "[SaveScene] failed to create directory '" << parent.string()
                        << "': " << ec.message() << "\n"
                        << "[SaveScene] will attempt to write to current working directory instead.\n";
                    // fallback: use filename only
                    outPathP = outPathP.filename();
                }
            }
        }

        // Prepare buffer & writer
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        // Try to open the file (this will create the file if it doesn't exist)
        std::ofstream ofs(outPathP.string(), std::ios::binary | std::ios::trunc);
        if (!ofs) {
            std::cerr << "[SaveScene] failed to open output file: " << outPathP.string() << "\n";
            return;
        }

        ofs << buffer.GetString();
        ofs.close();

        std::cout << "[SaveScene] wrote scene JSON to: " << outPathP.string()
            << " (" << buffer.GetSize() << " bytes)\n";
    }
}

void Serializer::DeserializeScene(const std::string& scenePath) {
    ENGINE_LOG_INFO("[Serializer] Deserializing scene: " + scenePath);
    using namespace std;
    namespace fs = std::filesystem;

    // Use platform abstraction to get asset list (works on Windows, Linux, Android)
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        ENGINE_LOG_DEBUG("[Serializer] ERROR: Platform not available for asset discovery!");
        return;
    }
    if (!platform->FileExists(scenePath)) {
        ENGINE_LOG_DEBUG("[Serializer]: Scene file not found: " + scenePath);
        return;
    }

    std::vector<uint8_t> metaFileData = platform->ReadAsset(scenePath);
    rapidjson::Document doc;
    if (!metaFileData.empty()) {
        rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
        doc.ParseStream(ms);
    }
    if (doc.HasParseError()) {
        ENGINE_LOG_DEBUG("[Serializer]: Rapidjson parse error: " + scenePath);
    }

    if (!doc.IsObject()) {
        return;
    }

    // Deserialize tags
    if (doc.HasMember("tags") && doc["tags"].IsArray()) {
        const auto& tagsArr = doc["tags"];
        for (rapidjson::SizeType i = 0; i < tagsArr.Size(); ++i) {
            std::string tag = tagsArr[i].GetString();
            TagManager::GetInstance().AddTag(tag);
        }
    }

    // Deserialize layers
    if (doc.HasMember("layers") && doc["layers"].IsArray()) {
        const auto& layersArr = doc["layers"];
        for (rapidjson::SizeType i = 0; i < layersArr.Size(); ++i) {
            const auto& layerObj = layersArr[i];
            if (layerObj.IsObject() && layerObj.HasMember("index") && layerObj.HasMember("name")) {
                int index = layerObj["index"].GetInt();
                std::string name = layerObj["name"].GetString();
                LayerManager::GetInstance().SetLayerName(index, name);
            }
        }
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        ENGINE_LOG_WARN("[CreateEntitiesFromJson] no entities array in JSON: " + scenePath);
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetECSManager(scenePath);

    const rapidjson::Value& ents = doc["entities"];
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) continue;

        Entity newEnt = CreateEntityViaGUID(entObj);

        if (!entObj.HasMember("components") || !entObj["components"].IsObject()) continue;
        const rapidjson::Value& comps = entObj["components"];

        // NameComponent
        if (comps.HasMember("NameComponent")) {
            const rapidjson::Value& nv = comps["NameComponent"];
            ecs.AddComponent<NameComponent>(newEnt, NameComponent{});
            auto& nameComp = ecs.GetComponent<NameComponent>(newEnt);
            DeserializeNameComponent(nameComp, nv);
        }

        // TagComponent
        if (comps.HasMember("TagComponent")) {
            const rapidjson::Value& tv = comps["TagComponent"];
            ecs.AddComponent<TagComponent>(newEnt, TagComponent{});
            auto& tagComp = ecs.GetComponent<TagComponent>(newEnt);
            DeserializeTagComponent(tagComp, tv);
        }

        // LayerComponent
        if (comps.HasMember("LayerComponent")) {
            const rapidjson::Value& lv = comps["LayerComponent"];
            ecs.AddComponent<LayerComponent>(newEnt, LayerComponent{});
            auto& layerComp = ecs.GetComponent<LayerComponent>(newEnt);
            DeserializeLayerComponent(layerComp, lv);
        }

        // Transform
        if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
            const rapidjson::Value& t = comps["Transform"];
            ecs.AddComponent<Transform>(newEnt, Transform{});
            DeserializeTransformComponent(newEnt, t);
        }

        // ModelRenderComponent
        if (comps.HasMember("ModelRenderComponent")) {
            const rapidjson::Value& mv = comps["ModelRenderComponent"];
            ecs.AddComponent<ModelRenderComponent>(newEnt, ModelRenderComponent{});
            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(newEnt);
            DeserializeModelComponent(modelComp, mv);
        }

        // SpriteRenderComponent
        if (comps.HasMember("SpriteRenderComponent")) {
            const rapidjson::Value& mv = comps["SpriteRenderComponent"];
            ecs.AddComponent<SpriteRenderComponent>(newEnt, SpriteRenderComponent{});
            auto& spriteComp = ecs.GetComponent<SpriteRenderComponent>(newEnt);
            DeserializeSpriteComponent(spriteComp, mv);
        }

        // TextRenderComponent
        if (comps.HasMember("TextRenderComponent") && comps["TextRenderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["TextRenderComponent"];
            ecs.AddComponent<TextRenderComponent>(newEnt, TextRenderComponent{});
            auto& textComp = ecs.GetComponent<TextRenderComponent>(newEnt);
            DeserializeTextComponent(textComp, tv);
        }

        // ParticleComponent
        if (comps.HasMember("ParticleComponent") && comps["ParticleComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["ParticleComponent"];
            ecs.AddComponent<ParticleComponent>(newEnt, ParticleComponent{});
            auto& particleComp = ecs.GetComponent<ParticleComponent>(newEnt);
            DeserializeParticleComponent(particleComp, tv);
        }

        // DirectionalLightComponent
        if (comps.HasMember("DirectionalLightComponent") && comps["DirectionalLightComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["DirectionalLightComponent"];
            ecs.AddComponent<DirectionalLightComponent>(newEnt, DirectionalLightComponent{});
            auto& dirLightComp = ecs.GetComponent<DirectionalLightComponent>(newEnt);
            DeserializeDirLightComponent(dirLightComp, tv);
        }

        // SpotLightComponent
        if (comps.HasMember("SpotLightComponent") && comps["SpotLightComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["SpotLightComponent"];
            ecs.AddComponent<SpotLightComponent>(newEnt, SpotLightComponent{});
            auto& spotlightComp = ecs.GetComponent<SpotLightComponent>(newEnt);
            DeserializeSpotLightComponent(spotlightComp, tv);
        }

        // PointLightComponent
        if (comps.HasMember("PointLightComponent") && comps["PointLightComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["PointLightComponent"];
            ecs.AddComponent<PointLightComponent>(newEnt, PointLightComponent{});
            auto& pointLightComp = ecs.GetComponent<PointLightComponent>(newEnt);
            DeserializePointLightComponent(pointLightComp, tv);
        }

        // AudioComponent
        if (comps.HasMember("AudioComponent") && comps["AudioComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["AudioComponent"];
            ecs.AddComponent<AudioComponent>(newEnt, AudioComponent{});
            auto& audioComp = ecs.GetComponent<AudioComponent>(newEnt);
            DeserializeAudioComponent(audioComp, tv);
        }

        // RigidBodyComponent
        if (comps.HasMember("RigidBodyComponent") && comps["RigidBodyComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["RigidBodyComponent"];
            ecs.AddComponent<RigidBodyComponent>(newEnt, RigidBodyComponent{});
            auto& rbComp = ecs.GetComponent<RigidBodyComponent>(newEnt);
            DeserializeRigidBodyComponent(rbComp, tv);
        }

        // ColliderComponent
        if (comps.HasMember("ColliderComponent") && comps["ColliderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["ColliderComponent"];
            ecs.AddComponent<ColliderComponent>(newEnt, ColliderComponent{});
            auto& colliderComp = ecs.GetComponent<ColliderComponent>(newEnt);
            DeserializeColliderComponent(colliderComp, tv);
        }

        // ParentComponent
        if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
            const auto& parentCompJSON = comps["ParentComponent"];
            ecs.AddComponent<ParentComponent>(newEnt, ParentComponent{});
            auto& parentComp = ecs.GetComponent<ParentComponent>(newEnt);
            DeserializeParentComponent(parentComp, parentCompJSON);
        }

        // ChildrenComponent
        if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
            const auto& childrenCompJSON = comps["ChildrenComponent"];
            ecs.AddComponent<ChildrenComponent>(newEnt, ChildrenComponent{});
            auto& childComp = ecs.GetComponent<ChildrenComponent>(newEnt);
            DeserializeChildrenComponent(childComp, childrenCompJSON);
        }

        // Ensure all entities have TagComponent and LayerComponent
        if (!ecs.HasComponent<TagComponent>(newEnt)) {
            ecs.AddComponent<TagComponent>(newEnt, TagComponent{0});
        }
        if (!ecs.HasComponent<LayerComponent>(newEnt)) {
            ecs.AddComponent<LayerComponent>(newEnt, LayerComponent{0});
        }

    } // end for entities

    // Deserialize tags
    if (doc.HasMember("tags") && doc["tags"].IsArray()) {
        const auto& tags = doc["tags"];
        for (rapidjson::SizeType i = 0; i < tags.Size(); ++i) {
            std::string tag = tags[i].GetString();
            TagManager::GetInstance().AddTag(tag);
        }
    }

    // Deserialize layers
    if (doc.HasMember("layers") && doc["layers"].IsArray()) {
        const auto& layers = doc["layers"];
        for (rapidjson::SizeType i = 0; i < layers.Size(); ++i) {
            const auto& layerObj = layers[i];
            int index = layerObj["index"].GetInt();
            std::string name = layerObj["name"].GetString();
            LayerManager::GetInstance().SetLayerName(index, name);
        }
    }

    std::cout << "[CreateEntitiesFromJson] loaded entities from: " << scenePath << "\n";
}

void Serializer::ReloadScene(const std::string& tempScenePath, const std::string& currentScenePath) {
    ENGINE_LOG_INFO("[Serializer] Reloading temp scene: " + tempScenePath);
    using namespace std;
    namespace fs = std::filesystem;

    // Use platform abstraction to get asset list (works on Windows, Linux, Android)
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        ENGINE_LOG_DEBUG("[Serializer] ERROR: Platform not available for asset discovery!");
        return;
    }
    if (!platform->FileExists(tempScenePath)) {
        ENGINE_LOG_DEBUG("[Serializer]: Scene file not found: " + tempScenePath);
        return;
    }

    std::vector<uint8_t> metaFileData = platform->ReadAsset(tempScenePath);
    rapidjson::Document doc;
    if (!metaFileData.empty()) {
        rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
        doc.ParseStream(ms);
    }
    if (doc.HasParseError()) {
        ENGINE_LOG_DEBUG("[Serializer]: Rapidjson parse error: " + tempScenePath);
    }

    // Deserialize tags
    if (doc.HasMember("tags") && doc["tags"].IsArray()) {
        const auto& tagsArr = doc["tags"];
        for (rapidjson::SizeType i = 0; i < tagsArr.Size(); ++i) {
            std::string tag = tagsArr[i].GetString();
            TagManager::GetInstance().AddTag(tag);
        }
    }

    // Deserialize layers
    if (doc.HasMember("layers") && doc["layers"].IsArray()) {
        const auto& layersArr = doc["layers"];
        for (rapidjson::SizeType i = 0; i < layersArr.Size(); ++i) {
            const auto& layerObj = layersArr[i];
            if (layerObj.IsObject() && layerObj.HasMember("index") && layerObj.HasMember("name")) {
                int index = layerObj["index"].GetInt();
                std::string name = layerObj["name"].GetString();
                LayerManager::GetInstance().SetLayerName(index, name);
            }
        }
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        ENGINE_LOG_WARN("[CreateEntitiesFromJson] no entities array in JSON: " + tempScenePath);
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetECSManager(currentScenePath);

    const rapidjson::Value& ents = doc["entities"];
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) continue;

        Entity currEnt = entObj["id"].GetUint();

        if (!entObj.HasMember("components") || !entObj["components"].IsObject()) continue;
        const rapidjson::Value& comps = entObj["components"];

        // NameComponent
        if (comps.HasMember("NameComponent")) {
            const rapidjson::Value& nv = comps["NameComponent"];
            auto& nameComp = ecs.GetComponent<NameComponent>(currEnt);
            DeserializeNameComponent(nameComp, nv);
        }

        // TagComponent
        if (comps.HasMember("TagComponent")) {
            const rapidjson::Value& tv = comps["TagComponent"];
            auto& tagComp = ecs.GetComponent<TagComponent>(currEnt);
            DeserializeTagComponent(tagComp, tv);
        }

        // LayerComponent
        if (comps.HasMember("LayerComponent")) {
            const rapidjson::Value& lv = comps["LayerComponent"];
            auto& layerComp = ecs.GetComponent<LayerComponent>(currEnt);
            DeserializeLayerComponent(layerComp, lv);
        }

        // Transform
        if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
            const rapidjson::Value& t = comps["Transform"];
            DeserializeTransformComponent(currEnt, t);
        }

        // ModelRenderComponent
        if (comps.HasMember("ModelRenderComponent")) {
            const rapidjson::Value& mv = comps["ModelRenderComponent"];
            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(currEnt);
            DeserializeModelComponent(modelComp, mv);
        }

        // SpriteRenderComponent
        if (comps.HasMember("SpriteRenderComponent")) {
            const rapidjson::Value& mv = comps["SpriteRenderComponent"];
            auto& spriteComp = ecs.GetComponent<SpriteRenderComponent>(currEnt);
            DeserializeSpriteComponent(spriteComp, mv);
        }

        // TextRenderComponent
        if (comps.HasMember("TextRenderComponent") && comps["TextRenderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["TextRenderComponent"];
            auto& textComp = ecs.GetComponent<TextRenderComponent>(currEnt);
            DeserializeTextComponent(textComp, tv);
        }

        // ParticleComponent
        if (comps.HasMember("ParticleComponent") && comps["ParticleComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["ParticleComponent"];
            auto& particleComp = ecs.GetComponent<ParticleComponent>(currEnt);
            DeserializeParticleComponent(particleComp, tv);
        }

        // DirectionalLightComponent
        if (comps.HasMember("DirectionalLightComponent") && comps["DirectionalLightComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["DirectionalLightComponent"];
            auto& dirLightComp = ecs.GetComponent<DirectionalLightComponent>(currEnt);
            DeserializeDirLightComponent(dirLightComp, tv);
        }

        // SpotLightComponent
        if (comps.HasMember("SpotLightComponent") && comps["SpotLightComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["SpotLightComponent"];
            auto& spotlightComp = ecs.GetComponent<SpotLightComponent>(currEnt);
            DeserializeSpotLightComponent(spotlightComp, tv);
        }

        // PointLightComponent
        if (comps.HasMember("PointLightComponent") && comps["PointLightComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["PointLightComponent"];
            auto& pointLightComp = ecs.GetComponent<PointLightComponent>(currEnt);
            DeserializePointLightComponent(pointLightComp, tv);
        }

        // AudioComponent
        if (comps.HasMember("AudioComponent") && comps["AudioComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["AudioComponent"];
            auto& audioComp = ecs.GetComponent<AudioComponent>(currEnt);
            DeserializeAudioComponent(audioComp, tv);
        }

        // RigidBodyComponent
        if (comps.HasMember("RigidBodyComponent") && comps["RigidBodyComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["RigidBodyComponent"];
            auto& rbComp = ecs.GetComponent<RigidBodyComponent>(currEnt);
            DeserializeRigidBodyComponent(rbComp, tv);
        }

        // ColliderComponent
        if (comps.HasMember("ColliderComponent") && comps["ColliderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["ColliderComponent"];
            auto& colliderComp = ecs.GetComponent<ColliderComponent>(currEnt);
            DeserializeColliderComponent(colliderComp, tv);
        }

        // ParentComponent
        if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
            const auto& parentCompJSON = comps["ParentComponent"];
            auto& parentComp = ecs.GetComponent<ParentComponent>(currEnt);
            DeserializeParentComponent(parentComp, parentCompJSON);
        }

        // ChildrenComponent
        if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
            const auto& childrenCompJSON = comps["ChildrenComponent"];
            auto& childComp = ecs.GetComponent<ChildrenComponent>(currEnt);
            DeserializeChildrenComponent(childComp, childrenCompJSON);
        }

        // Ensure all entities have TagComponent and LayerComponent
        if (!ecs.HasComponent<TagComponent>(currEnt)) {
            ecs.AddComponent<TagComponent>(currEnt, TagComponent{0});
        }
        if (!ecs.HasComponent<LayerComponent>(currEnt)) {
            ecs.AddComponent<LayerComponent>(currEnt, LayerComponent{0});
        }

    } // end for entities

    // Deserialize tags
    if (doc.HasMember("tags") && doc["tags"].IsArray()) {
        const auto& tags = doc["tags"];
        for (rapidjson::SizeType i = 0; i < tags.Size(); ++i) {
            std::string tag = tags[i].GetString();
            TagManager::GetInstance().AddTag(tag);
        }
    }

    // Deserialize layers
    if (doc.HasMember("layers") && doc["layers"].IsArray()) {
        const auto& layers = doc["layers"];
        for (rapidjson::SizeType i = 0; i < layers.Size(); ++i) {
            const auto& layerObj = layers[i];
            int index = layerObj["index"].GetInt();
            std::string name = layerObj["name"].GetString();
            LayerManager::GetInstance().SetLayerName(index, name);
        }
    }

    std::cout << "[CreateEntitiesFromJson] loaded entities from: " << tempScenePath << "\n";
}

Entity Serializer::CreateEntityViaGUID(const rapidjson::Value& entityJSON) {
    if (entityJSON.HasMember("guid")) {
        GUID_string guidStr = entityJSON["guid"].GetString();
        GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
        Entity newEnt = ECSRegistry::GetInstance().GetActiveECSManager().CreateEntityWithGUID(guid);
        ENGINE_LOG_INFO("Entity " + std::to_string(newEnt) + " created with GUID " + guidStr);
        return newEnt;
    }
    else {
        // Fallback for if there is no GUID, but it shouldn't happen.
        Entity newEnt = ECSRegistry::GetInstance().GetActiveECSManager().CreateEntity();
        ENGINE_LOG_WARN("Entity created with no GUID!");
        return newEnt;
    }
}

void Serializer::DeserializeNameComponent(NameComponent& nameComp, const rapidjson::Value& nameJSON) {
    std::string name;
    if (nameJSON.IsObject() && nameJSON.HasMember("name") && nameJSON["name"].IsString()) name = nameJSON["name"].GetString();
    else if (nameJSON.IsString()) name = nameJSON.GetString();
    else if (nameJSON.IsObject() && nameJSON.HasMember("data")) getStringFromValue(nameJSON["data"], name);

    if (!name.empty()) {
        nameComp.name = name;
    }
}

void Serializer::DeserializeTransformComponent(Entity newEnt, const rapidjson::Value& t) {
    Vector3D pos{ 0.f,0.f,0.f }, rot{ 0.f,0.f,0.f }, scale{ 1.f,1.f,1.f };
    // legacy simple arrays
    if (t.HasMember("position") && t["position"].IsArray()) readVec3FromArray(t["position"], pos);
    else if (t.HasMember("localPosition") && t["localPosition"].IsArray()) readVec3FromArray(t["localPosition"], pos);

    if (t.HasMember("rotation") && t["rotation"].IsArray()) readVec3FromArray(t["rotation"], rot);
    else if (t.HasMember("localRotation") && t["localRotation"].IsArray()) readVec3FromArray(t["localRotation"], rot);

    if (t.HasMember("scale") && t["scale"].IsArray()) readVec3FromArray(t["scale"], scale);
    else if (t.HasMember("localScale") && t["localScale"].IsArray()) readVec3FromArray(t["localScale"], scale);

    // typed Transform (your format): data is an array; observed order in your file is: position, scale, rotation(quaternion), ...
    if (t.HasMember("data") && t["data"].IsArray()) {
        const auto& darr = t["data"];
        if (darr.Size() > 0) {
            readVec3Generic(darr[0], pos); // index 0 -> position
        }
        if (darr.Size() > 1) {
            Vector3D maybeScale{ 1.f,1.f,1.f };
            if (readVec3Generic(darr[1], maybeScale)) scale = maybeScale; // index 1 -> scale
        }
        if (darr.Size() > 2) {
            double w = 1, x = 0, y = 0, z = 0;
            if (readQuatGeneric(darr[2], w, x, y, z)) { // index 2 -> quaternion
                rot = quatToEulerDeg(w, x, y, z);
            }
        }
    }

    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs.transformSystem) {
        ecs.transformSystem->SetLocalPosition(newEnt, pos);
        ecs.transformSystem->SetLocalRotation(newEnt, rot); // expects Euler degrees in your code
        ecs.transformSystem->SetLocalScale(newEnt, scale);
    }
    else
    {
        Transform tf; tf.localPosition = pos; tf.localRotation = Quaternion::FromEulerDegrees(rot); tf.localScale = scale;
    }
}

void Serializer::DeserializeModelComponent(ModelRenderComponent& modelComp, const rapidjson::Value& modelJSON) {
    if (modelJSON.IsObject()) {
        if (modelJSON.HasMember("data") && modelJSON["data"].IsArray() && modelJSON["data"].Size() > 0) {
            GUID_string modelGUIDStr = modelJSON["data"][0].GetString();
            GUID_string shaderGUIDStr = modelJSON["data"][1].GetString();
            GUID_string materialGUIDStr = modelJSON["data"][2].GetString();
            modelComp.modelGUID = GUIDUtilities::ConvertStringToGUID128(modelGUIDStr);
            modelComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(shaderGUIDStr);
            modelComp.materialGUID = GUIDUtilities::ConvertStringToGUID128(materialGUIDStr);
        }
    }
}

void Serializer::DeserializeSpriteComponent(SpriteRenderComponent& spriteComp, const rapidjson::Value& spriteJSON) {
    if (spriteJSON.IsObject()) {
        if (spriteJSON.HasMember("data") && spriteJSON["data"].IsArray() && spriteJSON["data"].Size() > 0) {
            GUID_string textureGUIDStr = spriteJSON["data"][0].GetString();
            GUID_string shaderGUIDStr = spriteJSON["data"][1].GetString();
            spriteComp.textureGUID = GUIDUtilities::ConvertStringToGUID128(textureGUIDStr);
            spriteComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(shaderGUIDStr);

            // Sprite position
            readVec3Generic(spriteJSON["data"][2], spriteComp.position);
            // Sprite scale
            readVec3Generic(spriteJSON["data"][3], spriteComp.scale);
            // Sprite rotation
            spriteComp.rotation = spriteJSON["data"][4]["data"].GetFloat();
            // Sprite color
            readVec3Generic(spriteJSON["data"][5], spriteComp.color);
            spriteComp.alpha = spriteJSON["data"][6]["data"].GetFloat();
            spriteComp.is3D = spriteJSON["data"][7]["data"].GetBool();
            spriteComp.enableBillboard = spriteJSON["data"][8]["data"].GetBool();
            spriteComp.layer = spriteJSON["data"][9]["data"].GetInt();
            readVec3Generic(spriteJSON["data"][10], spriteComp.saved3DPosition);
        }
    }
}

void Serializer::DeserializeTextComponent(TextRenderComponent& textComp, const rapidjson::Value& textJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (textJSON.HasMember("data") && textJSON["data"].IsArray()) {
        const auto& d = textJSON["data"];
        textComp.text = d[0]["data"].GetString();
        textComp.fontSize = d[1]["data"].GetUint();
        textComp.fontGUID = GUIDUtilities::ConvertStringToGUID128(d[2].GetString());
        textComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(d[3].GetString());
        readVec3Generic(d[4], textComp.position);
        readVec3Generic(d[5], textComp.color);
        textComp.scale = d[6]["data"].GetFloat();
        textComp.is3D = d[7]["data"].GetBool();
        textComp.alignment = static_cast<TextRenderComponent::Alignment>(d[9]["data"].GetInt());
    }
}

void Serializer::DeserializeParticleComponent(ParticleComponent& particleComp, const rapidjson::Value& particleJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (particleJSON.HasMember("data") && particleJSON["data"].IsArray()) {
        const auto& d = particleJSON["data"];
        GUID_string guidStr = d[0].GetString();
        particleComp.textureGUID = GUIDUtilities::ConvertStringToGUID128(guidStr);
        readVec3Generic(d[1], particleComp.emitterPosition);
        particleComp.emissionRate = d[2]["data"].GetFloat();
        particleComp.maxParticles = d[3]["data"].GetInt();
        particleComp.particleLifetime = d[4]["data"].GetFloat();
        particleComp.startSize = d[5]["data"].GetFloat();
        particleComp.endSize = d[6]["data"].GetFloat();
        readVec3Generic(d[7], particleComp.startColor);
        particleComp.startColorAlpha = d[8]["data"].GetFloat();
        readVec3Generic(d[9], particleComp.endColor);
        particleComp.endColorAlpha = d[10]["data"].GetFloat();
        readVec3Generic(d[11], particleComp.gravity);
        particleComp.velocityRandomness = d[12]["data"].GetFloat();
        readVec3Generic(d[13], particleComp.initialVelocity);
    }
}

void Serializer::DeserializeDirLightComponent(DirectionalLightComponent& dirLightComp, const rapidjson::Value& dirLightJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (dirLightJSON.HasMember("data") && dirLightJSON["data"].IsArray()) {
        const auto& d = dirLightJSON["data"];
        readVec3Generic(d[0], dirLightComp.color);
        dirLightComp.intensity = d[1]["data"].GetFloat();
        dirLightComp.enabled = d[2]["data"].GetBool();
        readVec3Generic(d[3], dirLightComp.direction);
        readVec3Generic(d[4], dirLightComp.ambient);
        readVec3Generic(d[5], dirLightComp.diffuse);
        readVec3Generic(d[6], dirLightComp.specular);
    }
}

void Serializer::DeserializeSpotLightComponent(SpotLightComponent& spotlightComp, const rapidjson::Value& spotLightJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (spotLightJSON.HasMember("data") && spotLightJSON["data"].IsArray()) {
        const auto& d = spotLightJSON["data"];
        readVec3Generic(d[0], spotlightComp.color);
        spotlightComp.intensity = d[1]["data"].GetFloat();
        spotlightComp.enabled = d[2]["data"].GetBool();
        readVec3Generic(d[3], spotlightComp.direction);
        spotlightComp.cutOff = d[4]["data"].GetFloat();
        spotlightComp.constant = d[5]["data"].GetFloat();
        spotlightComp.linear = d[6]["data"].GetFloat();
        spotlightComp.quadratic = d[7]["data"].GetFloat();
        readVec3Generic(d[8], spotlightComp.ambient);
        readVec3Generic(d[9], spotlightComp.diffuse);
        readVec3Generic(d[10], spotlightComp.specular);
    }
}

void Serializer::DeserializePointLightComponent(PointLightComponent& pointLightComp, const rapidjson::Value& pointLightJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (pointLightJSON.HasMember("data") && pointLightJSON["data"].IsArray()) {
        const auto& d = pointLightJSON["data"];
        readVec3Generic(d[0], pointLightComp.color);
        pointLightComp.intensity = d[1]["data"].GetFloat();
        pointLightComp.enabled = d[2]["data"].GetBool();
        pointLightComp.constant = d[3]["data"].GetFloat();
        pointLightComp.linear = d[4]["data"].GetFloat();
        pointLightComp.quadratic = d[5]["data"].GetFloat();
        readVec3Generic(d[6], pointLightComp.ambient);
        readVec3Generic(d[7], pointLightComp.diffuse);
        readVec3Generic(d[8], pointLightComp.specular);
    }
}

void Serializer::DeserializeAudioComponent(AudioComponent& audioComp, const rapidjson::Value& audioJSON) {
    // typed form: tv.data = [ {type: "bool", data: true}, "GUID_string", {type: "std::string", data: "clip"}, ... ]
    if (audioJSON.HasMember("data") && audioJSON["data"].IsArray()) {
        const auto& d = audioJSON["data"];
        audioComp.enabled = d[0]["data"].GetBool();  // d[0] is the enabled object
        GUID_string guidStr = d[1].GetString();  // d[1] is the GUID string
        audioComp.audioGUID = GUIDUtilities::ConvertStringToGUID128(guidStr);
		audioComp.Mute = d[2]["data"].GetBool();
		audioComp.bypassListenerEffects = d[3]["data"].GetBool();
		audioComp.PlayOnAwake = d[4]["data"].GetBool();
		audioComp.Loop = d[5]["data"].GetBool();
		audioComp.Priority = d[6]["data"].GetInt();
		audioComp.Volume = d[7]["data"].GetFloat();
		audioComp.Pitch = d[8]["data"].GetFloat();
		audioComp.StereoPan = d[9]["data"].GetFloat();
		audioComp.reverbZoneMix = d[10]["data"].GetFloat();
		audioComp.Spatialize = d[11]["data"].GetBool();
		audioComp.SpatialBlend = d[12]["data"].GetFloat();
		audioComp.DopplerLevel = d[13]["data"].GetFloat();
		audioComp.MinDistance = d[14]["data"].GetFloat();
		audioComp.MaxDistance = d[15]["data"].GetFloat();
    }
}

void Serializer::DeserializeRigidBodyComponent(RigidBodyComponent& rbComp, const rapidjson::Value& rbJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (rbJSON.HasMember("data") && rbJSON["data"].IsArray()) {
        const auto& d = rbJSON["data"];
        rbComp.motionID = d[0]["data"].GetInt();
        rbComp.motion = static_cast<Motion>(rbComp.motionID);
        rbComp.ccd = d[1]["data"].GetBool();
        rbComp.transform_dirty = true;
        rbComp.motion_dirty = true;
        rbComp.collider_seen_version = 0;
    }
}

void Serializer::DeserializeColliderComponent(ColliderComponent& colliderComp, const rapidjson::Value& colliderJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (colliderJSON.HasMember("data") && colliderJSON["data"].IsArray()) {
        const auto& d = colliderJSON["data"];
        colliderComp.layerID = d[0]["data"].GetInt();
        colliderComp.layer = static_cast<JPH::ObjectLayer>(colliderComp.layerID);
        colliderComp.version = d[1]["data"].GetUint();
        colliderComp.shapeTypeID = d[2]["data"].GetInt();
        colliderComp.shapeType = static_cast<ColliderShapeType>(colliderComp.shapeTypeID);
        readVec3Generic(d[3], colliderComp.boxHalfExtents);
        switch (colliderComp.shapeType)
        {
        case ColliderShapeType::Box:
            colliderComp.shape = new JPH::BoxShape((JPH::Vec3(colliderComp.boxHalfExtents.x, colliderComp.boxHalfExtents.y, colliderComp.boxHalfExtents.z)));
            break;
        default:
            break;
        }
    }
}

void Serializer::DeserializeParentComponent(ParentComponent& parentComp, const rapidjson::Value& parentJSON) {
    GUID_string parentGUIDStr = parentJSON["data"][0].GetString();
    parentComp.parent = GUIDUtilities::ConvertStringToGUID128(parentGUIDStr);
}

void Serializer::DeserializeChildrenComponent(ChildrenComponent& childComp, const rapidjson::Value& childJSON) {
    if (childJSON.HasMember("data")) {
        childComp.children.clear();
        const auto& childrenVectorJSON = childJSON["data"][0]["data"].GetArray();
        for (const auto& childJSON : childrenVectorJSON) {
            GUID_string childGUIDStr = childJSON.GetString();
            childComp.children.push_back(GUIDUtilities::ConvertStringToGUID128(childGUIDStr));
        }
    }
}

void Serializer::DeserializeTagComponent(TagComponent& tagComp, const rapidjson::Value& tagJSON) {
    if (tagJSON.HasMember("tagIndex") && tagJSON["tagIndex"].IsInt()) {
        tagComp.tagIndex = tagJSON["tagIndex"].GetInt();
    }
}

void Serializer::DeserializeLayerComponent(LayerComponent& layerComp, const rapidjson::Value& layerJSON) {
    if (layerJSON.HasMember("layerIndex") && layerJSON["layerIndex"].IsInt()) {
        layerComp.layerIndex = layerJSON["layerIndex"].GetInt();
    }
}
