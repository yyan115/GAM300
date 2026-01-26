#include "pch.h"
#include "Serialization/Serializer.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "ECS/ECSRegistry.hpp"
#include "Utilities/GUID.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Graphics/Lights/LightingSystem.hpp"
#include <algorithm>
#include <Graphics/Model/ModelFactory.hpp>
#include <Prefab/PrefabIO.hpp>

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

// Helper function to extract GUID string from various JSON formats
auto extractGUIDString = [](const rapidjson::Value& v) -> std::string {
    // Direct string format
    if (v.IsString()) {
        return v.GetString();
    }

    // Object with "data" field
    if (v.IsObject() && v.HasMember("data")) {
        const auto& dataField = v["data"];

        // data is a direct string
        if (dataField.IsString()) {
            return dataField.GetString();
        }

        // data is another object with "data" field (double-wrapped)
        if (dataField.IsObject() && dataField.HasMember("data") && dataField["data"].IsString()) {
            return dataField["data"].GetString();
        }
    }

    // Default empty GUID - must be 33 characters (16 hex + hyphen + 16 hex)
    return "0000000000000000-0000000000000000";  // 33 characters total
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

    //auto& guidRegistry = EntityGUIDRegistry::GetInstance();

	// Iterate entities recursively, starting from root entities (those without parents)
    for (auto entity : ecs.GetAllRootEntities()) {
		SerializeEntityRecursively(entity, alloc, entitiesArr);
        //auto entObj = SerializeEntity(entity, alloc);
        //entitiesArr.PushBack(entObj, alloc);
    }

    doc.AddMember("entities", entitiesArr, alloc);

    // NOTE: Tags, layers, and sorting layers are no longer saved per-scene.
    // They are now saved in project-wide settings (TagsLayersSettings.hpp)
    // This ensures consistency across all scenes in the project.

    // Serialize layers (kept for backward compatibility but will be ignored on load)
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

    // Serialize LightingSystem properties (scene-level lighting settings)
    if (ecs.lightingSystem) {
        rapidjson::Value lightingObj(rapidjson::kObjectType);

        // Ambient mode (enum as int)
        lightingObj.AddMember("ambientMode", static_cast<int>(ecs.lightingSystem->ambientMode), alloc);

        // Ambient sky color
        rapidjson::Value ambientSkyArr(rapidjson::kArrayType);
        ambientSkyArr.PushBack(ecs.lightingSystem->ambientSky.x, alloc);
        ambientSkyArr.PushBack(ecs.lightingSystem->ambientSky.y, alloc);
        ambientSkyArr.PushBack(ecs.lightingSystem->ambientSky.z, alloc);
        lightingObj.AddMember("ambientSky", ambientSkyArr, alloc);

        // Ambient equator color
        rapidjson::Value ambientEquatorArr(rapidjson::kArrayType);
        ambientEquatorArr.PushBack(ecs.lightingSystem->ambientEquator.x, alloc);
        ambientEquatorArr.PushBack(ecs.lightingSystem->ambientEquator.y, alloc);
        ambientEquatorArr.PushBack(ecs.lightingSystem->ambientEquator.z, alloc);
        lightingObj.AddMember("ambientEquator", ambientEquatorArr, alloc);

        // Ambient ground color
        rapidjson::Value ambientGroundArr(rapidjson::kArrayType);
        ambientGroundArr.PushBack(ecs.lightingSystem->ambientGround.x, alloc);
        ambientGroundArr.PushBack(ecs.lightingSystem->ambientGround.y, alloc);
        ambientGroundArr.PushBack(ecs.lightingSystem->ambientGround.z, alloc);
        lightingObj.AddMember("ambientGround", ambientGroundArr, alloc);

        doc.AddMember("lightingSystem", lightingObj, alloc);
    }

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

rapidjson::Value Serializer::SerializeEntityGUID(Entity entity, rapidjson::Document::AllocatorType& alloc) {
    auto& guidRegistry = EntityGUIDRegistry::GetInstance();
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    rapidjson::Value entObj(rapidjson::kObjectType);

    entObj = SerializeEntityGUID(entity, alloc, entObj);

    return entObj;
}

rapidjson::Value& Serializer::SerializeEntityGUID(Entity entity, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& entObj) {
    auto& guidRegistry = EntityGUIDRegistry::GetInstance();
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

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

    return entObj;
}

void Serializer::SerializeEntityRecursively(Entity entity, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& entitiesArr) {
	auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    Entity prefabReferenceEntity = static_cast<Entity>(-1);

    // Check whether this entity is a prefab root.
    // If it is, then when we recursively serialize this entity and its children,
	// we should only serialize components that have been modified from the prefab defaults.
    // This is so that when we load the prefab instance, we can apply the overrides.
    if (ecs.HasComponent<PrefabLinkComponent>(entity)) {
        // This is a Prefab Instance!
        std::string path = ecs.GetComponent<PrefabLinkComponent>(entity).prefabPath;

        // Load the Original Prefab
        Entity baselineRoot = InstantiatePrefabFromFile(path);

        // Calculate Diffs
        rapidjson::Value prefabNode(rapidjson::kObjectType);
        rapidjson::Value pathVal;
        pathVal.SetString(path.c_str(), static_cast<rapidjson::SizeType>(path.length()), alloc);
        prefabNode.AddMember("PrefabPath", pathVal, alloc);

  //      // Always save Root Transform (Position/Rot) as it's always an override
		//rapidjson::Value overridesArray(rapidjson::kArrayType);
  //      if (ecs.HasComponent<NameComponent>(entity)) {
  //          auto& c = ecs.GetComponent<NameComponent>(entity);
  //          rapidjson::Value valInst = SerializeComponentToValue(c, alloc);

  //          rapidjson::Value wrapper(rapidjson::kObjectType);
  //          wrapper.AddMember(rapidjson::StringRef("NameComponent"), valInst, alloc);
  //          overridesArray.PushBack(wrapper, alloc);
  //      }
  //      if (ecs.HasComponent<Transform>(entity)) {
  //          auto& c = ecs.GetComponent<Transform>(entity);
  //          rapidjson::Value valInst = SerializeComponentToValue(c, alloc);

  //          rapidjson::Value wrapper(rapidjson::kObjectType);
  //          wrapper.AddMember(rapidjson::StringRef("Transform"), valInst, alloc);
  //          overridesArray.PushBack(wrapper, alloc);
  //      }

        // D. Save recursive overrides
        SerializePrefabOverridesRecursive(ecs, entity, baselineRoot, alloc, prefabNode);

		ecs.DestroyEntity(baselineRoot); // clean up baseline prefab entity

        entitiesArr.PushBack(prefabNode, alloc);
    }
    else {
        auto entObj = SerializeEntity(entity, alloc);
        entitiesArr.PushBack(entObj, alloc);

	    // If entity has children, serialize them recursively
        if (ecs.HasComponent<ChildrenComponent>(entity)) {
		    auto& childComp = ecs.GetComponent<ChildrenComponent>(entity);
            for (const auto& childGUID : childComp.children) {
			    Entity childEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);
                SerializeEntityRecursively(childEntity, alloc, entitiesArr);
            }
        }
    }
}

rapidjson::Value Serializer::SerializeEntity(Entity entity, rapidjson::Document::AllocatorType& alloc, Entity prefabReferenceEntity) {
    auto& guidRegistry = EntityGUIDRegistry::GetInstance();
	auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    rapidjson::Value entObj = SerializeEntityGUID(entity, alloc);

    rapidjson::Value compsObj(rapidjson::kObjectType);

	// If this entity is part of a prefab instance, we should only serialize components that differ from the prefab defaults.
    bool isPrefabInstance = prefabReferenceEntity != static_cast<Entity>(-1);

    // For each component type, if entity has it, serialize and attach under its name
    if (ecs.HasComponent<PrefabLinkComponent>(entity)) {
        auto& c = ecs.GetComponent<PrefabLinkComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("PrefabLinkComponent", v, alloc);
    }

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
    if (ecs.HasComponent<SiblingIndexComponent>(entity)) {
        auto& c = ecs.GetComponent<SiblingIndexComponent>(entity);
        rapidjson::Value siblingObj(rapidjson::kObjectType);
        rapidjson::Value indexVal;
        indexVal.SetInt(c.siblingIndex);
        siblingObj.AddMember(rapidjson::Value("siblingIndex", alloc).Move(), indexVal, alloc);
        compsObj.AddMember(rapidjson::Value("SiblingIndexComponent", alloc).Move(), siblingObj, alloc);
    }
    if (ecs.HasComponent<Transform>(entity)) {
        auto& c = ecs.GetComponent<Transform>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("Transform", v, alloc);
    }
    if (ecs.HasComponent<ModelRenderComponent>(entity)) {
        auto& c = ecs.GetComponent<ModelRenderComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ModelRenderComponent", v, alloc);
    }
    if (ecs.HasComponent<SpriteRenderComponent>(entity)) {
        auto& c = ecs.GetComponent<SpriteRenderComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("SpriteRenderComponent", v, alloc);
    }
    if (ecs.HasComponent<SpriteAnimationComponent>(entity)) {
        auto& c = ecs.GetComponent<SpriteAnimationComponent>(entity);

        // Custom serialization to include UV coordinates
        rapidjson::Value animValue(rapidjson::kObjectType);

        // Serialize clips array
        rapidjson::Value clipsArray(rapidjson::kArrayType);
        for (const auto& clip : c.clips) {
            rapidjson::Value clipObj(rapidjson::kObjectType);
            clipObj.AddMember("name", rapidjson::Value(clip.name.c_str(), alloc), alloc);
            clipObj.AddMember("loop", clip.loop, alloc);

            // Serialize frames with UV coordinates
            rapidjson::Value framesArray(rapidjson::kArrayType);
            for (const auto& frame : clip.frames) {
                rapidjson::Value frameObj(rapidjson::kObjectType);

                // Texture GUID
                std::string guidStr = GUIDUtilities::ConvertGUID128ToString(frame.textureGUID);
                frameObj.AddMember("textureGUID", rapidjson::Value(guidStr.c_str(), alloc), alloc);
                frameObj.AddMember("texturePath", rapidjson::Value(frame.texturePath.c_str(), alloc), alloc);

                // UV coordinates - MANUALLY SERIALIZE
                rapidjson::Value uvOffsetArray(rapidjson::kArrayType);
                uvOffsetArray.PushBack(frame.uvOffset.x, alloc);
                uvOffsetArray.PushBack(frame.uvOffset.y, alloc);
                frameObj.AddMember("uvOffset", uvOffsetArray, alloc);

                rapidjson::Value uvScaleArray(rapidjson::kArrayType);
                uvScaleArray.PushBack(frame.uvScale.x, alloc);
                uvScaleArray.PushBack(frame.uvScale.y, alloc);
                frameObj.AddMember("uvScale", uvScaleArray, alloc);

                frameObj.AddMember("duration", frame.duration, alloc);
                framesArray.PushBack(frameObj, alloc);
            }
            clipObj.AddMember("frames", framesArray, alloc);
            clipsArray.PushBack(clipObj, alloc);
        }
        animValue.AddMember("clips", clipsArray, alloc);

        // Other fields
        animValue.AddMember("currentClipIndex", c.currentClipIndex, alloc);
        animValue.AddMember("currentFrameIndex", c.currentFrameIndex, alloc);
        animValue.AddMember("timeInCurrentFrame", c.timeInCurrentFrame, alloc);
        animValue.AddMember("playbackSpeed", c.playbackSpeed, alloc);
        animValue.AddMember("playing", c.playing, alloc);
        animValue.AddMember("enabled", c.enabled, alloc);
        animValue.AddMember("autoPlay", c.autoPlay, alloc);

        compsObj.AddMember("SpriteAnimationComponent", animValue, alloc);
    }
    if (ecs.HasComponent<TextRenderComponent>(entity)) {
        auto& c = ecs.GetComponent<TextRenderComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("TextRenderComponent", v, alloc);
    }
    if (ecs.HasComponent<ParticleComponent>(entity)) {
        auto& c = ecs.GetComponent<ParticleComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ParticleComponent", v, alloc);
    }
    //if (ecs.HasComponent<DebugDrawComponent>(entity)) {
    //    auto& c = ecs.GetComponent<DebugDrawComponent>(entity);
    //    rapidjson::Value v = SerializeComponentToValue(c);
    //    compsObj.AddMember("DebugDrawComponent", v, alloc);
    //}
    if (ecs.HasComponent<ChildrenComponent>(entity)) {
        auto& c = ecs.GetComponent<ChildrenComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ChildrenComponent", v, alloc);
    }
    if (ecs.HasComponent<ParentComponent>(entity)) {
        auto& c = ecs.GetComponent<ParentComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ParentComponent", v, alloc);
    }

    if (ecs.HasComponent<AudioComponent>(entity)) {
        auto& c = ecs.GetComponent<AudioComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("AudioComponent", v, alloc);
    }
    if (ecs.HasComponent<AudioListenerComponent>(entity)) {
        auto& c = ecs.GetComponent<AudioListenerComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("AudioListenerComponent", v, alloc);
    }
    if (ecs.HasComponent<AudioReverbZoneComponent>(entity)) {
        auto& c = ecs.GetComponent<AudioReverbZoneComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("AudioReverbZoneComponent", v, alloc);
    }
    if (ecs.HasComponent<LightComponent>(entity)) {
        auto& c = ecs.GetComponent<LightComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("LightComponent", v, alloc);
    }
    if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
        auto& c = ecs.GetComponent<DirectionalLightComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("DirectionalLightComponent", v, alloc);
    }
    if (ecs.HasComponent<PointLightComponent>(entity)) {
        auto& c = ecs.GetComponent<PointLightComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("PointLightComponent", v, alloc);
    }
    if (ecs.HasComponent<SpotLightComponent>(entity)) {
        auto& c = ecs.GetComponent<SpotLightComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("SpotLightComponent", v, alloc);
    }
    if (ecs.HasComponent<RigidBodyComponent>(entity)) {
        auto& c = ecs.GetComponent<RigidBodyComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("RigidBodyComponent", v, alloc);
    }
    if (ecs.HasComponent<ColliderComponent>(entity)) {
        auto& c = ecs.GetComponent<ColliderComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ColliderComponent", v, alloc);
    }
    if (ecs.HasComponent<CameraComponent>(entity)) {
        auto& c = ecs.GetComponent<CameraComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);

        // Add custom serialization for target and up (glm::vec3)
        rapidjson::Value targetVal(rapidjson::kObjectType);
        targetVal.AddMember("type", "glm::vec3", alloc);
        rapidjson::Value targetData(rapidjson::kArrayType);
        targetData.PushBack(c.target.x, alloc);
        targetData.PushBack(c.target.y, alloc);
        targetData.PushBack(c.target.z, alloc);
        targetVal.AddMember("data", targetData, alloc);
        v.AddMember("target", targetVal, alloc);

        rapidjson::Value upVal(rapidjson::kObjectType);
        upVal.AddMember("type", "glm::vec3", alloc);
        rapidjson::Value upData(rapidjson::kArrayType);
        upData.PushBack(c.up.x, alloc);
        upData.PushBack(c.up.y, alloc);
        upData.PushBack(c.up.z, alloc);
        upVal.AddMember("data", upData, alloc);
        v.AddMember("up", upVal, alloc);

        // Add custom serialization for backgroundColor (glm::vec3)
        rapidjson::Value bgColorVal(rapidjson::kObjectType);
        bgColorVal.AddMember("type", "glm::vec3", alloc);
        rapidjson::Value bgColorData(rapidjson::kArrayType);
        bgColorData.PushBack(c.backgroundColor.x, alloc);
        bgColorData.PushBack(c.backgroundColor.y, alloc);
        bgColorData.PushBack(c.backgroundColor.z, alloc);
        bgColorVal.AddMember("data", bgColorData, alloc);
        v.AddMember("backgroundColor", bgColorVal, alloc);

        // Add custom serialization for clearFlags (enum as int)
        v.AddMember("clearFlags", static_cast<int>(c.clearFlags), alloc);

        // Add custom serialization for projectionType (enum as int)
        v.AddMember("projectionType", static_cast<int>(c.projectionType), alloc);

        // Add custom serialization for useSkybox
        v.AddMember("useSkybox", c.useSkybox, alloc);

        // Add custom serialization for skyboxTexturePath
        rapidjson::Value skyboxPathVal;
        skyboxPathVal.SetString(c.skyboxTexturePath.c_str(), static_cast<rapidjson::SizeType>(c.skyboxTexturePath.size()), alloc);
        v.AddMember("skyboxTexturePath", skyboxPathVal, alloc);

        compsObj.AddMember("CameraComponent", v, alloc);
    }
    if (ecs.HasComponent<AnimationComponent>(entity)) {
        auto& c = ecs.GetComponent<AnimationComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("AnimationComponent", v, alloc);
    }
    if (ecs.HasComponent<ActiveComponent>(entity)) {
        auto& c = ecs.GetComponent<ActiveComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ActiveComponent", v, alloc);
    }
    if (ecs.HasComponent<ScriptComponentData>(entity)) {
        auto& scriptComp = ecs.GetComponent<ScriptComponentData>(entity);
        rapidjson::Value scriptObj(rapidjson::kObjectType);

        // NEW FORMAT: Save scripts as an array
        rapidjson::Value scriptsArr(rapidjson::kArrayType);

        for (const auto& sd : scriptComp.scripts) {
            rapidjson::Value scriptDataObj(rapidjson::kObjectType);

            // scriptGuidStr
            rapidjson::Value sguid;
            sguid.SetString(sd.scriptGuidStr.c_str(), static_cast<rapidjson::SizeType>(sd.scriptGuidStr.size()), alloc);
            scriptDataObj.AddMember("scriptGuidStr", sguid, alloc);

            // scriptPath
            rapidjson::Value sp;
            sp.SetString(sd.scriptPath.c_str(), static_cast<rapidjson::SizeType>(sd.scriptPath.size()), alloc);
            scriptDataObj.AddMember("scriptPath", sp, alloc);

            // enabled
            scriptDataObj.AddMember("enabled", rapidjson::Value(sd.enabled), alloc);

            // preserveKeys array
            rapidjson::Value pkArr(rapidjson::kArrayType);
            for (const auto& k : sd.preserveKeys) {
                rapidjson::Value ks;
                ks.SetString(k.c_str(), static_cast<rapidjson::SizeType>(k.size()), alloc);
                pkArr.PushBack(ks, alloc);
            }
            scriptDataObj.AddMember("preserveKeys", pkArr, alloc);

            // entryFunction and autoInvokeEntry
            rapidjson::Value entryVal;
            entryVal.SetString(sd.entryFunction.c_str(), static_cast<rapidjson::SizeType>(sd.entryFunction.size()), alloc);
            scriptDataObj.AddMember("entryFunction", entryVal, alloc);
            scriptDataObj.AddMember("autoInvokeEntry", rapidjson::Value(sd.autoInvokeEntry), alloc);

            // instance state (best-effort): if runtime exists and instanceId valid, ask Scripting to serialize it
            bool savedInstanceState = false;
            if (sd.instanceCreated && sd.instanceId >= 0 && Scripting::GetLuaState()) {
                try {
                    if (Scripting::IsValidInstance(sd.instanceId)) {
                        std::string instJson = Scripting::SerializeInstanceToJson(sd.instanceId);
                        if (!instJson.empty()) {
                            rapidjson::Document tmp;
                            if (!tmp.Parse(instJson.c_str()).HasParseError()) {
                                rapidjson::Value instVal;
                                instVal.CopyFrom(tmp, alloc);
                                scriptDataObj.AddMember("instanceState", instVal, alloc);
                                savedInstanceState = true;
                            }
                            else {
                                // fallback: store as raw string
                                rapidjson::Value raw;
                                raw.SetString(instJson.c_str(), static_cast<rapidjson::SizeType>(instJson.size()), alloc);
                                scriptDataObj.AddMember("instanceStateRaw", raw, alloc);
                                savedInstanceState = true;
                            }
                        }
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "[SerializeScene] Scripting::SerializeInstanceToJson failed: " << e.what() << "\n";
                }
                catch (...) {
                    std::cerr << "[SerializeScene] unknown exception serializing script instance\n";
                }
            }

            // If no runtime instance was saved (EDIT mode), save pendingInstanceState instead
            if (!savedInstanceState && !sd.pendingInstanceState.empty()) {
                ENGINE_PRINT("SAVE DEBUG: Saving pendingInstanceState for ", sd.scriptPath.c_str(), " (size=", sd.pendingInstanceState.size(), ")");
                rapidjson::Document tmp;
                if (!tmp.Parse(sd.pendingInstanceState.c_str()).HasParseError()) {
                    rapidjson::Value instVal;
                    instVal.CopyFrom(tmp, alloc);
                    scriptDataObj.AddMember("instanceState", instVal, alloc);
                    ENGINE_PRINT("  Saved as JSON object");
                }
                else {
                    // fallback: store as raw string
                    rapidjson::Value raw;
                    raw.SetString(sd.pendingInstanceState.c_str(), static_cast<rapidjson::SizeType>(sd.pendingInstanceState.size()), alloc);
                    scriptDataObj.AddMember("instanceStateRaw", raw, alloc);
                    ENGINE_PRINT("  Saved as raw string (parse error)");
                }
            }
            else if (!savedInstanceState) {
                ENGINE_PRINT("SAVE DEBUG: NOT saving pendingInstanceState for ", sd.scriptPath.c_str(), " (empty=", sd.pendingInstanceState.empty(), ")");
            }

            scriptsArr.PushBack(scriptDataObj, alloc);
        }

        scriptObj.AddMember("scripts", scriptsArr, alloc);
        compsObj.AddMember(rapidjson::Value("ScriptComponent", alloc).Move(), scriptObj, alloc);
    }
    if (ecs.HasComponent<BrainComponent>(entity)) {
        auto& c = ecs.GetComponent<BrainComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("BrainComponent", v, alloc);
    }
    if (ecs.HasComponent<ButtonComponent>(entity)) {
        auto& c = ecs.GetComponent<ButtonComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("ButtonComponent", v, alloc);
    }
    if (ecs.HasComponent<SliderComponent>(entity)) {
        auto& c = ecs.GetComponent<SliderComponent>(entity);
        rapidjson::Value v = SerializeComponentToValue(c, alloc);
        compsObj.AddMember("SliderComponent", v, alloc);
    }

    entObj.AddMember("components", compsObj, alloc);

    return entObj;
}

void Serializer::SerializePrefabInstanceDelta(ECSManager& sceneECS, Entity instanceEnt, Entity baselineEnt, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& outComponentsArray)
{
    // --- 1. Standard Reflection Components ---
    // Lambda for components that use your standard SerializeComponentToValue
    auto standardSerializer = [&](const auto& comp, auto& a) {
        return SerializeComponentToValue(comp, a);
        };

    CheckAndSerializeDelta<Transform>("Transform", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<ModelRenderComponent>("ModelRenderComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<SpriteRenderComponent>("SpriteRenderComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<TextRenderComponent>("TextRenderComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<ParticleComponent>("ParticleComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<AudioComponent>("AudioComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<AudioListenerComponent>("AudioListenerComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<AudioReverbZoneComponent>("AudioReverbZoneComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<LightComponent>("LightComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<DirectionalLightComponent>("DirectionalLightComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<PointLightComponent>("PointLightComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<SpotLightComponent>("SpotLightComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<RigidBodyComponent>("RigidBodyComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<ColliderComponent>("ColliderComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<AnimationComponent>("AnimationComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<ActiveComponent>("ActiveComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<BrainComponent>("BrainComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<ButtonComponent>("ButtonComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);
    CheckAndSerializeDelta<SliderComponent>("SliderComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray, standardSerializer);

    // Note: ChildrenComponent and ParentComponent are intentionally SKIPPED.

    // --- 2. Manual Components (Name, Tag, Layer, Sibling) ---

    CheckAndSerializeDelta<NameComponent>("NameComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const NameComponent& c, auto& a) {
            rapidjson::Value obj(rapidjson::kObjectType);
            rapidjson::Value nameStr;
            nameStr.SetString(c.name.c_str(), static_cast<rapidjson::SizeType>(c.name.size()), a);
            obj.AddMember("name", nameStr, a);
            return obj;
        });

    CheckAndSerializeDelta<TagComponent>("TagComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const TagComponent& c, auto& a) {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("tagIndex", c.tagIndex, a);
            return obj;
        });

    CheckAndSerializeDelta<LayerComponent>("LayerComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const LayerComponent& c, auto& a) {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("layerIndex", c.layerIndex, a);
            return obj;
        });

    CheckAndSerializeDelta<SiblingIndexComponent>("SiblingIndexComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const SiblingIndexComponent& c, auto& a) {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("siblingIndex", c.siblingIndex, a);
            return obj;
        });

    // --- 3. Complex Custom Components ---

    // SpriteAnimationComponent
    CheckAndSerializeDelta<SpriteAnimationComponent>("SpriteAnimationComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const SpriteAnimationComponent& c, auto& a) {
            rapidjson::Value animValue(rapidjson::kObjectType);

            // Serialize clips array
            rapidjson::Value clipsArray(rapidjson::kArrayType);
            for (const auto& clip : c.clips) {
                rapidjson::Value clipObj(rapidjson::kObjectType);
                clipObj.AddMember("name", rapidjson::Value(clip.name.c_str(), a), a);
                clipObj.AddMember("loop", clip.loop, a);

                // Serialize frames with UV coordinates
                rapidjson::Value framesArray(rapidjson::kArrayType);
                for (const auto& frame : clip.frames) {
                    rapidjson::Value frameObj(rapidjson::kObjectType);

                    // Texture GUID
                    std::string guidStr = GUIDUtilities::ConvertGUID128ToString(frame.textureGUID);
                    frameObj.AddMember("textureGUID", rapidjson::Value(guidStr.c_str(), a), a);
                    frameObj.AddMember("texturePath", rapidjson::Value(frame.texturePath.c_str(), a), a);

                    // UV coordinates - MANUALLY SERIALIZE
                    rapidjson::Value uvOffsetArray(rapidjson::kArrayType);
                    uvOffsetArray.PushBack(frame.uvOffset.x, a);
                    uvOffsetArray.PushBack(frame.uvOffset.y, a);
                    frameObj.AddMember("uvOffset", uvOffsetArray, a);

                    rapidjson::Value uvScaleArray(rapidjson::kArrayType);
                    uvScaleArray.PushBack(frame.uvScale.x, a);
                    uvScaleArray.PushBack(frame.uvScale.y, a);
                    frameObj.AddMember("uvScale", uvScaleArray, a);

                    frameObj.AddMember("duration", frame.duration, a);
                    framesArray.PushBack(frameObj, a);
                }
                clipObj.AddMember("frames", framesArray, a);
                clipsArray.PushBack(clipObj, a);
            }
            animValue.AddMember("clips", clipsArray, a);

            // Other fields
            animValue.AddMember("currentClipIndex", c.currentClipIndex, a);
            animValue.AddMember("currentFrameIndex", c.currentFrameIndex, a);
            animValue.AddMember("timeInCurrentFrame", c.timeInCurrentFrame, a);
            animValue.AddMember("playbackSpeed", c.playbackSpeed, a);
            animValue.AddMember("playing", c.playing, a);
            animValue.AddMember("enabled", c.enabled, a);
            animValue.AddMember("autoPlay", c.autoPlay, a);

            return animValue;
        });

    // CameraComponent
    CheckAndSerializeDelta<CameraComponent>("CameraComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const CameraComponent& c, auto& a) {
            rapidjson::Value v = SerializeComponentToValue(c, a);

            // Add custom serialization for target and up (glm::vec3)
            rapidjson::Value targetVal(rapidjson::kObjectType);
            targetVal.AddMember("type", "glm::vec3", a);
            rapidjson::Value targetData(rapidjson::kArrayType);
            targetData.PushBack(c.target.x, a);
            targetData.PushBack(c.target.y, a);
            targetData.PushBack(c.target.z, a);
            targetVal.AddMember("data", targetData, a);
            v.AddMember("target", targetVal, a);

            rapidjson::Value upVal(rapidjson::kObjectType);
            upVal.AddMember("type", "glm::vec3", a);
            rapidjson::Value upData(rapidjson::kArrayType);
            upData.PushBack(c.up.x, a);
            upData.PushBack(c.up.y, a);
            upData.PushBack(c.up.z, a);
            upVal.AddMember("data", upData, a);
            v.AddMember("up", upVal, a);

            // Add custom serialization for backgroundColor (glm::vec3)
            rapidjson::Value bgColorVal(rapidjson::kObjectType);
            bgColorVal.AddMember("type", "glm::vec3", a);
            rapidjson::Value bgColorData(rapidjson::kArrayType);
            bgColorData.PushBack(c.backgroundColor.x, a);
            bgColorData.PushBack(c.backgroundColor.y, a);
            bgColorData.PushBack(c.backgroundColor.z, a);
            bgColorVal.AddMember("data", bgColorData, a);
            v.AddMember("backgroundColor", bgColorVal, a);

            // Add custom serialization for clearFlags (enum as int)
            v.AddMember("clearFlags", static_cast<int>(c.clearFlags), a);

            // Add custom serialization for projectionType (enum as int)
            v.AddMember("projectionType", static_cast<int>(c.projectionType), a);

            // Add custom serialization for useSkybox
            v.AddMember("useSkybox", c.useSkybox, a);

            // Add custom serialization for skyboxTexturePath
            rapidjson::Value skyboxPathVal;
            skyboxPathVal.SetString(c.skyboxTexturePath.c_str(), static_cast<rapidjson::SizeType>(c.skyboxTexturePath.size()), a);
            v.AddMember("skyboxTexturePath", skyboxPathVal, a);

            return v;
        });

    // ScriptComponentData
    CheckAndSerializeDelta<ScriptComponentData>("ScriptComponent", sceneECS, instanceEnt, baselineEnt, alloc, outComponentsArray,
        [&](const ScriptComponentData& scriptComp, auto& a) {
            rapidjson::Value scriptObj(rapidjson::kObjectType);
            rapidjson::Value scriptsArr(rapidjson::kArrayType);

            for (const auto& sd : scriptComp.scripts) {
                rapidjson::Value scriptDataObj(rapidjson::kObjectType);

                // scriptGuidStr
                rapidjson::Value sguid;
                sguid.SetString(sd.scriptGuidStr.c_str(), static_cast<rapidjson::SizeType>(sd.scriptGuidStr.size()), a);
                scriptDataObj.AddMember("scriptGuidStr", sguid, a);

                // scriptPath
                rapidjson::Value sp;
                sp.SetString(sd.scriptPath.c_str(), static_cast<rapidjson::SizeType>(sd.scriptPath.size()), a);
                scriptDataObj.AddMember("scriptPath", sp, a);

                // enabled
                scriptDataObj.AddMember("enabled", rapidjson::Value(sd.enabled), a);

                // preserveKeys array
                rapidjson::Value pkArr(rapidjson::kArrayType);
                for (const auto& k : sd.preserveKeys) {
                    rapidjson::Value ks;
                    ks.SetString(k.c_str(), static_cast<rapidjson::SizeType>(k.size()), a);
                    pkArr.PushBack(ks, a);
                }
                scriptDataObj.AddMember("preserveKeys", pkArr, a);

                // entryFunction and autoInvokeEntry
                rapidjson::Value entryVal;
                entryVal.SetString(sd.entryFunction.c_str(), static_cast<rapidjson::SizeType>(sd.entryFunction.size()), a);
                scriptDataObj.AddMember("entryFunction", entryVal, a);
                scriptDataObj.AddMember("autoInvokeEntry", rapidjson::Value(sd.autoInvokeEntry), a);

                // instance state (best-effort): if runtime exists and instanceId valid, ask Scripting to serialize it
                bool savedInstanceState = false;
                if (sd.instanceCreated && sd.instanceId >= 0 && Scripting::GetLuaState()) {
                    try {
                        if (Scripting::IsValidInstance(sd.instanceId)) {
                            std::string instJson = Scripting::SerializeInstanceToJson(sd.instanceId);
                            if (!instJson.empty()) {
                                rapidjson::Document tmp;
                                if (!tmp.Parse(instJson.c_str()).HasParseError()) {
                                    rapidjson::Value instVal;
                                    instVal.CopyFrom(tmp, a);
                                    scriptDataObj.AddMember("instanceState", instVal, a);
                                    savedInstanceState = true;
                                }
                                else {
                                    // fallback: store as raw string
                                    rapidjson::Value raw;
                                    raw.SetString(instJson.c_str(), static_cast<rapidjson::SizeType>(instJson.size()), a);
                                    scriptDataObj.AddMember("instanceStateRaw", raw, a);
                                    savedInstanceState = true;
                                }
                            }
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "[SerializeScene] Scripting::SerializeInstanceToJson failed: " << e.what() << "\n";
                    }
                    catch (...) {
                        std::cerr << "[SerializeScene] unknown exception serializing script instance\n";
                    }
                }

                // If no runtime instance was saved (EDIT mode), save pendingInstanceState instead
                if (!savedInstanceState && !sd.pendingInstanceState.empty()) {
                    ENGINE_PRINT("SAVE DEBUG: Saving pendingInstanceState for ", sd.scriptPath.c_str(), " (size=", sd.pendingInstanceState.size(), ")");
                    rapidjson::Document tmp;
                    if (!tmp.Parse(sd.pendingInstanceState.c_str()).HasParseError()) {
                        rapidjson::Value instVal;
                        instVal.CopyFrom(tmp, a);
                        scriptDataObj.AddMember("instanceState", instVal, a);
                        ENGINE_PRINT("  Saved as JSON object");
                    }
                    else {
                        // fallback: store as raw string
                        rapidjson::Value raw;
                        raw.SetString(sd.pendingInstanceState.c_str(), static_cast<rapidjson::SizeType>(sd.pendingInstanceState.size()), a);
                        scriptDataObj.AddMember("instanceStateRaw", raw, a);
                        ENGINE_PRINT("  Saved as raw string (parse error)");
                    }
                }
                else if (!savedInstanceState) {
                    ENGINE_PRINT("SAVE DEBUG: NOT saving pendingInstanceState for ", sd.scriptPath.c_str(), " (empty=", sd.pendingInstanceState.empty(), ")");
                }

                scriptsArr.PushBack(scriptDataObj, a);
            }

            scriptObj.AddMember("scripts", scriptsArr, a);
            return scriptObj;
        });
}

void Serializer::SerializePrefabOverridesRecursive(ECSManager& sceneECS, Entity instanceEnt, Entity baselineEnt, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& outEntityNode) {
    // 1. Save Basic ID (So we know who this override belongs to)
    // We usually save the Name or Sibling Index to identify the entity on load.
    outEntityNode = SerializeEntityGUID(instanceEnt, alloc, outEntityNode);
    if (sceneECS.HasComponent<NameComponent>(instanceEnt)) {
        outEntityNode.AddMember("Name", rapidjson::StringRef(sceneECS.GetComponent<NameComponent>(instanceEnt).name.c_str()), alloc);
    }
    if (sceneECS.HasComponent<ParentComponent>(instanceEnt)) {
		rapidjson::Value v = SerializeComponentToValue(sceneECS.GetComponent<ParentComponent>(instanceEnt), alloc);
        outEntityNode.AddMember("ParentComponent", v, alloc);
    }

    // 2. Compare & Save Component Overrides
	rapidjson::Value overridesArray(rapidjson::kArrayType);
    SerializePrefabInstanceDelta(sceneECS, instanceEnt, baselineEnt, alloc, overridesArray);

    if (!overridesArray.Empty()) {
        outEntityNode.AddMember("ComponentOverrides", overridesArray, alloc);
    }

    // 3. Recurse Children
    if (sceneECS.HasComponent<ChildrenComponent>(instanceEnt)) {
        auto& instChildren = sceneECS.GetComponent<ChildrenComponent>(instanceEnt).children;
        auto& baseChildren = sceneECS.GetComponent<ChildrenComponent>(baselineEnt).children;

        rapidjson::Value childrenOverrides(rapidjson::kArrayType);

        // Match children by Name (or Index)
        for (const auto& instChildGUID : instChildren) {
            Entity instChild = EntityGUIDRegistry::GetInstance().GetEntityByGUID(instChildGUID);
            std::string childName = sceneECS.GetComponent<NameComponent>(instChild).name;

            // Find matching child in Baseline
            Entity baseChild = static_cast<Entity>(-1);
            for (const auto& baseChildGUID : baseChildren) {
                Entity bChild = EntityGUIDRegistry::GetInstance().GetEntityByGUID(baseChildGUID); // Note: Need separate registry for baseline?
                // Actually, for a dummy world, you might just iterate entities directly.
                if (sceneECS.GetComponent<NameComponent>(bChild).name == childName) {
                    baseChild = bChild;
                    break;
                }
            }

            // If we found a match, check for differences
            if (baseChild != static_cast<Entity>(-1)) {
                rapidjson::Value childNode(rapidjson::kObjectType);
                SerializePrefabOverridesRecursive(sceneECS, instChild, baseChild, alloc, childNode);

                // Only add to list if there was actually an override inside
                if (childNode.HasMember("ComponentOverrides") || childNode.HasMember("Children")) {
                    childrenOverrides.PushBack(childNode, alloc);
                }
            }
            else {
                // This is a NEW child added in the scene (not in prefab).
                // You need to serialize it fully as a "Added Entity".
                SerializeEntity(instChild, alloc);
            }
        }

        if (!childrenOverrides.Empty()) {
            outEntityNode.AddMember("Children", childrenOverrides, alloc);
        }
    }
}

void Serializer::UpdateEntityGUID_Safe(ECSManager& ecs, Entity entity, GUID_128 newGUID) {
	GUID_128 oldGUID = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(entity);

    if (oldGUID == newGUID) return; // Already correct

    // 1. Update Registry
    // (Assuming your registry has Unregister/Register. Adjust if names differ)
    EntityGUIDRegistry::GetInstance().Unregister(entity);
    EntityGUIDRegistry::GetInstance().Register(entity, newGUID);

    // 2. Fix MY Parent's reference to ME
    if (ecs.HasComponent<ParentComponent>(entity)) {
        GUID_128 parentGUID = ecs.GetComponent<ParentComponent>(entity).parent;
        Entity parentEnt = EntityGUIDRegistry::GetInstance().GetEntityByGUID(parentGUID);

        if (parentEnt != static_cast<Entity>(-1) && ecs.HasComponent<ChildrenComponent>(parentEnt)) {
            auto& childrenList = ecs.GetComponent<ChildrenComponent>(parentEnt).children;
            // Find my old GUID in parent's list and replace with new GUID
            std::replace(childrenList.begin(), childrenList.end(), oldGUID, newGUID);
        }
    }

    // 3. Fix MY Children's reference to ME
    if (ecs.HasComponent<ChildrenComponent>(entity)) {
        auto& myChildren = ecs.GetComponent<ChildrenComponent>(entity).children;
        for (const auto& childGUID : myChildren) {
            Entity childEnt = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);

            if (childEnt != static_cast<Entity>(-1) && ecs.HasComponent<ParentComponent>(childEnt)) {
                // Update child's parent pointer
                ecs.GetComponent<ParentComponent>(childEnt).parent = newGUID;
            }
        }
    }
}

void Serializer::RestorePrefabHierarchy(ECSManager& ecs, Entity currentEntity, const rapidjson::Value& jsonNode) {
    // 1. Restore GUID for THIS entity
    // (This calls the safe swapper we wrote above)
    if (jsonNode.HasMember("guid")) { // Ensure this key matches your save format ("guid" vs "GUID")
        GUID_128 savedGUID = DeserializeEntityGUID(jsonNode);
        UpdateEntityGUID_Safe(ecs, currentEntity, savedGUID);
    }

    // 2. Recurse for Children
    if (jsonNode.HasMember("Children") && ecs.HasComponent<ChildrenComponent>(currentEntity)) {
        const auto& jsonChildren = jsonNode["Children"];
        auto& childrenComp = ecs.GetComponent<ChildrenComponent>(currentEntity);

        // COPY the real children entities to a vector first.
        // Why? Because 'UpdateEntityGUID_Safe' modifies the GUIDs, which effectively modifies
        // the map/registry lookups. We want a stable list of Entity IDs to iterate.
        std::vector<Entity> realChildrenEntities;
        for (auto& g : childrenComp.children) {
            realChildrenEntities.push_back(EntityGUIDRegistry::GetInstance().GetEntityByGUID(g));
        }

        // Iterate JSON children
        for (const auto& jsonChild : jsonChildren.GetArray()) {
            if (!jsonChild.HasMember("Name")) continue;
            std::string nameToFind = jsonChild["Name"].GetString();

            // Find match in real hierarchy by Name
            Entity match = static_cast<Entity>(-1);
            for (Entity ent : realChildrenEntities) {
                if (ent != static_cast<Entity>(-1) && ecs.GetComponent<NameComponent>(ent).name == nameToFind) {
                    match = ent;
                    break;
                }
            }

            // If found, recurse down
            if (match != static_cast<Entity>(-1)) {
                RestorePrefabHierarchy(ecs, match, jsonChild);
            }
        }
    }
}

void Serializer::DeserializeEntity(ECSManager& ecs, const rapidjson::Value& entObj, bool isPrefab, Entity entity, bool skipSpawnChildren) {
    if (!entObj.IsObject()) return;

    // 1. Check: Is this a Prefab Instance?
    // (Assuming you saved "PrefabPath" at the top level of your entity JSON)
    if (entObj.HasMember("PrefabPath")) {
        std::string path = entObj["PrefabPath"].GetString();

        // A. SPAWN (Creates Random GUIDs)
        Entity instanceRoot = InstantiatePrefabFromFile(path);

        // B. RESTORE HIERARCHY GUIDS (The Fix)
        // This recursively swaps all random GUIDs for the saved ones 
        // AND fixes the Parent/Children links internally.
        RestorePrefabHierarchy(ecs, instanceRoot, entObj);

        // C. Link to Scene Parent (If applicable)
        // Now that instanceRoot has the correct GUID, we can safely link it to the scene.
        if (entObj.HasMember("ParentComponent")) {
            // Your existing logic to read parent GUID...
            GUID_string parentGUIDStr = GetString(entObj["ParentComponent"]["data"], 0);
            if (parentGUIDStr != "") {
                GUID_128 parentGUID = GUIDUtilities::ConvertStringToGUID128(parentGUIDStr);

                if (!ecs.HasComponent<ParentComponent>(instanceRoot))
                    ecs.AddComponent<ParentComponent>(instanceRoot, ParentComponent{});

                ecs.GetComponent<ParentComponent>(instanceRoot).parent = parentGUID;

                // IMPORTANT: You must also add this child to the Parent's list!
                // (If your engine doesn't do this automatically via system)
				std::string instanceName = ecs.GetComponent<NameComponent>(instanceRoot).name;
                Entity parentEnt = EntityGUIDRegistry::GetInstance().GetEntityByGUID(parentGUID);
                if (parentEnt != -1 && ecs.HasComponent<ChildrenComponent>(parentEnt)) {
                    GUID_128 instanceGUID = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(instanceRoot);
                    auto& childComp = ecs.GetComponent<ChildrenComponent>(parentEnt);

                    std::vector<Entity> realChildrenEntities;
                    for (auto& g : childComp.children) {
                        realChildrenEntities.push_back(EntityGUIDRegistry::GetInstance().GetEntityByGUID(g));
                    }

                    for (size_t i = 0; i < realChildrenEntities.size(); ++i) {
                        Entity child = realChildrenEntities[i];
                        if (!ecs.HasComponent<NameComponent>(child)) continue;

						std::string childName = ecs.GetComponent<NameComponent>(child).name;
                        if (childName == instanceName) {
                            // Fix MY Parent's reference to ME
                            auto& childrenList = childComp.children;
                            // Find my old GUID in parent's list and replace with new GUID
                            std::replace(childrenList.begin(), childrenList.end(), EntityGUIDRegistry::GetInstance().GetGUIDByEntity(realChildrenEntities[i]), instanceGUID);
                            break;
                        }
                    }
                }
            }
        }

        // D. Apply Overrides
        ApplyPrefabOverridesRecursive(ecs, instanceRoot, entObj);

        return;
    }

    Entity newEnt{};
    if (isPrefab) {
        newEnt = entity;
    }
    else newEnt = CreateEntityViaGUID(entObj);

    const rapidjson::Value& comps = entObj["components"];

    // NameComponent
    if (comps.HasMember("NameComponent")) {
        const rapidjson::Value& nv = comps["NameComponent"];
        ecs.AddComponent<NameComponent>(newEnt, NameComponent{});
        auto& nameComp = ecs.GetComponent<NameComponent>(newEnt);
        DeserializeNameComponent(nameComp, nv);
    }

    // Transform
    if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
        const rapidjson::Value& t = comps["Transform"];
        ecs.AddComponent<Transform>(newEnt, Transform{});
        DeserializeTransformComponent(newEnt, t);
    }

    // PrefabLinkComponent
    if (!isPrefab) {
        if (comps.HasMember("PrefabLinkComponent") && comps["PrefabLinkComponent"].IsObject()) {
            const auto& prefabCompJSON = comps["PrefabLinkComponent"];
            ecs.AddComponent<PrefabLinkComponent>(newEnt, PrefabLinkComponent{});
            auto& prefabComp = ecs.GetComponent<PrefabLinkComponent>(newEnt);
            DeserializePrefabLinkComponent(prefabComp, prefabCompJSON);
        
            InstantiatePrefabIntoEntity(prefabComp.prefabPath, newEnt);
            return;
        }
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

    // SiblingIndexComponent
    if (comps.HasMember("SiblingIndexComponent")) {
        const rapidjson::Value& sv = comps["SiblingIndexComponent"];
        ecs.AddComponent<SiblingIndexComponent>(newEnt, SiblingIndexComponent{});
        auto& siblingComp = ecs.GetComponent<SiblingIndexComponent>(newEnt);
        DeserializeSiblingIndexComponent(siblingComp, sv);
    }

    // ParentComponent
    if (!isPrefab && comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
        const auto& parentCompJSON = comps["ParentComponent"];
        if (!ecs.HasComponent<ParentComponent>(newEnt)) {
            ecs.AddComponent<ParentComponent>(newEnt, ParentComponent{});
        }
        auto& parentComp = ecs.GetComponent<ParentComponent>(newEnt);
        DeserializeParentComponent(parentComp, parentCompJSON);
    }

    // ChildrenComponent
    if (!isPrefab && comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
        const auto& childrenCompJSON = comps["ChildrenComponent"];
        if (!ecs.HasComponent<ChildrenComponent>(newEnt)) {
            ecs.AddComponent<ChildrenComponent>(newEnt, ChildrenComponent{});
        }
        auto& childComp = ecs.GetComponent<ChildrenComponent>(newEnt);
        DeserializeChildrenComponent(childComp, childrenCompJSON);
    }

    // ModelRenderComponent
    if (comps.HasMember("ModelRenderComponent")) {
        const rapidjson::Value& mv = comps["ModelRenderComponent"];
        ecs.AddComponent<ModelRenderComponent>(newEnt, ModelRenderComponent{});
        auto& modelComp = ecs.GetComponent<ModelRenderComponent>(newEnt);
        DeserializeModelComponent(modelComp, mv, newEnt, skipSpawnChildren);
    }

    // SpriteRenderComponent
    if (comps.HasMember("SpriteRenderComponent")) {
        const rapidjson::Value& mv = comps["SpriteRenderComponent"];
        ecs.AddComponent<SpriteRenderComponent>(newEnt, SpriteRenderComponent{});
        auto& spriteComp = ecs.GetComponent<SpriteRenderComponent>(newEnt);
        DeserializeSpriteComponent(spriteComp, mv);
    }

    // SpriteAnimationComponent
    if (comps.HasMember("SpriteAnimationComponent") && comps["SpriteAnimationComponent"].IsObject()) {
        const rapidjson::Value& mv = comps["SpriteAnimationComponent"];
        ecs.AddComponent<SpriteAnimationComponent>(newEnt, SpriteAnimationComponent{});
        auto& animComp = ecs.GetComponent<SpriteAnimationComponent>(newEnt);
        DeserializeSpriteAnimationComponent(animComp, mv);
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

    // AudioListenerComponent
    if (comps.HasMember("AudioListenerComponent") && comps["AudioListenerComponent"].IsObject()) {
        const rapidjson::Value& tv = comps["AudioListenerComponent"];
        ecs.AddComponent<AudioListenerComponent>(newEnt, AudioListenerComponent{});
        auto& audioListenerComp = ecs.GetComponent<AudioListenerComponent>(newEnt);
        DeserializeAudioListenerComponent(audioListenerComp, tv);
    }

    // AudioReverbZoneComponent
    if (comps.HasMember("AudioReverbZoneComponent") && comps["AudioReverbZoneComponent"].IsObject()) {
        const rapidjson::Value& tv = comps["AudioReverbZoneComponent"];
        ecs.AddComponent<AudioReverbZoneComponent>(newEnt, AudioReverbZoneComponent{});
        auto& audioReverbZoneComp = ecs.GetComponent<AudioReverbZoneComponent>(newEnt);
        DeserializeAudioReverbZoneComponent(audioReverbZoneComp, tv);
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

    // CameraComponent
    if (comps.HasMember("CameraComponent") && comps["CameraComponent"].IsObject()) {
        const rapidjson::Value& tv = comps["CameraComponent"];
        ecs.AddComponent<CameraComponent>(newEnt, CameraComponent{});
        auto& cameraComp = ecs.GetComponent<CameraComponent>(newEnt);
        DeserializeCameraComponent(cameraComp, tv);
    }

	// AnimationComponent
    if (comps.HasMember("AnimationComponent") && comps["AnimationComponent"].IsObject()) {
        const rapidjson::Value& tv = comps["AnimationComponent"];
        AnimationComponent animComp{};
        TypeResolver<AnimationComponent>::Get()->Deserialize(&animComp, tv);
        ecs.AddComponent<AnimationComponent>(newEnt, animComp);

		// For prefabs, we need to initialise the animation component after deserialization.
        if (isPrefab && ecs.HasComponent<ModelRenderComponent>(newEnt)) {
			auto& modelComp = ecs.GetComponent<ModelRenderComponent>(newEnt);
            auto& actualAnimComp = ecs.GetComponent<AnimationComponent>(newEnt);
            ecs.animationSystem->InitialiseAnimationComponent(newEnt, modelComp, actualAnimComp);
        }
    }

    // ActiveComponent
    if (comps.HasMember("ActiveComponent") && comps["ActiveComponent"].IsObject()) {
        const rapidjson::Value& tv = comps["ActiveComponent"];
        ecs.AddComponent<ActiveComponent>(newEnt, ActiveComponent{});
        auto& activeComp = ecs.GetComponent<ActiveComponent>(newEnt);
        DeserializeActiveComponent(activeComp, tv);
    }

    // Script component (engine-side)
    if (comps.HasMember("ScriptComponent") && comps["ScriptComponent"].IsObject())
    {
        const rapidjson::Value& sv = comps["ScriptComponent"];
        Serializer::DeserializeScriptComponent(newEnt, sv);
    }
    // BrainComponent
    if (comps.HasMember("BrainComponent") && comps["BrainComponent"].IsObject()) {
        const auto& brainCompJSON = comps["BrainComponent"];
        ecs.AddComponent<BrainComponent>(newEnt, BrainComponent{});
        auto& brainComp = ecs.GetComponent<BrainComponent>(newEnt);
        DeserializeBrainComponent(brainComp, brainCompJSON);
    }
    // ButtonComponent
    if (comps.HasMember("ButtonComponent") && comps["ButtonComponent"].IsObject()) {
        const auto& buttonCompJSON = comps["ButtonComponent"];
        ecs.AddComponent<ButtonComponent>(newEnt, ButtonComponent{});
        auto& buttonComp = ecs.GetComponent<ButtonComponent>(newEnt);
        DeserializeButtonComponent(buttonComp, buttonCompJSON);
    }
    // SliderComponent
    if (comps.HasMember("SliderComponent") && comps["SliderComponent"].IsObject()) {
        const auto& sliderCompJSON = comps["SliderComponent"];
        ecs.AddComponent<SliderComponent>(newEnt, SliderComponent{});
        auto& sliderComp = ecs.GetComponent<SliderComponent>(newEnt);
        DeserializeSliderComponent(sliderComp, sliderCompJSON);
    }

    // Ensure all entities have TagComponent and LayerComponent
    if (!ecs.HasComponent<TagComponent>(newEnt)) {
        ecs.AddComponent<TagComponent>(newEnt, TagComponent{ 0 });
    }
    if (!ecs.HasComponent<LayerComponent>(newEnt)) {
        ecs.AddComponent<LayerComponent>(newEnt, LayerComponent{ 0 });
    }
}

void Serializer::ApplyPrefabOverridesRecursive(ECSManager& ecs, Entity currentEntity, const rapidjson::Value& jsonNode) {
    // 1. Apply Component Overrides for THIS entity
    // This overwrites values like Transform, Light Color, etc.
    if (jsonNode.HasMember("ComponentOverrides")) {
        const auto& overrides = jsonNode["ComponentOverrides"];

        // Loop through the list: [{"Transform":...}, {"LightComponent":...}]
        for (const auto& compWrapper : overrides.GetArray()) {
            for (auto it = compWrapper.MemberBegin(); it != compWrapper.MemberEnd(); ++it) {
                std::string typeName = it->name.GetString();
                const rapidjson::Value& data = it->value;

                // Reuse your existing deserializers to apply the data
                if (typeName == "Transform") {
                    // Transform is usually mandatory, but good to be safe
                    if (!ecs.HasComponent<Transform>(currentEntity)) ecs.AddComponent<Transform>(currentEntity, Transform{});
                    DeserializeTransformComponent(currentEntity, data);
                }
                else if (typeName == "NameComponent") {
                    if (!ecs.HasComponent<NameComponent>(currentEntity)) ecs.AddComponent<NameComponent>(currentEntity, NameComponent{});
                    DeserializeNameComponent(ecs.GetComponent<NameComponent>(currentEntity), data);
                }
                else if (typeName == "TagComponent") {
                    if (!ecs.HasComponent<TagComponent>(currentEntity)) ecs.AddComponent<TagComponent>(currentEntity, TagComponent{});
                    DeserializeTagComponent(ecs.GetComponent<TagComponent>(currentEntity), data);
                }
                else if (typeName == "LayerComponent") {
                    if (!ecs.HasComponent<LayerComponent>(currentEntity)) ecs.AddComponent<LayerComponent>(currentEntity, LayerComponent{});
                    DeserializeLayerComponent(ecs.GetComponent<LayerComponent>(currentEntity), data);
                }
                else if (typeName == "SiblingIndexComponent") {
                    if (!ecs.HasComponent<SiblingIndexComponent>(currentEntity)) ecs.AddComponent<SiblingIndexComponent>(currentEntity, SiblingIndexComponent{});
                    DeserializeSiblingIndexComponent(ecs.GetComponent<SiblingIndexComponent>(currentEntity), data);
                }
                else if (typeName == "ActiveComponent") {
                    if (!ecs.HasComponent<ActiveComponent>(currentEntity)) ecs.AddComponent<ActiveComponent>(currentEntity, ActiveComponent{});
                    DeserializeActiveComponent(ecs.GetComponent<ActiveComponent>(currentEntity), data);
                }

                // --- GRAPHICS ---
                else if (typeName == "ModelRenderComponent") {
                    if (!ecs.HasComponent<ModelRenderComponent>(currentEntity)) ecs.AddComponent<ModelRenderComponent>(currentEntity, ModelRenderComponent{});
                    DeserializeModelComponent(ecs.GetComponent<ModelRenderComponent>(currentEntity), data, currentEntity);
                }
                else if (typeName == "SpriteRenderComponent") {
                    if (!ecs.HasComponent<SpriteRenderComponent>(currentEntity)) ecs.AddComponent<SpriteRenderComponent>(currentEntity, SpriteRenderComponent{});
                    DeserializeSpriteComponent(ecs.GetComponent<SpriteRenderComponent>(currentEntity), data);
                }
                else if (typeName == "SpriteAnimationComponent") {
                    if (!ecs.HasComponent<SpriteAnimationComponent>(currentEntity)) ecs.AddComponent<SpriteAnimationComponent>(currentEntity, SpriteAnimationComponent{});
                    DeserializeSpriteAnimationComponent(ecs.GetComponent<SpriteAnimationComponent>(currentEntity), data);
                }
                else if (typeName == "TextRenderComponent") {
                    if (!ecs.HasComponent<TextRenderComponent>(currentEntity)) ecs.AddComponent<TextRenderComponent>(currentEntity, TextRenderComponent{});
                    DeserializeTextComponent(ecs.GetComponent<TextRenderComponent>(currentEntity), data);
                }
                else if (typeName == "ParticleComponent") {
                    if (!ecs.HasComponent<ParticleComponent>(currentEntity)) ecs.AddComponent<ParticleComponent>(currentEntity, ParticleComponent{});
                    DeserializeParticleComponent(ecs.GetComponent<ParticleComponent>(currentEntity), data);
                }
                else if (typeName == "CameraComponent") {
                    if (!ecs.HasComponent<CameraComponent>(currentEntity)) ecs.AddComponent<CameraComponent>(currentEntity, CameraComponent{});
                    DeserializeCameraComponent(ecs.GetComponent<CameraComponent>(currentEntity), data);
                }
                else if (typeName == "AnimationComponent") {
                    // Animation usually requires TypeResolver or specific logic
                    AnimationComponent animComp{};
                    TypeResolver<AnimationComponent>::Get()->Deserialize(&animComp, data);
                    ecs.AddComponent<AnimationComponent>(currentEntity, animComp);

                    // Re-initialization might be needed if model changed
                    if (ecs.HasComponent<ModelRenderComponent>(currentEntity)) {
                        auto& modelComp = ecs.GetComponent<ModelRenderComponent>(currentEntity);
                        auto& actualAnimComp = ecs.GetComponent<AnimationComponent>(currentEntity);
                        if (ecs.animationSystem)
                            ecs.animationSystem->InitialiseAnimationComponent(currentEntity, modelComp, actualAnimComp);
                    }
                }

                // --- AUDIO ---
                else if (typeName == "AudioComponent") {
                    if (!ecs.HasComponent<AudioComponent>(currentEntity)) ecs.AddComponent<AudioComponent>(currentEntity, AudioComponent{});
                    DeserializeAudioComponent(ecs.GetComponent<AudioComponent>(currentEntity), data);
                }
                else if (typeName == "AudioListenerComponent") {
                    if (!ecs.HasComponent<AudioListenerComponent>(currentEntity)) ecs.AddComponent<AudioListenerComponent>(currentEntity, AudioListenerComponent{});
                    DeserializeAudioListenerComponent(ecs.GetComponent<AudioListenerComponent>(currentEntity), data);
                }
                else if (typeName == "AudioReverbZoneComponent") {
                    if (!ecs.HasComponent<AudioReverbZoneComponent>(currentEntity)) ecs.AddComponent<AudioReverbZoneComponent>(currentEntity, AudioReverbZoneComponent{});
                    DeserializeAudioReverbZoneComponent(ecs.GetComponent<AudioReverbZoneComponent>(currentEntity), data);
                }

                // --- LIGHTING ---
                else if (typeName == "DirectionalLightComponent") {
                    if (!ecs.HasComponent<DirectionalLightComponent>(currentEntity)) ecs.AddComponent<DirectionalLightComponent>(currentEntity, DirectionalLightComponent{});
                    DeserializeDirLightComponent(ecs.GetComponent<DirectionalLightComponent>(currentEntity), data);
                }
                else if (typeName == "PointLightComponent") {
                    if (!ecs.HasComponent<PointLightComponent>(currentEntity)) ecs.AddComponent<PointLightComponent>(currentEntity, PointLightComponent{});
                    DeserializePointLightComponent(ecs.GetComponent<PointLightComponent>(currentEntity), data);
                }
                else if (typeName == "SpotLightComponent") {
                    if (!ecs.HasComponent<SpotLightComponent>(currentEntity)) ecs.AddComponent<SpotLightComponent>(currentEntity, SpotLightComponent{});
                    DeserializeSpotLightComponent(ecs.GetComponent<SpotLightComponent>(currentEntity), data);
                }

                // --- PHYSICS ---
                else if (typeName == "RigidBodyComponent") {
                    if (!ecs.HasComponent<RigidBodyComponent>(currentEntity)) ecs.AddComponent<RigidBodyComponent>(currentEntity, RigidBodyComponent{});
                    DeserializeRigidBodyComponent(ecs.GetComponent<RigidBodyComponent>(currentEntity), data);
                }
                else if (typeName == "ColliderComponent") {
                    if (!ecs.HasComponent<ColliderComponent>(currentEntity)) ecs.AddComponent<ColliderComponent>(currentEntity, ColliderComponent{});
                    DeserializeColliderComponent(ecs.GetComponent<ColliderComponent>(currentEntity), data);
                }

                // --- LOGIC & UI ---
                else if (typeName == "ScriptComponent") {
                    // Note: Serialization key is "ScriptComponent", but data struct is ScriptComponentData
                    // Your deserializer likely handles the internal add/get
                    Serializer::DeserializeScriptComponent(currentEntity, data);
                }
                else if (typeName == "BrainComponent") {
                    if (!ecs.HasComponent<BrainComponent>(currentEntity)) ecs.AddComponent<BrainComponent>(currentEntity, BrainComponent{});
                    DeserializeBrainComponent(ecs.GetComponent<BrainComponent>(currentEntity), data);
                }
                else if (typeName == "ButtonComponent") {
                    if (!ecs.HasComponent<ButtonComponent>(currentEntity)) ecs.AddComponent<ButtonComponent>(currentEntity, ButtonComponent{});
                    DeserializeButtonComponent(ecs.GetComponent<ButtonComponent>(currentEntity), data);
                }
                else if (typeName == "SliderComponent") {
                    if (!ecs.HasComponent<SliderComponent>(currentEntity)) ecs.AddComponent<SliderComponent>(currentEntity, SliderComponent{});
                    DeserializeSliderComponent(ecs.GetComponent<SliderComponent>(currentEntity), data);
                }
            }
        }
    }

    // 2. Recursively Handle Children
    // We only descend if the JSON has overrides for children
    if (jsonNode.HasMember("Children") && ecs.HasComponent<ChildrenComponent>(currentEntity)) {
        const auto& jsonChildren = jsonNode["Children"];
        auto& childrenComp = ecs.GetComponent<ChildrenComponent>(currentEntity);

        for (const auto& childNode : jsonChildren.GetArray()) {
            if (!childNode.HasMember("Name")) continue;
            std::string targetName = childNode["Name"].GetString();

            // FIND the matching child in the live hierarchy
            Entity matchingChild = static_cast<Entity>(-1);

            for (const auto& childGUID : childrenComp.children) {
                Entity realChild = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);
                if (ecs.HasComponent<NameComponent>(realChild)) {
                    // Match by Name (Unity-style matching)
                    if (ecs.GetComponent<NameComponent>(realChild).name == targetName) {
                        matchingChild = realChild;
                        break;
                    }
                }
            }

            // If found, recurse to patch that child
            if (matchingChild != static_cast<Entity>(-1)) {
                ApplyPrefabOverridesRecursive(ecs, matchingChild, childNode);
            }
            else {
                // Edge Case: The child exists in the Scene save but not in the Prefab?
                // This means it was "Added in Scene". You would need to CreateEntity here.
                DeserializeEntity(ecs, childNode);
            }
        }
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

    // NOTE: Tags, layers, and sorting layers are now loaded from project-wide settings
    // (TagsLayersSettings.hpp) and are NOT loaded from scene files anymore.
    // Old scene files may still contain these, but they will be ignored.

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        ENGINE_LOG_WARN("[CreateEntitiesFromJson] no entities array in JSON: " + scenePath);
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetECSManager(scenePath);

    const rapidjson::Value& ents = doc["entities"];
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) continue;

        // Use default skipSpawnChildren=false for DeserializeScene to maintain backwards
        // compatibility with older scene files where children might not be serialized
        DeserializeEntity(ecs, entObj);
    }

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

    // Deserialize sorting layers
    if (doc.HasMember("sortingLayers") && doc["sortingLayers"].IsArray()) {
        SortingLayerManager::GetInstance().Clear(); // Clear and reload from file
        const auto& sortingLayersArr = doc["sortingLayers"];
        for (rapidjson::SizeType i = 0; i < sortingLayersArr.Size(); ++i) {
            const auto& layerObj = sortingLayersArr[i];
            if (layerObj.IsObject() && layerObj.HasMember("name")) {
                std::string name = layerObj["name"].GetString();
                SortingLayerManager::GetInstance().AddLayer(name);
            }
        }
    }

    // Deserialize LightingSystem properties (scene-level lighting settings)
    if (doc.HasMember("lightingSystem") && doc["lightingSystem"].IsObject() && ecs.lightingSystem) {
        const auto& lightingObj = doc["lightingSystem"];

        // Ambient mode
        if (lightingObj.HasMember("ambientMode") && lightingObj["ambientMode"].IsInt()) {
            ecs.lightingSystem->ambientMode = static_cast<LightingSystem::AmbientMode>(lightingObj["ambientMode"].GetInt());
        }

        // Ambient sky color
        if (lightingObj.HasMember("ambientSky") && lightingObj["ambientSky"].IsArray() && lightingObj["ambientSky"].Size() >= 3) {
            const auto& arr = lightingObj["ambientSky"];
            ecs.lightingSystem->ambientSky.x = arr[0].GetFloat();
            ecs.lightingSystem->ambientSky.y = arr[1].GetFloat();
            ecs.lightingSystem->ambientSky.z = arr[2].GetFloat();
        }

        // Ambient equator color
        if (lightingObj.HasMember("ambientEquator") && lightingObj["ambientEquator"].IsArray() && lightingObj["ambientEquator"].Size() >= 3) {
            const auto& arr = lightingObj["ambientEquator"];
            ecs.lightingSystem->ambientEquator.x = arr[0].GetFloat();
            ecs.lightingSystem->ambientEquator.y = arr[1].GetFloat();
            ecs.lightingSystem->ambientEquator.z = arr[2].GetFloat();
        }

        // Ambient ground color
        if (lightingObj.HasMember("ambientGround") && lightingObj["ambientGround"].IsArray() && lightingObj["ambientGround"].Size() >= 3) {
            const auto& arr = lightingObj["ambientGround"];
            ecs.lightingSystem->ambientGround.x = arr[0].GetFloat();
            ecs.lightingSystem->ambientGround.y = arr[1].GetFloat();
            ecs.lightingSystem->ambientGround.z = arr[2].GetFloat();
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

    // NOTE: Tags, layers, and sorting layers are now loaded from project-wide settings
    // (TagsLayersSettings.hpp) and are NOT loaded from scene files anymore.
    // Old scene files may still contain these, but they will be ignored.

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        ENGINE_LOG_WARN("[CreateEntitiesFromJson] no entities array in JSON: " + tempScenePath);
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetECSManager(currentScenePath);

    // Destroy all entities (DAMN UNOPTIMISED BUT FIX NEXT TIME).
    ecs.ClearAllEntities();

    const rapidjson::Value& ents = doc["entities"];
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) continue;

        //Entity currEnt = entObj["id"].GetUint();

        //if (!entObj.HasMember("components") || !entObj["components"].IsObject()) continue;
        //const rapidjson::Value& comps = entObj["components"];

        //std::string entityGuidStr;
        //if (entObj.HasMember("guid") && entObj["guid"].IsString()) {
        //    entityGuidStr = entObj["guid"].GetString();
        //}

        //GUID_128 entityGuid = GUIDUtilities::ConvertStringToGUID128(entityGuidStr);
        //Entity existingEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(entityGuid);

        //auto entities = ecs.GetAllEntities(); // copy once
        //bool entityExists = (existingEntity != static_cast<Entity>(-1)) &&
        //    std::find(entities.begin(), entities.end(), existingEntity) != entities.end();

        //if (!entityExists) {
        //    Entity newEnt = ecs.CreateEntityWithGUID(entityGuid);
        //    currEnt = newEnt;

        //    if (!ecs.HasComponent<NameComponent>(newEnt)) {
        //        ecs.AddComponent<NameComponent>(newEnt, NameComponent{});
        //    }
        //    if (!ecs.HasComponent<ActiveComponent>(newEnt)) {
        //        ecs.AddComponent<ActiveComponent>(newEnt, ActiveComponent{});
        //    }
        //    if (!ecs.HasComponent<TagComponent>(newEnt)) {
        //        ecs.AddComponent<TagComponent>(newEnt, TagComponent{ 0 });
        //    }
        //    if (!ecs.HasComponent<LayerComponent>(newEnt)) {
        //        ecs.AddComponent<LayerComponent>(newEnt, LayerComponent{ 0 });
        //    }
        //    if (!ecs.HasComponent<SiblingIndexComponent>(newEnt)) {
        //        ecs.AddComponent<SiblingIndexComponent>(newEnt, SiblingIndexComponent{ 0 });
        //    }
        //    if (!ecs.HasComponent<Transform>(newEnt)) {
        //        ecs.AddComponent<Transform>(newEnt, Transform{});
        //    }
        //}
        //else {
        //    currEnt = existingEntity;
        //}

        DeserializeEntity(ecs, entObj, false);

        //// NameComponent
        //if (comps.HasMember("NameComponent")) {
        //    const rapidjson::Value& nv = comps["NameComponent"];
        //    auto& nameComp = ecs.GetComponent<NameComponent>(currEnt);
        //    DeserializeNameComponent(nameComp, nv);
        //}

        //// Transform
        //if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
        //    const rapidjson::Value& t = comps["Transform"];
        //    DeserializeTransformComponent(currEnt, t);
        //}

        //// PrefabLinkComponent
        //if (comps.HasMember("PrefabLinkComponent") && comps["PrefabLinkComponent"].IsObject()) {
        //    const auto& prefabCompJSON = comps["PrefabLinkComponent"];
        //    ecs.AddComponent<PrefabLinkComponent>(currEnt, PrefabLinkComponent{});
        //    auto& prefabComp = ecs.GetComponent<PrefabLinkComponent>(currEnt);
        //    DeserializePrefabLinkComponent(prefabComp, prefabCompJSON);

        //    InstantiatePrefabIntoEntity(prefabComp.prefabPath, currEnt);
        //    return;
        //}

        //// TagComponent
        //if (comps.HasMember("TagComponent")) {
        //    const rapidjson::Value& tv = comps["TagComponent"];
        //    auto& tagComp = ecs.GetComponent<TagComponent>(currEnt);
        //    DeserializeTagComponent(tagComp, tv);
        //}

        //// LayerComponent
        //if (comps.HasMember("LayerComponent")) {
        //    const rapidjson::Value& lv = comps["LayerComponent"];
        //    auto& layerComp = ecs.GetComponent<LayerComponent>(currEnt);
        //    DeserializeLayerComponent(layerComp, lv);
        //}

        //// SiblingIndexComponent
        //if (comps.HasMember("SiblingIndexComponent")) {
        //    const rapidjson::Value& sv = comps["SiblingIndexComponent"];
        //    if (!ecs.HasComponent<SiblingIndexComponent>(currEnt)) {
        //        ecs.AddComponent<SiblingIndexComponent>(currEnt, SiblingIndexComponent{});
        //    }
        //    auto& siblingComp = ecs.GetComponent<SiblingIndexComponent>(currEnt);
        //    DeserializeSiblingIndexComponent(siblingComp, sv);
        //}

        //// ParentComponent
        //if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
        //    const auto& parentCompJSON = comps["ParentComponent"];
        //    if (!ecs.HasComponent<ParentComponent>(currEnt)) {
        //        ecs.AddComponent<ParentComponent>(currEnt, ParentComponent{});
        //    }
        //    auto& parentComp = ecs.GetComponent<ParentComponent>(currEnt);
        //    DeserializeParentComponent(parentComp, parentCompJSON);
        //}

        //// ChildrenComponent
        //if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
        //    const auto& childrenCompJSON = comps["ChildrenComponent"];
        //    if (!ecs.HasComponent<ChildrenComponent>(currEnt)) {
        //        ecs.AddComponent<ChildrenComponent>(currEnt, ChildrenComponent{});
        //    }
        //    auto& childComp = ecs.GetComponent<ChildrenComponent>(currEnt);
        //    childComp.children.clear();
        //    DeserializeChildrenComponent(childComp, childrenCompJSON);
        //}

        //// ModelRenderComponent
        //if (comps.HasMember("ModelRenderComponent")) {
        //    const rapidjson::Value& mv = comps["ModelRenderComponent"];
        //    if (!ecs.HasComponent<ModelRenderComponent>(currEnt)) {
        //        ecs.AddComponent<ModelRenderComponent>(currEnt, ModelRenderComponent{});
        //    }
        //    auto& modelComp = ecs.GetComponent<ModelRenderComponent>(currEnt);
        //    DeserializeModelComponent(modelComp, mv, currEnt);
        //}

        //// SpriteRenderComponent
        //if (comps.HasMember("SpriteRenderComponent")) {
        //    const rapidjson::Value& mv = comps["SpriteRenderComponent"];
        //    if (!ecs.HasComponent<SpriteRenderComponent>(currEnt)) {
        //        ecs.AddComponent<SpriteRenderComponent>(currEnt, SpriteRenderComponent{});
        //    }
        //    auto& spriteComp = ecs.GetComponent<SpriteRenderComponent>(currEnt);
        //    DeserializeSpriteComponent(spriteComp, mv);
        //}

        //// SpriteAnimationComponent
        //if (comps.HasMember("SpriteAnimationComponent") && comps["SpriteAnimationComponent"].IsObject()) {
        //    const rapidjson::Value& mv = comps["SpriteAnimationComponent"];
        //    if (!ecs.HasComponent<SpriteAnimationComponent>(currEnt)) {
        //        ecs.AddComponent<SpriteAnimationComponent>(currEnt, SpriteAnimationComponent{});
        //    }
        //    auto& animComp = ecs.GetComponent<SpriteAnimationComponent>(currEnt);
        //    DeserializeSpriteAnimationComponent(animComp, mv);
        //}

        //// TextRenderComponent
        //if (comps.HasMember("TextRenderComponent") && comps["TextRenderComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["TextRenderComponent"];
        //    if (!ecs.HasComponent<TextRenderComponent>(currEnt)) {
        //        ecs.AddComponent<TextRenderComponent>(currEnt, TextRenderComponent{});
        //    }
        //    auto& textComp = ecs.GetComponent<TextRenderComponent>(currEnt);
        //    DeserializeTextComponent(textComp, tv);
        //}

        //// ParticleComponent
        //if (comps.HasMember("ParticleComponent") && comps["ParticleComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["ParticleComponent"];
        //    auto& particleComp = ecs.GetComponent<ParticleComponent>(currEnt);
        //    DeserializeParticleComponent(particleComp, tv);
        //}

        //// DirectionalLightComponent
        //if (comps.HasMember("DirectionalLightComponent") && comps["DirectionalLightComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["DirectionalLightComponent"];
        //    auto& dirLightComp = ecs.GetComponent<DirectionalLightComponent>(currEnt);
        //    DeserializeDirLightComponent(dirLightComp, tv);
        //}

        //// SpotLightComponent
        //if (comps.HasMember("SpotLightComponent") && comps["SpotLightComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["SpotLightComponent"];
        //    auto& spotlightComp = ecs.GetComponent<SpotLightComponent>(currEnt);
        //    DeserializeSpotLightComponent(spotlightComp, tv);
        //}

        //// PointLightComponent
        //if (comps.HasMember("PointLightComponent") && comps["PointLightComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["PointLightComponent"];
        //    auto& pointLightComp = ecs.GetComponent<PointLightComponent>(currEnt);
        //    DeserializePointLightComponent(pointLightComp, tv);
        //}

        //// AudioComponent
        //if (comps.HasMember("AudioComponent") && comps["AudioComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["AudioComponent"];
        //    auto& audioComp = ecs.GetComponent<AudioComponent>(currEnt);
        //    DeserializeAudioComponent(audioComp, tv);
        //}

        //// AudioListenerComponent
        //if (comps.HasMember("AudioListenerComponent") && comps["AudioListenerComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["AudioListenerComponent"];
        //    auto& audioListenerComp = ecs.GetComponent<AudioListenerComponent>(currEnt);
        //    DeserializeAudioListenerComponent(audioListenerComp, tv);
        //}

        //// AudioReverbZoneComponent
        //if (comps.HasMember("AudioReverbZoneComponent") && comps["AudioReverbZoneComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["AudioReverbZoneComponent"];
        //    auto& audioReverbZoneComp = ecs.GetComponent<AudioReverbZoneComponent>(currEnt);
        //    DeserializeAudioReverbZoneComponent(audioReverbZoneComp, tv);
        //}

        //// RigidBodyComponent
        //if (comps.HasMember("RigidBodyComponent") && comps["RigidBodyComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["RigidBodyComponent"];
        //    auto& rbComp = ecs.GetComponent<RigidBodyComponent>(currEnt);
        //    DeserializeRigidBodyComponent(rbComp, tv);
        //}

        //// ColliderComponent
        //if (comps.HasMember("ColliderComponent") && comps["ColliderComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["ColliderComponent"];
        //    auto& colliderComp = ecs.GetComponent<ColliderComponent>(currEnt);
        //    DeserializeColliderComponent(colliderComp, tv);
        //}

        //// CameraComponent
        //if (comps.HasMember("CameraComponent") && comps["CameraComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["CameraComponent"];
        //    auto& cameraComp = ecs.GetComponent<CameraComponent>(currEnt);
        //    DeserializeCameraComponent(cameraComp, tv);
        //}

        //// AnimationComponent
        //if (comps.HasMember("AnimationComponent") && comps["AnimationComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["AnimationComponent"];
        //    auto& animComp = ecs.GetComponent<AnimationComponent>(currEnt);
        //    TypeResolver<AnimationComponent>::Get()->Deserialize(&animComp, tv);
        //}

        //// ActiveComponent
        //if (comps.HasMember("ActiveComponent") && comps["ActiveComponent"].IsObject()) {
        //    const rapidjson::Value& tv = comps["ActiveComponent"];
        //    auto& activeComp = ecs.GetComponent<ActiveComponent>(currEnt);
        //    DeserializeActiveComponent(activeComp, tv);
        //}

        //// ScriptComponent (engine-side) - use DeserializeScriptComponent for consistency
        //if (comps.HasMember("ScriptComponent") && comps["ScriptComponent"].IsObject()) {
        //    Serializer::DeserializeScriptComponent(currEnt, comps["ScriptComponent"]);
        //}
        //// BrainComponent
        //if (comps.HasMember("BrainComponent") && comps["BrainComponent"].IsObject()) {
        //    const auto& brainCompJSON = comps["BrainComponent"];
        //    auto& brainComp = ecs.GetComponent<BrainComponent>(currEnt);
        //    DeserializeBrainComponent(brainComp, brainCompJSON);
        //}
        //// ButtonComponent
        //if (comps.HasMember("ButtonComponent") && comps["ButtonComponent"].IsObject()) {
        //    const auto& buttonCompJSON = comps["ButtonComponent"];
        //    if (ecs.HasComponent<ButtonComponent>(currEnt)) {
        //        auto& buttonComp = ecs.GetComponent<ButtonComponent>(currEnt);
        //        DeserializeButtonComponent(buttonComp, buttonCompJSON);
        //    }
        //}
        //// SliderComponent
        //if (comps.HasMember("SliderComponent") && comps["SliderComponent"].IsObject()) {
        //    const auto& sliderCompJSON = comps["SliderComponent"];
        //    if (ecs.HasComponent<SliderComponent>(currEnt)) {
        //        auto& sliderComp = ecs.GetComponent<SliderComponent>(currEnt);
        //        DeserializeSliderComponent(sliderComp, sliderCompJSON);
        //    }
        //}
        //// PrefabLinkComponent
        //if (comps.HasMember("PrefabLinkComponent") && comps["PrefabLinkComponent"].IsObject()) {
        //    const auto& prefabCompJSON = comps["PrefabLinkComponent"];
        //    if (ecs.HasComponent<PrefabLinkComponent>(currEnt)) {
        //        auto& prefabComp = ecs.GetComponent<PrefabLinkComponent>(currEnt);
        //        DeserializePrefabLinkComponent(prefabComp, prefabCompJSON);
        //    }
        //}

        //// Ensure all entities have TagComponent and LayerComponent
        //if (!ecs.HasComponent<TagComponent>(currEnt)) {
        //    ecs.AddComponent<TagComponent>(currEnt, TagComponent{ 0 });
        //}
        //if (!ecs.HasComponent<LayerComponent>(currEnt)) {
        //    ecs.AddComponent<LayerComponent>(currEnt, LayerComponent{ 0 });
        //}

    } // end for entities

    // Rebuild boneNameToEntityMap for all ModelRenderComponents after all entities are created
    // This is necessary because when skipSpawnChildren is true, the map isn't populated during
    // DeserializeModelComponent, but we need it for animation and rendering to work correctly
    for (const auto& entity : ecs.GetAllEntities()) {
        if (ecs.HasComponent<ModelRenderComponent>(entity)) {
            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
            if (modelComp.model) {
                modelComp.boneNameToEntityMap.clear();
                modelComp.boneNameToEntityMap[modelComp.model->modelName] = entity;
                ModelFactory::PopulateBoneNameToEntityMap(entity, modelComp.boneNameToEntityMap, *modelComp.model);
            }
        }
    }

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

GUID_128 Serializer::DeserializeEntityGUID(const rapidjson::Value& entityJSON) {
    if (entityJSON.HasMember("guid")) {
        GUID_string guidStr = entityJSON["guid"].GetString();
        GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
        return guid;
    }

    return GUID_128{};
}

Entity Serializer::CreateEntityViaGUID(const rapidjson::Value& entityJSON) {
    GUID_128 guid = DeserializeEntityGUID(entityJSON);
    if (guid != GUID_128{}) {
        Entity newEnt = ECSRegistry::GetInstance().GetActiveECSManager().CreateEntityWithGUID(guid);
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
    Quaternion quat;
    bool hasQuaternion = false;

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
                // Directly use quaternion instead of converting to Euler and back
                quat = Quaternion(static_cast<float>(w), static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                hasQuaternion = true;
            }
        }
    }

    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs.transformSystem) {
        ecs.transformSystem->SetLocalPosition(newEnt, pos);

        // Use quaternion directly if available, otherwise convert from Euler
        if (hasQuaternion) {
            // Directly set the quaternion to avoid conversion errors
            if (ecs.HasComponent<Transform>(newEnt)) {
                auto& transform = ecs.GetComponent<Transform>(newEnt);
                transform.localRotation = quat;
                transform.localRotation.Normalize(); // Ensure normalized
            }
        }
        else {
            ecs.transformSystem->SetLocalRotation(newEnt, rot); // expects Euler degrees
        }

        ecs.transformSystem->SetLocalScale(newEnt, scale);
    }
    else
    {
        Transform tf;
        tf.localPosition = pos;
        tf.localRotation = hasQuaternion ? quat : Quaternion::FromEulerDegrees(rot);
        tf.localRotation.Normalize();
        tf.localScale = scale;
    }
}

void Serializer::DeserializeModelComponent(ModelRenderComponent& modelComp, const rapidjson::Value& modelJSON, Entity root, bool skipSpawnChildren) {
    if (modelJSON.IsObject()) {
        if (modelJSON.HasMember("data") && modelJSON["data"].IsArray() && modelJSON["data"].Size() > 0) {
            const auto& d = modelJSON["data"];

            int startIdx = 0;
            // Check if we have base class fields or not
            if (d[0].IsObject() && d[0].HasMember("type")) {
                if (d[0]["type"].GetString() == std::string("bool")) {
                    // Check if we have complete base class fields
                    modelComp.isVisible = Serializer::GetBool(d, 0, true);

                    // Check second element for renderOrder
                    if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                        if (d[1]["type"].GetString() == std::string("int")) {
                            modelComp.renderOrder = Serializer::GetInt(d, 1, 100);
                            startIdx = 2;
                        }
                        else {
                            // d[1] is not renderOrder, so only isVisible present
                            modelComp.renderOrder = 100;  // default
                            startIdx = 1;
                        }
                    }
                    else if (d.Size() > 1 && d[1].IsString()) {
                        // d[1] is a string (probably GUID), so only isVisible present
                        modelComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                    else {
                        // Only one element or d[1] is something else
                        modelComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                }
                else {
                    // No base class fields - old format from snapshot
                    modelComp.isVisible = true;  // default
                    modelComp.renderOrder = 100;  // default
                    startIdx = 0;
                }
            }
            else if (d[0].IsString()) {
                // Old format - starts with GUID string
                modelComp.isVisible = true;  // default
                modelComp.renderOrder = 100;  // default
                startIdx = 0;
            }
            else {
                modelComp.isVisible = true;  // default
                modelComp.renderOrder = 100;  // default
                startIdx = 0;
            }

            // Component-specific fields
            GUID_string modelGUIDStr = extractGUIDString(d[startIdx]);
            GUID_string shaderGUIDStr = extractGUIDString(d[startIdx + 1]);
            GUID_string materialGUIDStr = extractGUIDString(d[startIdx + 2]);
            modelComp.modelGUID = GUIDUtilities::ConvertStringToGUID128(modelGUIDStr);
            modelComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(shaderGUIDStr);
            modelComp.materialGUID = GUIDUtilities::ConvertStringToGUID128(materialGUIDStr);

            // Load the actual resources from GUIDs
            if (modelComp.modelGUID.high != 0 || modelComp.modelGUID.low != 0) {
                std::string modelPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.modelGUID);
                if (!modelPath.empty()) {
                    modelComp.model = ResourceManager::GetInstance().GetResourceFromGUID<Model>(modelComp.modelGUID, modelPath);
                }
            }
            if (modelComp.shaderGUID.high != 0 || modelComp.shaderGUID.low != 0) {
                std::string shaderPath = ResourceManager::GetInstance().GetPlatformShaderPath("default");
                if (!shaderPath.empty()) {
                    modelComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(modelComp.shaderGUID, shaderPath);
                }
            }
            if (modelComp.materialGUID.high != 0 || modelComp.materialGUID.low != 0) {
                std::string materialPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.materialGUID);
                if (!materialPath.empty()) {
                    modelComp.material = ResourceManager::GetInstance().GetResourceFromGUID<Material>(modelComp.materialGUID, materialPath);
                }
            }

            modelComp.childBonesSaved = Serializer::GetBool(d, 5, false);

            if (modelComp.model) {
                modelComp.boneNameToEntityMap[modelComp.model->modelName] = root;
                // Only spawn children if:
                // 1. childBonesSaved is false (children weren't serialized)
                // 2. We're not in ReloadScene mode (skipSpawnChildren is false)
                // During ReloadScene, all entities including children are in the JSON,
                // so we must not spawn them here to avoid duplication.
                if (!modelComp.childBonesSaved && !skipSpawnChildren) {
                    ModelFactory::SpawnModelNode(modelComp.model->rootNode, MAX_ENTITIES, modelComp.boneNameToEntityMap, root);
                }
            }
        }
    }
}

void Serializer::DeserializeSpriteComponent(SpriteRenderComponent& spriteComp, const rapidjson::Value& spriteJSON) {
    if (spriteJSON.IsObject()) {
        if (spriteJSON.HasMember("data") && spriteJSON["data"].IsArray() && spriteJSON["data"].Size() > 0) {
            const auto& d = spriteJSON["data"];

            int startIdx = 0;
            // Check if we have base class fields or not
            if (d[0].IsObject() && d[0].HasMember("type")) {
                if (d[0]["type"].GetString() == std::string("bool")) {
                    // Has isVisible field
                    spriteComp.isVisible = Serializer::GetBool(d, 0, true);

                    // Check second element for renderOrder
                    if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                        if (d[1]["type"].GetString() == std::string("int")) {
                            spriteComp.renderOrder = Serializer::GetInt(d, 1, 100);
                            startIdx = 2;
                        }
                        else {
                            spriteComp.renderOrder = 100;  // default
                            startIdx = 1;
                        }
                    }
                    else if (d.Size() > 1 && d[1].IsString()) {
                        spriteComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                    else {
                        spriteComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                }
                else {
                    // No base class fields
                    spriteComp.isVisible = true;  // default
                    spriteComp.renderOrder = 100;  // default
                    startIdx = 0;
                }
            }
            else if (d[0].IsString()) {
                // Old format - starts with GUID string
                spriteComp.isVisible = true;  // default
                spriteComp.renderOrder = 100;  // default
                startIdx = 0;
            }
            else {
                spriteComp.isVisible = true;  // default
                spriteComp.renderOrder = 100;  // default
                startIdx = 0;
            }

            // Component-specific fields
            GUID_string textureGUIDStr = extractGUIDString(d[startIdx]);
            GUID_string shaderGUIDStr = extractGUIDString(d[startIdx + 1]);
            spriteComp.textureGUID = GUIDUtilities::ConvertStringToGUID128(textureGUIDStr);
            spriteComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(shaderGUIDStr);

            // Load the actual resources from GUIDs
            if (spriteComp.textureGUID.high != 0 || spriteComp.textureGUID.low != 0) {
                std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteComp.textureGUID);
                if (!texturePath.empty()) {
                    spriteComp.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(spriteComp.textureGUID, texturePath);
                }
            }
            if (spriteComp.shaderGUID.high != 0 || spriteComp.shaderGUID.low != 0) {
                std::string shaderPath = ResourceManager::GetInstance().GetPlatformShaderPath("sprite");
                if (!shaderPath.empty()) {
                    spriteComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(spriteComp.shaderGUID, shaderPath);
                }
            }

            // Sprite position and other fields
            readVec3Generic(d[startIdx + 2], spriteComp.position);
            // Sprite scale
            readVec3Generic(d[startIdx + 3], spriteComp.scale);
            // Sprite rotation
            spriteComp.rotation = Serializer::GetFloat(d, startIdx + 4);
            // Sprite color
            readVec3Generic(d[startIdx + 5], spriteComp.color);
            spriteComp.alpha = Serializer::GetFloat(d, startIdx + 6);
            spriteComp.is3D = Serializer::GetBool(d, startIdx + 7);
            spriteComp.enableBillboard = Serializer::GetBool(d, startIdx + 8);

            // Backward compatibility: old scenes have "layer", new scenes have "sortingLayer" and "sortingOrder"
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 9)) {
                // Load old "layer" field into sortingLayer for backward compatibility
                spriteComp.sortingLayer = Serializer::GetInt(d, startIdx + 9);
            }
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 10) &&
                d[startIdx + 10].IsObject() && d[startIdx + 10].HasMember("type")) {
                // Check if this is sortingOrder (int) or saved3DPosition (vector)
                std::string fieldType = d[startIdx + 10]["type"].GetString();
                if (fieldType == "int") {
                    // New format with sortingOrder
                    spriteComp.sortingOrder = Serializer::GetInt(d, startIdx + 10);
                    if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 11)) {
                        readVec3Generic(d[startIdx + 11], spriteComp.saved3DPosition);
                    }
                }
                else {
                    // Old format - this is saved3DPosition
                    readVec3Generic(d[startIdx + 10], spriteComp.saved3DPosition);
                }
            }
        }
    }
}

void Serializer::DeserializeSpriteAnimationComponent(SpriteAnimationComponent& animComp, const rapidjson::Value& animJSON) {
    if (!animJSON.IsObject()) return;

    // Clear existing data
    animComp.clips.clear();

    // Deserialize clips
    if (animJSON.HasMember("clips") && animJSON["clips"].IsArray()) {
        const auto& clipsArray = animJSON["clips"];
        for (rapidjson::SizeType i = 0; i < clipsArray.Size(); i++) {
            const auto& clipObj = clipsArray[i];
            if (!clipObj.IsObject()) continue;

            SpriteAnimationClip clip;

            // Name and loop
            if (clipObj.HasMember("name") && clipObj["name"].IsString()) {
                clip.name = clipObj["name"].GetString();
            }
            if (clipObj.HasMember("loop") && clipObj["loop"].IsBool()) {
                clip.loop = clipObj["loop"].GetBool();
            }

            // Deserialize frames with UV coordinates
            if (clipObj.HasMember("frames") && clipObj["frames"].IsArray()) {
                const auto& framesArray = clipObj["frames"];
                for (rapidjson::SizeType j = 0; j < framesArray.Size(); j++) {
                    const auto& frameObj = framesArray[j];
                    if (!frameObj.IsObject()) continue;

                    SpriteFrame frame;

                    // Texture GUID
                    if (frameObj.HasMember("textureGUID") && frameObj["textureGUID"].IsString()) {
                        std::string guidStr = frameObj["textureGUID"].GetString();
                        frame.textureGUID = GUIDUtilities::ConvertStringToGUID128(guidStr);
                    }

                    // Texture path
                    if (frameObj.HasMember("texturePath") && frameObj["texturePath"].IsString()) {
                        frame.texturePath = frameObj["texturePath"].GetString();
                    }

                    // UV Offset - MANUAL DESERIALIZE
                    if (frameObj.HasMember("uvOffset") && frameObj["uvOffset"].IsArray() && frameObj["uvOffset"].Size() >= 2) {
                        frame.uvOffset.x = static_cast<float>(frameObj["uvOffset"][0].GetDouble());
                        frame.uvOffset.y = static_cast<float>(frameObj["uvOffset"][1].GetDouble());
                    }

                    // UV Scale - MANUAL DESERIALIZE
                    if (frameObj.HasMember("uvScale") && frameObj["uvScale"].IsArray() && frameObj["uvScale"].Size() >= 2) {
                        frame.uvScale.x = static_cast<float>(frameObj["uvScale"][0].GetDouble());
                        frame.uvScale.y = static_cast<float>(frameObj["uvScale"][1].GetDouble());
                    }

                    // Duration
                    if (frameObj.HasMember("duration") && frameObj["duration"].IsNumber()) {
                        frame.duration = static_cast<float>(frameObj["duration"].GetDouble());
                    }

                    clip.frames.push_back(frame);
                }
            }

            animComp.clips.push_back(clip);
        }
    }

    // Other fields
    if (animJSON.HasMember("currentClipIndex") && animJSON["currentClipIndex"].IsInt()) {
        animComp.currentClipIndex = animJSON["currentClipIndex"].GetInt();
    }
    if (animJSON.HasMember("currentFrameIndex") && animJSON["currentFrameIndex"].IsInt()) {
        animComp.currentFrameIndex = animJSON["currentFrameIndex"].GetInt();
    }
    if (animJSON.HasMember("timeInCurrentFrame") && animJSON["timeInCurrentFrame"].IsNumber()) {
        animComp.timeInCurrentFrame = static_cast<float>(animJSON["timeInCurrentFrame"].GetDouble());
    }
    if (animJSON.HasMember("playbackSpeed") && animJSON["playbackSpeed"].IsNumber()) {
        animComp.playbackSpeed = static_cast<float>(animJSON["playbackSpeed"].GetDouble());
    }
    if (animJSON.HasMember("playing") && animJSON["playing"].IsBool()) {
        animComp.playing = animJSON["playing"].GetBool();
    }
    if (animJSON.HasMember("enabled") && animJSON["enabled"].IsBool()) {
        animComp.enabled = animJSON["enabled"].GetBool();
    }
    if (animJSON.HasMember("autoPlay") && animJSON["autoPlay"].IsBool()) {
        animComp.autoPlay = animJSON["autoPlay"].GetBool();
    }
}

void Serializer::DeserializeTextComponent(TextRenderComponent& textComp, const rapidjson::Value& textJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (textJSON.HasMember("data") && textJSON["data"].IsArray()) {
        const auto& d = textJSON["data"];

        int startIdx = 0;
        // Check if we have base class fields or not
        if (d[0].IsObject() && d[0].HasMember("type")) {
            if (d[0]["type"].GetString() == std::string("bool")) {
                // Has isVisible field
                textComp.isVisible = Serializer::GetBool(d, 0, true);

                // Check second element for renderOrder
                if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                    if (d[1]["type"].GetString() == std::string("int")) {
                        textComp.renderOrder = Serializer::GetInt(d, 1, 100);
                        startIdx = 2;
                    }
                    else {
                        textComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                }
                else if (d.Size() > 1 && d[1].IsString()) {
                    textComp.renderOrder = 100;  // default
                    startIdx = 1;
                }
                else {
                    textComp.renderOrder = 100;  // default
                    startIdx = 1;
                }
            }
            else {
                // No base class fields - old format from snapshot
                textComp.isVisible = true;  // default
                textComp.renderOrder = 100;  // default
                startIdx = 0;
            }
        }
        else {
            // Unknown format - set defaults
            textComp.isVisible = true;  // default
            textComp.renderOrder = 100;  // default
            startIdx = 0;
        }

        // Component-specific fields
        textComp.text = Serializer::GetString(d, startIdx);
        textComp.fontSize = static_cast<unsigned int>(Serializer::GetInt(d, startIdx + 1));

        // Use helper function to extract GUIDs
        GUID_string fontGUIDStr = extractGUIDString(d[startIdx + 2]);
        textComp.fontGUID = GUIDUtilities::ConvertStringToGUID128(fontGUIDStr);

        GUID_string shaderGUIDStr = extractGUIDString(d[startIdx + 3]);
        textComp.shaderGUID = GUIDUtilities::ConvertStringToGUID128(shaderGUIDStr);
        readVec3Generic(d[startIdx + 4], textComp.position);
        readVec3Generic(d[startIdx + 5], textComp.color);

        // Detect old vs new format by checking if index 6 is float (old scale) or bool (new is3D)
        bool hasOldScaleField = false;
        if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 6) &&
            d[startIdx + 6].IsObject() && d[startIdx + 6].HasMember("type") &&
            d[startIdx + 6]["type"].GetString() == std::string("float")) {
            // OLD format with scale field - read and discard it
            hasOldScaleField = true;
            // float oldScale = d[startIdx + 6]["data"].GetFloat();
        }

        if (hasOldScaleField) {
            // OLD format indices (with scale at 6)
            // Old order: text, fontSize, fontGUID, shaderGUID, position, color, scale, is3D, alignmentInt
            textComp.is3D = Serializer::GetBool(d, startIdx + 7);
            // sortingLayer and sortingOrder didn't exist in old format - use defaults
            textComp.sortingLayer = 0;
            textComp.sortingOrder = 0;
            textComp.alignmentInt = Serializer::GetInt(d, startIdx + 8);
        }
        else {
            // NEW format indices (without scale)
            // New order: text, fontSize, fontGUID, shaderGUID, position, color, is3D, sortingLayer, sortingOrder, transform, alignmentInt
            textComp.is3D = Serializer::GetBool(d, startIdx + 6);
            textComp.sortingLayer = Serializer::GetInt(d, startIdx + 7);
            textComp.sortingOrder = Serializer::GetInt(d, startIdx + 8);
            // Skip transform at index 9 (Matrix4x4 - not used, handled elsewhere)
            textComp.alignmentInt = Serializer::GetInt(d, startIdx + 10);

            // LINE WRAPPING PROPERTIES (indices 11, 12, 13)
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 11)) {
                textComp.wordWrap = Serializer::GetBool(d, startIdx + 11, false);
            }
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 12)) {
                textComp.maxWidth = Serializer::GetFloat(d, startIdx + 12, 0.0f);
            }
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 13)) {
                textComp.lineSpacing = Serializer::GetFloat(d, startIdx + 13, 1.2f);
            }
        }
    }
}

void Serializer::DeserializeParticleComponent(ParticleComponent& particleComp, const rapidjson::Value& particleJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (particleJSON.HasMember("data") && particleJSON["data"].IsArray()) {
        const auto& d = particleJSON["data"];

        int startIdx = 0;
        // Check if we have base class fields or not
        if (d[0].IsObject() && d[0].HasMember("type")) {
            if (d[0]["type"].GetString() == std::string("bool")) {
                // Check if we have complete base class fields
                particleComp.isVisible = Serializer::GetBool(d, 0, true);

                // Check second element for renderOrder
                if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                    if (d[1]["type"].GetString() == std::string("int")) {
                        particleComp.renderOrder = Serializer::GetInt(d, 1, 100);
                        startIdx = 2;
                    }
                    else {
                        // d[1] is not renderOrder, so only isVisible present
                        particleComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                }
                else if (d.Size() > 1 && d[1].IsString()) {
                    // d[1] is a string (probably GUID), so only isVisible present
                    particleComp.renderOrder = 100;  // default
                    startIdx = 1;
                }
                else {
                    // Only one element or d[1] is something else
                    particleComp.renderOrder = 100;  // default
                    startIdx = 1;
                }
            }
            else {
                // No base class fields - old format from snapshot
                particleComp.isVisible = true;  // default
                particleComp.renderOrder = 100;  // default
                startIdx = 0;
            }
        }
        else if (d[0].IsString()) {
            // Old format - starts with GUID string
            particleComp.isVisible = true;  // default
            particleComp.renderOrder = 100;  // default
            startIdx = 0;
        }
        else {
            particleComp.isVisible = true;  // default
            particleComp.renderOrder = 100;  // default
            startIdx = 0;
        }

        // Component-specific fields
        GUID_string guidStr = extractGUIDString(d[startIdx]);
        particleComp.textureGUID = GUIDUtilities::ConvertStringToGUID128(guidStr);

        // Rest of ParticleComponent fields
        readVec3Generic(d[startIdx + 1], particleComp.emitterPosition);
        particleComp.emissionRate = Serializer::GetFloat(d, startIdx + 2);
        particleComp.maxParticles = Serializer::GetInt(d, startIdx + 3);
        particleComp.particleLifetime = Serializer::GetFloat(d, startIdx + 4);
        particleComp.startSize = Serializer::GetFloat(d, startIdx + 5);
        particleComp.endSize = Serializer::GetFloat(d, startIdx + 6);
        readVec3Generic(d[startIdx + 7], particleComp.startColor);
        particleComp.startColorAlpha = Serializer::GetFloat(d, startIdx + 8);
        readVec3Generic(d[startIdx + 9], particleComp.endColor);
        particleComp.endColorAlpha = Serializer::GetFloat(d, startIdx + 10);
        readVec3Generic(d[startIdx + 11], particleComp.gravity);
        particleComp.velocityRandomness = Serializer::GetFloat(d, startIdx + 12);
        readVec3Generic(d[startIdx + 13], particleComp.initialVelocity);
    }
}

void Serializer::DeserializeDirLightComponent(DirectionalLightComponent& dirLightComp, const rapidjson::Value& dirLightJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (dirLightJSON.HasMember("data") && dirLightJSON["data"].IsArray()) {
        const auto& d = dirLightJSON["data"];
        readVec3Generic(d[0], dirLightComp.color);
        dirLightComp.intensity = Serializer::GetFloat(d, 1);
        dirLightComp.enabled = Serializer::GetBool(d, 2);
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
        spotlightComp.intensity = Serializer::GetFloat(d, 1);
        spotlightComp.enabled = Serializer::GetBool(d, 2);
        readVec3Generic(d[3], spotlightComp.direction);
        spotlightComp.cutOff = Serializer::GetFloat(d, 4);
        spotlightComp.constant = Serializer::GetFloat(d, 5);
        spotlightComp.linear = Serializer::GetFloat(d, 6);
        spotlightComp.quadratic = Serializer::GetFloat(d, 7);
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
        pointLightComp.intensity = Serializer::GetFloat(d, 1);
        pointLightComp.enabled = Serializer::GetBool(d, 2);
        pointLightComp.constant = Serializer::GetFloat(d, 3);
        pointLightComp.linear = Serializer::GetFloat(d, 4);
        pointLightComp.quadratic = Serializer::GetFloat(d, 5);
        readVec3Generic(d[6], pointLightComp.ambient);
        readVec3Generic(d[7], pointLightComp.diffuse);
        readVec3Generic(d[8], pointLightComp.specular);
        pointLightComp.castShadows = Serializer::GetBool(d, 9);
    }
}

void Serializer::DeserializeAudioComponent(AudioComponent& audioComp, const rapidjson::Value& audioJSON) {
    // typed form: tv.data = [ {type: "bool", data: true}, "GUID_string", {type: "std::string", data: "clip"}, ... ]
    if (audioJSON.HasMember("data") && audioJSON["data"].IsArray()) {
        const auto& d = audioJSON["data"];
        audioComp.enabled = Serializer::GetBool(d, 0);

        // Use helper function to extract audio GUID
        GUID_string guidStr = extractGUIDString(d[1]);
        audioComp.audioGUID = GUIDUtilities::ConvertStringToGUID128(guidStr);
        audioComp.Mute = Serializer::GetBool(d, 2);
        audioComp.bypassListenerEffects = Serializer::GetBool(d, 3);
        audioComp.PlayOnAwake = Serializer::GetBool(d, 4);
        audioComp.Loop = Serializer::GetBool(d, 5);
        audioComp.Priority = Serializer::GetInt(d, 6);
        audioComp.Volume = Serializer::GetFloat(d, 7);
        audioComp.Pitch = Serializer::GetFloat(d, 8);
        audioComp.StereoPan = Serializer::GetFloat(d, 9);
        audioComp.reverbZoneMix = Serializer::GetFloat(d, 10);
        audioComp.Spatialize = Serializer::GetBool(d, 11);
        audioComp.SpatialBlend = Serializer::GetFloat(d, 12);
        audioComp.DopplerLevel = Serializer::GetFloat(d, 13);
        audioComp.MinDistance = Serializer::GetFloat(d, 14);
        audioComp.MaxDistance = Serializer::GetFloat(d, 15);
        audioComp.OutputAudioMixerGroup = Serializer::GetString(d, 16);
    }
}

void Serializer::DeserializeAudioListenerComponent(AudioListenerComponent& audioListenerComp, const rapidjson::Value& audioListenerJSON) {
    // typed form: tv.data = [ {type: "bool", data: true} ]
    if (audioListenerJSON.HasMember("data") && audioListenerJSON["data"].IsArray()) {
        const auto& d = audioListenerJSON["data"];
        audioListenerComp.enabled = Serializer::GetBool(d, 0);
    }
}

void Serializer::DeserializeAudioReverbZoneComponent(AudioReverbZoneComponent& audioReverbZoneComp, const rapidjson::Value& audioReverbZoneJSON) {
    // typed form: tv.data = [ {type: "bool", data: true}, {type: "float", data: 10.0}, ... ]
    if (audioReverbZoneJSON.HasMember("data") && audioReverbZoneJSON["data"].IsArray()) {
        const auto& d = audioReverbZoneJSON["data"];
        audioReverbZoneComp.enabled = Serializer::GetBool(d, 0);
        audioReverbZoneComp.MinDistance = Serializer::GetFloat(d, 1);
        audioReverbZoneComp.MaxDistance = Serializer::GetFloat(d, 2);
        audioReverbZoneComp.reverbPresetIndex = Serializer::GetInt(d, 3);
        audioReverbZoneComp.decayTime = Serializer::GetFloat(d, 4);
        audioReverbZoneComp.earlyDelay = Serializer::GetFloat(d, 5);
        audioReverbZoneComp.lateDelay = Serializer::GetFloat(d, 6);
        audioReverbZoneComp.hfReference = Serializer::GetFloat(d, 7);
        audioReverbZoneComp.hfDecayRatio = Serializer::GetFloat(d, 8);
        audioReverbZoneComp.diffusion = Serializer::GetFloat(d, 9);
        audioReverbZoneComp.density = Serializer::GetFloat(d, 10);
        audioReverbZoneComp.lowShelfFrequency = Serializer::GetFloat(d, 11);
        audioReverbZoneComp.lowShelfGain = Serializer::GetFloat(d, 12);
        audioReverbZoneComp.highCut = Serializer::GetFloat(d, 13);
        audioReverbZoneComp.earlyLateMix = Serializer::GetFloat(d, 14);
        audioReverbZoneComp.wetLevel = Serializer::GetFloat(d, 15);
    }
}

void Serializer::DeserializeRigidBodyComponent(RigidBodyComponent& rbComp, const rapidjson::Value& rbJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (rbJSON.HasMember("data") && rbJSON["data"].IsArray()) {
        const auto& d = rbJSON["data"];
        rbComp.motionID = Serializer::GetInt(d, 1);
        rbComp.motion = static_cast<Motion>(rbComp.motionID);
        rbComp.ccd = Serializer::GetBool(d, 2);
        rbComp.transform_dirty = true;
        rbComp.motion_dirty = true;
        rbComp.collider_seen_version = 0;
    }
}

void Serializer::DeserializeColliderComponent(ColliderComponent& colliderComp, const rapidjson::Value& colliderJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (colliderJSON.HasMember("data") && colliderJSON["data"].IsArray()) {
        const auto& d = colliderJSON["data"];
        colliderComp.enabled = Serializer::GetBool(d, 0);
        colliderComp.layerID = Serializer::GetInt(d, 1);
        colliderComp.layer = static_cast<JPH::ObjectLayer>(colliderComp.layerID);
        colliderComp.version = static_cast<unsigned int>(Serializer::GetInt(d, 2));
        colliderComp.shapeTypeID = Serializer::GetInt(d, 3);
        colliderComp.shapeType = static_cast<ColliderShapeType>(colliderComp.shapeTypeID);
        readVec3Generic(d[4], colliderComp.boxHalfExtents);
        colliderComp.sphereRadius = Serializer::GetFloat(d, 5);
        colliderComp.capsuleRadius = Serializer::GetFloat(d, 6);
        colliderComp.capsuleHalfHeight = Serializer::GetFloat(d, 7);
        colliderComp.cylinderRadius = Serializer::GetFloat(d, 8);
        colliderComp.cylinderHalfHeight = Serializer::GetFloat(d, 9);
    }
}

void Serializer::DeserializeParentComponent(ParentComponent& parentComp, const rapidjson::Value& parentJSON, std::unordered_map<GUID_128, GUID_128>* guidRemap) {
    const auto& d = parentJSON["data"];

    // Use helper function to extract parent GUID
    GUID_string parentGUIDStr = extractGUIDString(d[0]);
	GUID_128 parentGUID = GUIDUtilities::ConvertStringToGUID128(parentGUIDStr);

    if (guidRemap == nullptr) {
        parentComp.parent = parentGUID;
    }
    else {
        if (guidRemap->find(parentGUID) != guidRemap->end()) {
			parentComp.parent = (*guidRemap)[parentGUID];
        }
        else {
            ENGINE_LOG_WARN("[Serializer] Parent GUID not found in remap during deserialization: " + parentGUIDStr);
        }
    }
}

void Serializer::DeserializeChildrenComponent(ChildrenComponent& childComp, const rapidjson::Value& _childJSON, std::unordered_map<GUID_128, GUID_128>* guidRemap) {
    if (_childJSON.HasMember("data")) {
        childComp.children.clear();
        const auto& childrenVectorJSON = _childJSON["data"][0]["data"].GetArray();
        for (const auto& childJSON : childrenVectorJSON) {
            // Use helper function to extract child GUIDs
            GUID_string childGUIDStr = extractGUIDString(childJSON);
			GUID_128 childGUID = GUIDUtilities::ConvertStringToGUID128(childGUIDStr);

            if (guidRemap == nullptr) {
                childComp.children.push_back(GUIDUtilities::ConvertStringToGUID128(childGUIDStr));
            }
            else {
                if (guidRemap->find(childGUID) != guidRemap->end()) {
                    childComp.children.push_back((*guidRemap)[childGUID]);
                }
                else {
                    ENGINE_LOG_WARN("[Serializer] Child GUID not found in remap during deserialization: " + childGUIDStr);
				}
            }
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

void Serializer::DeserializeSiblingIndexComponent(SiblingIndexComponent& siblingComp, const rapidjson::Value& siblingJSON) {
    if (siblingJSON.HasMember("siblingIndex") && siblingJSON["siblingIndex"].IsInt()) {
        siblingComp.siblingIndex = siblingJSON["siblingIndex"].GetInt();
    }
}

void Serializer::DeserializeCameraComponent(CameraComponent& cameraComp, const rapidjson::Value& cameraJSON) {
    if (cameraJSON.HasMember("data") && cameraJSON["data"].IsArray()) {
        const auto& d = cameraJSON["data"];

        rapidjson::SizeType idx = 0;
        cameraComp.enabled = Serializer::GetBool(d, idx++);
        cameraComp.isActive = Serializer::GetBool(d, idx++);
        cameraComp.priority = Serializer::GetInt(d, idx++);

        // Skip target and up in the old format (they're not in the reflection data array)
        // These are handled separately by custom serialization

        cameraComp.yaw = Serializer::GetFloat(d, idx++);
        cameraComp.pitch = Serializer::GetFloat(d, idx++);
        cameraComp.useFreeRotation = Serializer::GetBool(d, idx++);
        cameraComp.fov = Serializer::GetFloat(d, idx++);
        cameraComp.nearPlane = Serializer::GetFloat(d, idx++);
        cameraComp.farPlane = Serializer::GetFloat(d, idx++);
        cameraComp.orthoSize = Serializer::GetFloat(d, idx++);
        cameraComp.movementSpeed = Serializer::GetFloat(d, idx++);
        cameraComp.mouseSensitivity = Serializer::GetFloat(d, idx++);
        cameraComp.minZoom = Serializer::GetFloat(d, idx++);
        cameraComp.maxZoom = Serializer::GetFloat(d, idx++);
        cameraComp.zoomSpeed = Serializer::GetFloat(d, idx++);
        cameraComp.shakeIntensity = Serializer::GetFloat(d, idx++);
        cameraComp.shakeDuration = Serializer::GetFloat(d, idx++);
        cameraComp.shakeFrequency = Serializer::GetFloat(d, idx++);
        if (d.Size() > idx) {
            // Use helper function to extract skybox texture GUID
            GUID_string skyboxGUIDStr = extractGUIDString(d[idx]);
            idx++;
            cameraComp.skyboxTextureGUID = GUIDUtilities::ConvertStringToGUID128(skyboxGUIDStr);
        }
    }

    // Check if we have custom serialized target and up vectors
    if (cameraJSON.HasMember("target") && cameraJSON["target"].IsObject()) {
        const auto& targetObj = cameraJSON["target"];
        if (targetObj.HasMember("type") && targetObj["type"].GetString() == std::string("glm::vec3")) {
            if (targetObj.HasMember("data") && targetObj["data"].IsArray() && targetObj["data"].Size() >= 3) {
                const auto& vec = targetObj["data"];
                cameraComp.target.x = vec[0].GetFloat();
                cameraComp.target.y = vec[1].GetFloat();
                cameraComp.target.z = vec[2].GetFloat();
            }
        }
    }

    if (cameraJSON.HasMember("up") && cameraJSON["up"].IsObject()) {
        const auto& upObj = cameraJSON["up"];
        if (upObj.HasMember("type") && upObj["type"].GetString() == std::string("glm::vec3")) {
            if (upObj.HasMember("data") && upObj["data"].IsArray() && upObj["data"].Size() >= 3) {
                const auto& vec = upObj["data"];
                cameraComp.up.x = vec[0].GetFloat();
                cameraComp.up.y = vec[1].GetFloat();
                cameraComp.up.z = vec[2].GetFloat();
            }
        }
    }

    // Deserialize backgroundColor
    if (cameraJSON.HasMember("backgroundColor") && cameraJSON["backgroundColor"].IsObject()) {
        const auto& bgColorObj = cameraJSON["backgroundColor"];
        if (bgColorObj.HasMember("type") && bgColorObj["type"].GetString() == std::string("glm::vec3")) {
            if (bgColorObj.HasMember("data") && bgColorObj["data"].IsArray() && bgColorObj["data"].Size() >= 3) {
                const auto& vec = bgColorObj["data"];
                cameraComp.backgroundColor.x = vec[0].GetFloat();
                cameraComp.backgroundColor.y = vec[1].GetFloat();
                cameraComp.backgroundColor.z = vec[2].GetFloat();
            }
        }
    }

    // Deserialize clearFlags
    if (cameraJSON.HasMember("clearFlags") && cameraJSON["clearFlags"].IsInt()) {
        cameraComp.clearFlags = static_cast<CameraClearFlags>(cameraJSON["clearFlags"].GetInt());
    }

    // Deserialize projectionType
    if (cameraJSON.HasMember("projectionType") && cameraJSON["projectionType"].IsInt()) {
        cameraComp.projectionType = static_cast<ProjectionType>(cameraJSON["projectionType"].GetInt());
    }

    // Deserialize useSkybox
    if (cameraJSON.HasMember("useSkybox") && cameraJSON["useSkybox"].IsBool()) {
        cameraComp.useSkybox = cameraJSON["useSkybox"].GetBool();
    }

    // Deserialize skyboxTexturePath
    if (cameraJSON.HasMember("skyboxTexturePath") && cameraJSON["skyboxTexturePath"].IsString()) {
        cameraComp.skyboxTexturePath = cameraJSON["skyboxTexturePath"].GetString();
    }
}

// Helper function to deserialize a single script instance
static void DeserializeSingleScript(ScriptData& sd, const std::string& instJson) {
    try {
        int instId = Scripting::CreateInstanceFromFile(sd.scriptPath);
        if (Scripting::IsValidInstance(instId)) {
            sd.instanceId = instId;
            sd.instanceCreated = true;

            // register preserveKeys with runtime
            if (!sd.preserveKeys.empty()) {
                Scripting::RegisterInstancePreserveKeys(instId, sd.preserveKeys);
            }

            // deserialize instance state if available
            if (!instJson.empty()) {
                bool ok = Scripting::DeserializeJsonToInstance(instId, instJson);
                if (!ok) {
                    std::cerr << "[DeserializeSingleScript] Scripting::DeserializeJsonToInstance failed for " << sd.scriptPath << "\n";
                    // fallback: store pending state
                    sd.pendingInstanceState = instJson;
                }
                else {
                    // Successfully deserialized, clear any pending state
                    sd.pendingInstanceState = instJson;
                }
            }

            // optionally call entry function
            if (sd.autoInvokeEntry && !sd.entryFunction.empty()) {
                bool called = Scripting::CallInstanceFunction(instId, sd.entryFunction);
                if (!called) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "DeserializeSingleScript: entry call failed: ", sd.entryFunction.c_str());
                }
            }
        }
        else {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "DeserializeSingleScript: CreateInstanceFromFile failed for ", sd.scriptPath.c_str());
            // Store pending state for later
            if (!instJson.empty()) {
                sd.pendingInstanceState = instJson;
            }
            sd.instanceId = -1;
            sd.instanceCreated = false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[DeserializeSingleScript] exception: " << e.what() << "\n";
        // Store pending state for later
        if (!instJson.empty()) {
            sd.pendingInstanceState = instJson;
        }
        sd.instanceId = -1;
        sd.instanceCreated = false;
    }
    catch (...) {
        std::cerr << "[DeserializeSingleScript] unknown exception\n";
        // Store pending state for later
        if (!instJson.empty()) {
            sd.pendingInstanceState = instJson;
        }
        sd.instanceId = -1;
        sd.instanceCreated = false;
    }
}

void Serializer::DeserializeScriptComponent(Entity entity, const rapidjson::Value& scriptJSON) {
    // Ensure ECS manager exists
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // Add or get the engine-side ScriptComponentData for this entity
    if (!ecs.HasComponent<ScriptComponentData>(entity)) {
        ecs.AddComponent<ScriptComponentData>(entity, ScriptComponentData{});
    }
    auto& scriptComp = ecs.GetComponent<ScriptComponentData>(entity);

    // Check if this is the new format (has "scripts" array) or old format (single script fields)
    if (scriptJSON.HasMember("scripts") && scriptJSON["scripts"].IsArray()) {
        // NEW FORMAT: Multiple scripts
        const auto& scriptsArray = scriptJSON["scripts"];
        scriptComp.scripts.clear();

        for (rapidjson::SizeType i = 0; i < scriptsArray.Size(); ++i) {
            const auto& scriptData = scriptsArray[i];
            ScriptData sd{};

            // Read script fields
            if (scriptData.HasMember("scriptGuidStr") && scriptData["scriptGuidStr"].IsString()) {
                sd.scriptGuidStr = scriptData["scriptGuidStr"].GetString();
                if (!sd.scriptGuidStr.empty())
                    sd.scriptGuid = GUIDUtilities::ConvertStringToGUID128(sd.scriptGuidStr);
            }
            if (scriptData.HasMember("scriptPath") && scriptData["scriptPath"].IsString()) {
                sd.scriptPath = scriptData["scriptPath"].GetString();
#ifndef EDITOR
                // Game build: normalize paths saved by Editor (../../Resources/... -> Resources/...)
                if (sd.scriptPath.find("../../Resources") == 0) {
                    sd.scriptPath = sd.scriptPath.substr(6); // Remove "../../" prefix
                }
#endif
            }

            // If scriptPath is empty, try to resolve it from GUID
            if (sd.scriptPath.empty() && !sd.scriptGuidStr.empty()) {
                std::string resolvedPath = AssetManager::GetInstance().GetAssetPathFromGUID(sd.scriptGuid);
                if (!resolvedPath.empty()) {
                    // Extract relative path starting from "Resources"
                    size_t resPos = resolvedPath.find("Resources");
                    if (resPos != std::string::npos) {
                        sd.scriptPath = resolvedPath.substr(resPos);
                    }
                    else {
                        sd.scriptPath = resolvedPath;
                    }
                    ENGINE_PRINT("LOAD DEBUG: Resolved scriptPath from GUID: ", sd.scriptPath.c_str(), "\n");
                }
            }

            if (scriptData.HasMember("enabled") && scriptData["enabled"].IsBool()) {
                sd.enabled = scriptData["enabled"].GetBool();
            }
            if (scriptData.HasMember("entryFunction") && scriptData["entryFunction"].IsString()) {
                sd.entryFunction = scriptData["entryFunction"].GetString();
            }
            if (scriptData.HasMember("autoInvokeEntry") && scriptData["autoInvokeEntry"].IsBool()) {
                sd.autoInvokeEntry = scriptData["autoInvokeEntry"].GetBool();
            }

            // preserveKeys
            if (scriptData.HasMember("preserveKeys") && scriptData["preserveKeys"].IsArray()) {
                for (rapidjson::SizeType k = 0; k < scriptData["preserveKeys"].Size(); ++k) {
                    if (scriptData["preserveKeys"][k].IsString()) {
                        sd.preserveKeys.emplace_back(scriptData["preserveKeys"][k].GetString());
                    }
                }
            }

            // Extract instanceState
            std::string instJson;
            if (scriptData.HasMember("instanceState")) {
                rapidjson::StringBuffer buf;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
                scriptData["instanceState"].Accept(writer);
                instJson = buf.GetString();
            }
            else if (scriptData.HasMember("instanceStateRaw") && scriptData["instanceStateRaw"].IsString()) {
                instJson = scriptData["instanceStateRaw"].GetString();
            }

            // Try to create runtime instance if Lua is available
            if (!sd.scriptPath.empty() && Scripting::GetLuaState()) {
                DeserializeSingleScript(sd, instJson);
            }
            else if (!instJson.empty()) {
                // No runtime available - store pending state
                sd.pendingInstanceState = instJson;
            }

            scriptComp.scripts.push_back(sd);
        }
    }
    else {
        // OLD FORMAT: Single script (backward compatibility)
        ScriptData sd{};

        // Read simple fields
        if (scriptJSON.HasMember("scriptPath") && scriptJSON["scriptPath"].IsString()) {
            sd.scriptPath = scriptJSON["scriptPath"].GetString();
#ifndef EDITOR
            // Game build: normalize paths saved by Editor (../../Resources/... -> Resources/...)
            if (sd.scriptPath.find("../../Resources") == 0) {
                sd.scriptPath = sd.scriptPath.substr(6); // Remove "../../" prefix
            }
#endif
        }
        if (scriptJSON.HasMember("enabled") && scriptJSON["enabled"].IsBool()) {
            sd.enabled = scriptJSON["enabled"].GetBool();
        }
        if (scriptJSON.HasMember("entryFunction") && scriptJSON["entryFunction"].IsString()) {
            sd.entryFunction = scriptJSON["entryFunction"].GetString();
        }
        if (scriptJSON.HasMember("autoInvokeEntry") && scriptJSON["autoInvokeEntry"].IsBool()) {
            sd.autoInvokeEntry = scriptJSON["autoInvokeEntry"].GetBool();
        }

        // preserveKeys
        if (scriptJSON.HasMember("preserveKeys") && scriptJSON["preserveKeys"].IsArray()) {
            for (rapidjson::SizeType i = 0; i < scriptJSON["preserveKeys"].Size(); ++i) {
                if (scriptJSON["preserveKeys"][i].IsString()) {
                    sd.preserveKeys.emplace_back(scriptJSON["preserveKeys"][i].GetString());
                }
            }
        }

        // Extract instanceState
        std::string instJson;
        if (scriptJSON.HasMember("instanceState")) {
            rapidjson::StringBuffer buf;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
            scriptJSON["instanceState"].Accept(writer);
            instJson = buf.GetString();
        }
        else if (scriptJSON.HasMember("instanceStateRaw") && scriptJSON["instanceStateRaw"].IsString()) {
            instJson = scriptJSON["instanceStateRaw"].GetString();
        }

        // If scripting runtime is available, attempt to create instance and restore saved state (best-effort)
        if (!sd.scriptPath.empty() && Scripting::GetLuaState()) {
            DeserializeSingleScript(sd, instJson);
        }
        else if (!instJson.empty()) {
            // No runtime available - store pending state
            sd.pendingInstanceState = instJson;
        }

        // Add the single script to the scripts array (for backward compatibility)
        scriptComp.scripts.clear();
        if (!sd.scriptPath.empty()) {
            scriptComp.scripts.push_back(sd);
        }
    }
}

void Serializer::DeserializeActiveComponent(ActiveComponent& activeComp, const rapidjson::Value& activeJSON) {
    if (activeJSON.HasMember("data") && activeJSON["data"].IsArray()) {
        const auto& d = activeJSON["data"];
        activeComp.isActive = Serializer::GetBool(d, 0, true);
    }
}

void Serializer::DeserializeBrainComponent(BrainComponent& brainComp, const rapidjson::Value& brainJSON) {
    if (brainJSON.HasMember("data") && brainJSON["data"].IsArray()) {
        const auto& d = brainJSON["data"];
        brainComp.kindInt = Serializer::GetInt(d, 0);
        brainComp.kind = static_cast<BrainKind>(brainComp.kindInt);
        brainComp.started = false;
        brainComp.activeState = Serializer::GetString(d, 1);
        brainComp.enabled = Serializer::GetBool(d, 2);
    }
}

void Serializer::DeserializeButtonComponent(ButtonComponent& buttonComp, const rapidjson::Value& buttonJSON) {
    if (buttonJSON.HasMember("data") && buttonJSON["data"].IsArray()) {
        const auto& d = buttonJSON["data"];
        buttonComp.bindings.clear();

        if (d.Size() > 0 && d[0].HasMember("data") && d[0]["data"].IsArray()) {
            const auto& bindingsArr = d[0]["data"].GetArray();
            for (const auto& bindingJSON : bindingsArr) {
                ButtonBinding binding;
                if (bindingJSON.HasMember("data") && bindingJSON["data"].IsArray()) {
                    const auto& bd = bindingJSON["data"];

                    // Handle both old format (4 fields) and new format (5 fields with scriptPath)
                    // New format: 0: targetEntityGuidStr, 1: scriptPath, 2: scriptGuidStr, 3: functionName, 4: callWithSelf
                    // Old format: 0: targetEntityGuidStr, 1: scriptGuidStr, 2: functionName, 3: callWithSelf

                    if (bd.Size() >= 5) {
                        // New format with scriptPath
                        if (bd[0].HasMember("data") && bd[0]["data"].IsString())
                            binding.targetEntityGuidStr = bd[0]["data"].GetString();
                        if (bd[1].HasMember("data") && bd[1]["data"].IsString()) {
                            binding.scriptPath = bd[1]["data"].GetString();
#ifndef EDITOR
                            // Game build: normalize paths saved by Editor (../../Resources/... -> Resources/...)
                            if (binding.scriptPath.find("../../Resources") == 0) {
                                binding.scriptPath = binding.scriptPath.substr(6); // Remove "../../" prefix
                            }
#endif
                        }
                        if (bd[2].HasMember("data") && bd[2]["data"].IsString())
                            binding.scriptGuidStr = bd[2]["data"].GetString();
                        if (bd[3].HasMember("data") && bd[3]["data"].IsString())
                            binding.functionName = bd[3]["data"].GetString();
                        if (bd[4].HasMember("data") && bd[4]["data"].IsBool())
                            binding.callWithSelf = bd[4]["data"].GetBool();
                    }
                    else if (bd.Size() >= 4) {
                        // Old format without scriptPath - maintain backward compatibility
                        if (bd[0].HasMember("data") && bd[0]["data"].IsString())
                            binding.targetEntityGuidStr = bd[0]["data"].GetString();
                        if (bd[1].HasMember("data") && bd[1]["data"].IsString())
                            binding.scriptGuidStr = bd[1]["data"].GetString();
                        if (bd[2].HasMember("data") && bd[2]["data"].IsString())
                            binding.functionName = bd[2]["data"].GetString();
                        if (bd[3].HasMember("data") && bd[3]["data"].IsBool())
                            binding.callWithSelf = bd[3]["data"].GetBool();
                        // scriptPath will remain empty for old scenes
                    }
                }
                buttonComp.bindings.push_back(binding);
            }
        }

        buttonComp.interactable = Serializer::GetBool(d, 1, true);
    }
}

void Serializer::DeserializeSliderComponent(SliderComponent& sliderComp, const rapidjson::Value& sliderJSON) {
    if (sliderJSON.HasMember("data") && sliderJSON["data"].IsArray()) {
        const auto& d = sliderJSON["data"];
        sliderComp.onValueChanged.clear();

        // 0: onValueChanged
        if (d.Size() > 0 && d[0].HasMember("data") && d[0]["data"].IsArray()) {
            const auto& bindingsArr = d[0]["data"].GetArray();
            for (const auto& bindingJSON : bindingsArr) {
                SliderBinding binding;
                if (bindingJSON.HasMember("data") && bindingJSON["data"].IsArray()) {
                    const auto& bd = bindingJSON["data"];

                    if (bd.Size() >= 5) {
                        if (bd[0].HasMember("data") && bd[0]["data"].IsString())
                            binding.targetEntityGuidStr = bd[0]["data"].GetString();
                        if (bd[1].HasMember("data") && bd[1]["data"].IsString()) {
                            binding.scriptPath = bd[1]["data"].GetString();
#ifndef EDITOR
                            if (binding.scriptPath.find("../../Resources") == 0) {
                                binding.scriptPath = binding.scriptPath.substr(6); // Remove "../../" prefix
                            }
#endif
                        }
                        if (bd[2].HasMember("data") && bd[2]["data"].IsString())
                            binding.scriptGuidStr = bd[2]["data"].GetString();
                        if (bd[3].HasMember("data") && bd[3]["data"].IsString())
                            binding.functionName = bd[3]["data"].GetString();
                        if (bd[4].HasMember("data") && bd[4]["data"].IsBool())
                            binding.callWithSelf = bd[4]["data"].GetBool();
                    }
                }
                sliderComp.onValueChanged.push_back(binding);
            }
        }

        sliderComp.minValue = Serializer::GetFloat(d, 1);
        sliderComp.maxValue = Serializer::GetFloat(d, 2);
        sliderComp.value = Serializer::GetFloat(d, 3);
        sliderComp.wholeNumbers = Serializer::GetBool(d, 4);
        sliderComp.interactable = Serializer::GetBool(d, 5);
        sliderComp.horizontal = Serializer::GetBool(d, 6);

        // 7: trackEntityGuid
        if (d.Size() > 7) {
            std::string guidStr = extractGUIDString(d[7]);
            if (!guidStr.empty()) sliderComp.trackEntityGuid = GUIDUtilities::ConvertStringToGUID128(guidStr);
        }
        // 8: handleEntityGuid
        if (d.Size() > 8) {
            std::string guidStr = extractGUIDString(d[8]);
            if (!guidStr.empty()) sliderComp.handleEntityGuid = GUIDUtilities::ConvertStringToGUID128(guidStr);
        }
    }
}

void Serializer::DeserializePrefabLinkComponent(PrefabLinkComponent& prefabLinkComp, const rapidjson::Value& prefabLinkJSON) {
    if (prefabLinkJSON.HasMember("data") && prefabLinkJSON["data"].IsArray()) {
        const auto& d = prefabLinkJSON["data"];
#ifdef EDITOR
		prefabLinkComp.prefabPath = Serializer::GetString(d, 0);
#else
		// For game builds, we need to adjust the prefab path to be relative to Resources.
		std::string prefabPath = Serializer::GetString(d, 0);
		prefabPath = prefabPath.substr(prefabPath.find("Resources/")); // Remove any leading relative path
		prefabLinkComp.prefabPath = prefabPath;
#endif
    }
}