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
        if (ecs.HasComponent<AudioListenerComponent>(entity)) {
            auto& c = ecs.GetComponent<AudioListenerComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("AudioListenerComponent", v, alloc);
		}
        if (ecs.HasComponent<AudioReverbZoneComponent>(entity)) {
            auto& c = ecs.GetComponent<AudioReverbZoneComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("AudioReverbZoneComponent", v, alloc);
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
        if (ecs.HasComponent<CameraComponent>(entity)) {
            auto& c = ecs.GetComponent<CameraComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);

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
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("AnimationComponent", v, alloc);
        }
        if (ecs.HasComponent<ActiveComponent>(entity)) {
            auto& c = ecs.GetComponent<ActiveComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
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
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("BrainComponent", v, alloc);
        }
        if (ecs.HasComponent<ButtonComponent>(entity)) {
            auto& c = ecs.GetComponent<ButtonComponent>(entity);
            rapidjson::Value v = serializeComponentToValue(c);
            compsObj.AddMember("ButtonComponent", v, alloc);
        }

        entObj.AddMember("components", compsObj, alloc);
        entitiesArr.PushBack(entObj, alloc);
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

        // Ambient intensity
        lightingObj.AddMember("ambientIntensity", ecs.lightingSystem->ambientIntensity, alloc);

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

        // SiblingIndexComponent
        if (comps.HasMember("SiblingIndexComponent")) {
            const rapidjson::Value& sv = comps["SiblingIndexComponent"];
            ecs.AddComponent<SiblingIndexComponent>(newEnt, SiblingIndexComponent{});
            auto& siblingComp = ecs.GetComponent<SiblingIndexComponent>(newEnt);
            DeserializeSiblingIndexComponent(siblingComp, sv);
        }

        // Transform
        if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
            const rapidjson::Value& t = comps["Transform"];
            ecs.AddComponent<Transform>(newEnt, Transform{});
            DeserializeTransformComponent(newEnt, t);
        }

        // ParentComponent
        if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
            const auto& parentCompJSON = comps["ParentComponent"];
            if (!ecs.HasComponent<ParentComponent>(newEnt)) {
                ecs.AddComponent<ParentComponent>(newEnt, ParentComponent{});
            }
            auto& parentComp = ecs.GetComponent<ParentComponent>(newEnt);
            DeserializeParentComponent(parentComp, parentCompJSON);
        }

        // ChildrenComponent
        if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
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
            DeserializeModelComponent(modelComp, mv, newEnt);
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

        if (comps.HasMember("AnimationComponent") && comps["AnimationComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["AnimationComponent"];
            AnimationComponent animComp{};
            TypeResolver<AnimationComponent>::Get()->Deserialize(&animComp, tv);
            ecs.AddComponent<AnimationComponent>(newEnt, animComp);
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
            const auto&brainCompJSON = comps["BrainComponent"];
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

        // Ambient intensity
        if (lightingObj.HasMember("ambientIntensity") && lightingObj["ambientIntensity"].IsNumber()) {
            ecs.lightingSystem->ambientIntensity = lightingObj["ambientIntensity"].GetFloat();
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

    const rapidjson::Value& ents = doc["entities"];
    for (rapidjson::SizeType i = 0; i < ents.Size(); ++i) {
        const rapidjson::Value& entObj = ents[i];
        if (!entObj.IsObject()) continue;

        Entity currEnt = entObj["id"].GetUint();

        if (!entObj.HasMember("components") || !entObj["components"].IsObject()) continue;
        const rapidjson::Value& comps = entObj["components"];

        std::string entityGuidStr;
        if (entObj.HasMember("guid") && entObj["guid"].IsString()) {
            entityGuidStr = entObj["guid"].GetString();
        }

        GUID_128 entityGuid = GUIDUtilities::ConvertStringToGUID128(entityGuidStr);
        Entity existingEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(entityGuid);

        auto entities = ecs.GetAllEntities(); // copy once
        bool entityExists = (existingEntity != static_cast<Entity>(-1)) &&
                           std::find(entities.begin(), entities.end(), existingEntity) != entities.end();

        if (!entityExists) {
            Entity newEnt = ecs.CreateEntityWithGUID(entityGuid);
            currEnt = newEnt;

            if (!ecs.HasComponent<NameComponent>(newEnt)) {
                ecs.AddComponent<NameComponent>(newEnt, NameComponent{});
            }
            if (!ecs.HasComponent<ActiveComponent>(newEnt)) {
                ecs.AddComponent<ActiveComponent>(newEnt, ActiveComponent{});
            }
            if (!ecs.HasComponent<TagComponent>(newEnt)) {
                ecs.AddComponent<TagComponent>(newEnt, TagComponent{0});
            }
            if (!ecs.HasComponent<LayerComponent>(newEnt)) {
                ecs.AddComponent<LayerComponent>(newEnt, LayerComponent{0});
            }
            if (!ecs.HasComponent<SiblingIndexComponent>(newEnt)) {
                ecs.AddComponent<SiblingIndexComponent>(newEnt, SiblingIndexComponent{0});
            }
            if (!ecs.HasComponent<Transform>(newEnt)) {
                ecs.AddComponent<Transform>(newEnt, Transform{});
            }
        } else {
            currEnt = existingEntity;
        }

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

        // SiblingIndexComponent
        if (comps.HasMember("SiblingIndexComponent")) {
            const rapidjson::Value& sv = comps["SiblingIndexComponent"];
            if (!ecs.HasComponent<SiblingIndexComponent>(currEnt)) {
                ecs.AddComponent<SiblingIndexComponent>(currEnt, SiblingIndexComponent{});
            }
            auto& siblingComp = ecs.GetComponent<SiblingIndexComponent>(currEnt);
            DeserializeSiblingIndexComponent(siblingComp, sv);
        }

        // Transform
        if (comps.HasMember("Transform") && comps["Transform"].IsObject()) {
            const rapidjson::Value& t = comps["Transform"];
            DeserializeTransformComponent(currEnt, t);
        }

        // ParentComponent
        if (comps.HasMember("ParentComponent") && comps["ParentComponent"].IsObject()) {
            const auto& parentCompJSON = comps["ParentComponent"];
            if (!ecs.HasComponent<ParentComponent>(currEnt)) {
                ecs.AddComponent<ParentComponent>(currEnt, ParentComponent{});
            }
            auto& parentComp = ecs.GetComponent<ParentComponent>(currEnt);
            DeserializeParentComponent(parentComp, parentCompJSON);
        }

        // ChildrenComponent
        if (comps.HasMember("ChildrenComponent") && comps["ChildrenComponent"].IsObject()) {
            const auto& childrenCompJSON = comps["ChildrenComponent"];
            if (!ecs.HasComponent<ChildrenComponent>(currEnt)) {
                ecs.AddComponent<ChildrenComponent>(currEnt, ChildrenComponent{});
            }
            auto& childComp = ecs.GetComponent<ChildrenComponent>(currEnt);
            childComp.children.clear();
            DeserializeChildrenComponent(childComp, childrenCompJSON);
        }

        // ModelRenderComponent
        if (comps.HasMember("ModelRenderComponent")) {
            const rapidjson::Value& mv = comps["ModelRenderComponent"];
            if (!ecs.HasComponent<ModelRenderComponent>(currEnt)) {
                ecs.AddComponent<ModelRenderComponent>(currEnt, ModelRenderComponent{});
            }
            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(currEnt);
            DeserializeModelComponent(modelComp, mv, currEnt);
        }

        // SpriteRenderComponent
        if (comps.HasMember("SpriteRenderComponent")) {
            const rapidjson::Value& mv = comps["SpriteRenderComponent"];
            if (!ecs.HasComponent<SpriteRenderComponent>(currEnt)) {
                ecs.AddComponent<SpriteRenderComponent>(currEnt, SpriteRenderComponent{});
            }
            auto& spriteComp = ecs.GetComponent<SpriteRenderComponent>(currEnt);
            DeserializeSpriteComponent(spriteComp, mv);
        }

        // SpriteAnimationComponent
        if (comps.HasMember("SpriteAnimationComponent") && comps["SpriteAnimationComponent"].IsObject()) {
            const rapidjson::Value& mv = comps["SpriteAnimationComponent"];
            if (!ecs.HasComponent<SpriteAnimationComponent>(currEnt)) {
                ecs.AddComponent<SpriteAnimationComponent>(currEnt, SpriteAnimationComponent{});
            }
            auto& animComp = ecs.GetComponent<SpriteAnimationComponent>(currEnt);
            DeserializeSpriteAnimationComponent(animComp, mv);
        }

        // TextRenderComponent
        if (comps.HasMember("TextRenderComponent") && comps["TextRenderComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["TextRenderComponent"];
            if (!ecs.HasComponent<TextRenderComponent>(currEnt)) {
                ecs.AddComponent<TextRenderComponent>(currEnt, TextRenderComponent{});
            }
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

		// AudioListenerComponent
        if (comps.HasMember("AudioListenerComponent") && comps["AudioListenerComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["AudioListenerComponent"];
            auto& audioListenerComp = ecs.GetComponent<AudioListenerComponent>(currEnt);
			DeserializeAudioListenerComponent(audioListenerComp, tv);
        }

		// AudioReverbZoneComponent
        if (comps.HasMember("AudioReverbZoneComponent") && comps["AudioReverbZoneComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["AudioReverbZoneComponent"];
            auto& audioReverbZoneComp = ecs.GetComponent<AudioReverbZoneComponent>(currEnt);
			DeserializeAudioReverbZoneComponent(audioReverbZoneComp, tv);
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

        // CameraComponent
        if (comps.HasMember("CameraComponent") && comps["CameraComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["CameraComponent"];
            auto& cameraComp = ecs.GetComponent<CameraComponent>(currEnt);
            DeserializeCameraComponent(cameraComp, tv);
        }

        // AnimationComponent
        if (comps.HasMember("AnimationComponent") && comps["AnimationComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["AnimationComponent"];
            auto& animComp = ecs.GetComponent<AnimationComponent>(currEnt);
            TypeResolver<AnimationComponent>::Get()->Deserialize(&animComp, tv);
        }

        // ActiveComponent
        if (comps.HasMember("ActiveComponent") && comps["ActiveComponent"].IsObject()) {
            const rapidjson::Value& tv = comps["ActiveComponent"];
            auto& activeComp = ecs.GetComponent<ActiveComponent>(currEnt);
            DeserializeActiveComponent(activeComp, tv);
        }

        // ScriptComponent (engine-side) - use DeserializeScriptComponent for consistency
        if (comps.HasMember("ScriptComponent") && comps["ScriptComponent"].IsObject()) {
            Serializer::DeserializeScriptComponent(currEnt, comps["ScriptComponent"]);
        }
        // BrainComponent
        if (comps.HasMember("BrainComponent") && comps["BrainComponent"].IsObject()) {
            const auto& brainCompJSON = comps["BrainComponent"];
            auto& brainComp = ecs.GetComponent<BrainComponent>(currEnt);
            DeserializeBrainComponent(brainComp, brainCompJSON);
        }
        // ButtonComponent
        if (comps.HasMember("ButtonComponent") && comps["ButtonComponent"].IsObject()) {
            const auto& buttonCompJSON = comps["ButtonComponent"];
            if (ecs.HasComponent<ButtonComponent>(currEnt)) {
                auto& buttonComp = ecs.GetComponent<ButtonComponent>(currEnt);
                DeserializeButtonComponent(buttonComp, buttonCompJSON);
            }
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

    //THIS SHOULDNT BE HERE BUT NO CHOICE I GUESS OTHERWISE SCRIPTING DIES
    ECSRegistry::GetInstance().GetActiveECSManager().GetSystem<ScriptSystem>()->ReloadSystem();

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
        } else {
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

void Serializer::DeserializeModelComponent(ModelRenderComponent& modelComp, const rapidjson::Value& modelJSON, Entity root) {
    if (modelJSON.IsObject()) {
        if (modelJSON.HasMember("data") && modelJSON["data"].IsArray() && modelJSON["data"].Size() > 0) {
            const auto& d = modelJSON["data"];

            int startIdx = 0;
            // Check if we have base class fields or not
            if (d[0].IsObject() && d[0].HasMember("type")) {
                if (d[0]["type"].GetString() == std::string("bool")) {
                    // Check if we have complete base class fields
                    modelComp.isVisible = d[0]["data"].GetBool();

                    // Check second element for renderOrder
                    if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                        if (d[1]["type"].GetString() == std::string("int")) {
                            modelComp.renderOrder = d[1]["data"].GetInt();
                            startIdx = 2;
                        } else {
                            // d[1] is not renderOrder, so only isVisible present
                            modelComp.renderOrder = 100;  // default
                            startIdx = 1;
                        }
                    } else if (d.Size() > 1 && d[1].IsString()) {
                        // d[1] is a string (probably GUID), so only isVisible present
                        modelComp.renderOrder = 100;  // default
                        startIdx = 1;
                    } else {
                        // Only one element or d[1] is something else
                        modelComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                } else {
                    // No base class fields - old format from snapshot
                    modelComp.isVisible = true;  // default
                    modelComp.renderOrder = 100;  // default
                    startIdx = 0;
                }
            } else if (d[0].IsString()) {
                // Old format - starts with GUID string
                modelComp.isVisible = true;  // default
                modelComp.renderOrder = 100;  // default
                startIdx = 0;
            } else {
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

            // 1. Calculate the index first for clarity
            rapidjson::SizeType targetIndex = static_cast<rapidjson::SizeType>(5);

            // 2. Check if the index is within the bounds of the array 'd'
            if (targetIndex < d.Size()) {
                // Get a reference to the element to avoid repeated lookups
                const rapidjson::Value& element = d[targetIndex];

                // 3. Check if the element has the member "data" AND if it is a boolean
                if (element.HasMember("data") && element["data"].IsBool()) {
                    modelComp.childBonesSaved = element["data"].GetBool();
                }
            }

            modelComp.boneNameToEntityMap[modelComp.model->modelName] = root;
            if (!modelComp.childBonesSaved) {
                ModelFactory::SpawnModelNode(modelComp.model->rootNode, MAX_ENTITIES, modelComp.boneNameToEntityMap, root);
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
                    spriteComp.isVisible = d[0]["data"].GetBool();

                    // Check second element for renderOrder
                    if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                        if (d[1]["type"].GetString() == std::string("int")) {
                            spriteComp.renderOrder = d[1]["data"].GetInt();
                            startIdx = 2;
                        } else {
                            spriteComp.renderOrder = 100;  // default
                            startIdx = 1;
                        }
                    } else if (d.Size() > 1 && d[1].IsString()) {
                        spriteComp.renderOrder = 100;  // default
                        startIdx = 1;
                    } else {
                        spriteComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                } else {
                    // No base class fields
                    spriteComp.isVisible = true;  // default
                    spriteComp.renderOrder = 100;  // default
                    startIdx = 0;
                }
            } else if (d[0].IsString()) {
                // Old format - starts with GUID string
                spriteComp.isVisible = true;  // default
                spriteComp.renderOrder = 100;  // default
                startIdx = 0;
            } else {
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
            spriteComp.rotation = d[startIdx + 4]["data"].GetFloat();
            // Sprite color
            readVec3Generic(d[startIdx + 5], spriteComp.color);
            spriteComp.alpha = d[startIdx + 6]["data"].GetFloat();
            spriteComp.is3D = d[startIdx + 7]["data"].GetBool();
            spriteComp.enableBillboard = d[startIdx + 8]["data"].GetBool();

            // Backward compatibility: old scenes have "layer", new scenes have "sortingLayer" and "sortingOrder"
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 9)) {
                // Load old "layer" field into sortingLayer for backward compatibility
                spriteComp.sortingLayer = d[startIdx + 9]["data"].GetInt();
            }
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 10) &&
                d[startIdx + 10].IsObject() && d[startIdx + 10].HasMember("type")) {
                // Check if this is sortingOrder (int) or saved3DPosition (vector)
                std::string fieldType = d[startIdx + 10]["type"].GetString();
                if (fieldType == "int") {
                    // New format with sortingOrder
                    spriteComp.sortingOrder = d[startIdx + 10]["data"].GetInt();
                    if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 11)) {
                        readVec3Generic(d[startIdx + 11], spriteComp.saved3DPosition);
                    }
                } else {
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
                textComp.isVisible = d[0]["data"].GetBool();

                // Check second element for renderOrder
                if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                    if (d[1]["type"].GetString() == std::string("int")) {
                        textComp.renderOrder = d[1]["data"].GetInt();
                        startIdx = 2;
                    } else {
                        textComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                } else if (d.Size() > 1 && d[1].IsString()) {
                    textComp.renderOrder = 100;  // default
                    startIdx = 1;
                } else {
                    textComp.renderOrder = 100;  // default
                    startIdx = 1;
                }
            } else {
                // No base class fields - old format from snapshot
                textComp.isVisible = true;  // default
                textComp.renderOrder = 100;  // default
                startIdx = 0;
            }
        } else {
            // Unknown format - set defaults
            textComp.isVisible = true;  // default
            textComp.renderOrder = 100;  // default
            startIdx = 0;
        }

        // Component-specific fields
        textComp.text = d[startIdx]["data"].GetString();
        textComp.fontSize = d[startIdx + 1]["data"].GetUint();

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
            float oldScale = d[startIdx + 6]["data"].GetFloat();
            (void)oldScale; // Suppress unused variable warning
        }

        if (hasOldScaleField) {
            // OLD format indices (with scale at 6)
            // Old order: text, fontSize, fontGUID, shaderGUID, position, color, scale, is3D, alignmentInt
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 7)) {
                textComp.is3D = d[startIdx + 7]["data"].GetBool();
            }
            // sortingLayer and sortingOrder didn't exist in old format - use defaults
            textComp.sortingLayer = 0;
            textComp.sortingOrder = 0;
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 8)) {
                textComp.alignmentInt = d[startIdx + 8]["data"].GetInt();
            }
        } else {
            // NEW format indices (without scale)
            // New order: text, fontSize, fontGUID, shaderGUID, position, color, is3D, sortingLayer, sortingOrder, transform, alignmentInt
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 6)) {
                textComp.is3D = d[startIdx + 6]["data"].GetBool();
            }
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 7)) {
                textComp.sortingLayer = d[startIdx + 7]["data"].GetInt();
            }
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 8)) {
                textComp.sortingOrder = d[startIdx + 8]["data"].GetInt();
            }
            // Skip transform at index 9 (Matrix4x4 - not used, handled elsewhere)
            if (d.Size() > static_cast<rapidjson::SizeType>(startIdx + 10)) {
                textComp.alignmentInt = d[startIdx + 10]["data"].GetInt();
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
                particleComp.isVisible = d[0]["data"].GetBool();

                // Check second element for renderOrder
                if (d.Size() > 1 && d[1].IsObject() && d[1].HasMember("type") && d[1].HasMember("data")) {
                    if (d[1]["type"].GetString() == std::string("int")) {
                        particleComp.renderOrder = d[1]["data"].GetInt();
                        startIdx = 2;
                    } else {
                        // d[1] is not renderOrder, so only isVisible present
                        particleComp.renderOrder = 100;  // default
                        startIdx = 1;
                    }
                } else if (d.Size() > 1 && d[1].IsString()) {
                    // d[1] is a string (probably GUID), so only isVisible present
                    particleComp.renderOrder = 100;  // default
                    startIdx = 1;
                } else {
                    // Only one element or d[1] is something else
                    particleComp.renderOrder = 100;  // default
                    startIdx = 1;
                }
            } else {
                // No base class fields - old format from snapshot
                particleComp.isVisible = true;  // default
                particleComp.renderOrder = 100;  // default
                startIdx = 0;
            }
        } else if (d[0].IsString()) {
            // Old format - starts with GUID string
            particleComp.isVisible = true;  // default
            particleComp.renderOrder = 100;  // default
            startIdx = 0;
        } else {
            particleComp.isVisible = true;  // default
            particleComp.renderOrder = 100;  // default
            startIdx = 0;
        }

        // Component-specific fields
        GUID_string guidStr = extractGUIDString(d[startIdx]);
        particleComp.textureGUID = GUIDUtilities::ConvertStringToGUID128(guidStr);

        // Rest of ParticleComponent fields
        readVec3Generic(d[startIdx + 1], particleComp.emitterPosition);
        particleComp.emissionRate = d[startIdx + 2]["data"].GetFloat();
        particleComp.maxParticles = d[startIdx + 3]["data"].GetInt();
        particleComp.particleLifetime = d[startIdx + 4]["data"].GetFloat();
        particleComp.startSize = d[startIdx + 5]["data"].GetFloat();
        particleComp.endSize = d[startIdx + 6]["data"].GetFloat();
        readVec3Generic(d[startIdx + 7], particleComp.startColor);
        particleComp.startColorAlpha = d[startIdx + 8]["data"].GetFloat();
        readVec3Generic(d[startIdx + 9], particleComp.endColor);
        particleComp.endColorAlpha = d[startIdx + 10]["data"].GetFloat();
        readVec3Generic(d[startIdx + 11], particleComp.gravity);
        particleComp.velocityRandomness = d[startIdx + 12]["data"].GetFloat();
        readVec3Generic(d[startIdx + 13], particleComp.initialVelocity);
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

        // Use helper function to extract audio GUID
        GUID_string guidStr = extractGUIDString(d[1]);
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

void Serializer::DeserializeAudioListenerComponent(AudioListenerComponent& audioListenerComp, const rapidjson::Value& audioListenerJSON) {
    // typed form: tv.data = [ {type: "bool", data: true} ]
    if (audioListenerJSON.HasMember("data") && audioListenerJSON["data"].IsArray()) {
        const auto& d = audioListenerJSON["data"];
        audioListenerComp.enabled = d[0]["data"].GetBool();  // d[0] is the enabled object
    }
}

void Serializer::DeserializeAudioReverbZoneComponent(AudioReverbZoneComponent& audioReverbZoneComp, const rapidjson::Value& audioReverbZoneJSON) {
    // typed form: tv.data = [ {type: "bool", data: true}, {type: "float", data: 10.0}, ... ]
    if (audioReverbZoneJSON.HasMember("data") && audioReverbZoneJSON["data"].IsArray()) {
        const auto& d = audioReverbZoneJSON["data"];
        audioReverbZoneComp.enabled = d[0]["data"].GetBool();
        audioReverbZoneComp.MinDistance = d[1]["data"].GetFloat();
        audioReverbZoneComp.MaxDistance = d[2]["data"].GetFloat();
        audioReverbZoneComp.reverbPresetIndex = d[3]["data"].GetInt();
        audioReverbZoneComp.decayTime = d[4]["data"].GetFloat();
        audioReverbZoneComp.earlyDelay = d[5]["data"].GetFloat();
        audioReverbZoneComp.lateDelay = d[6]["data"].GetFloat();
        audioReverbZoneComp.hfReference = d[7]["data"].GetFloat();
        audioReverbZoneComp.hfDecayRatio = d[8]["data"].GetFloat();
        audioReverbZoneComp.diffusion = d[9]["data"].GetFloat();
        audioReverbZoneComp.density = d[10]["data"].GetFloat();
        audioReverbZoneComp.lowShelfFrequency = d[11]["data"].GetFloat();
        audioReverbZoneComp.lowShelfGain = d[12]["data"].GetFloat();
        audioReverbZoneComp.highCut = d[13]["data"].GetFloat();
        audioReverbZoneComp.earlyLateMix = d[14]["data"].GetFloat();
        audioReverbZoneComp.wetLevel = d[15]["data"].GetFloat();
    }
}

void Serializer::DeserializeRigidBodyComponent(RigidBodyComponent& rbComp, const rapidjson::Value& rbJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (rbJSON.HasMember("data") && rbJSON["data"].IsArray()) {
        const auto& d = rbJSON["data"];
        rbComp.motionID = d[1]["data"].GetInt();
        rbComp.motion = static_cast<Motion>(rbComp.motionID);
        rbComp.ccd = d[2]["data"].GetBool();
        rbComp.transform_dirty = true;
        rbComp.motion_dirty = true;
        rbComp.collider_seen_version = 0;
    }
}

void Serializer::DeserializeColliderComponent(ColliderComponent& colliderComp, const rapidjson::Value& colliderJSON) {
    // typed form: tv.data = [ {type: "std::string", data: "Hello"}, { type:"float", data: 1 }, {type:"bool", data:false} ]
    if (colliderJSON.HasMember("data") && colliderJSON["data"].IsArray()) {
        const auto& d = colliderJSON["data"];
        colliderComp.enabled = d[0]["data"].GetBool();
        colliderComp.layerID = d[1]["data"].GetInt();
        colliderComp.layer = static_cast<JPH::ObjectLayer>(colliderComp.layerID);
        colliderComp.version = d[2]["data"].GetUint();
        colliderComp.shapeTypeID = d[3]["data"].GetInt();
        colliderComp.shapeType = static_cast<ColliderShapeType>(colliderComp.shapeTypeID);
        readVec3Generic(d[4], colliderComp.boxHalfExtents);
        colliderComp.sphereRadius = d[5]["data"].GetFloat();
        colliderComp.capsuleRadius = d[6]["data"].GetFloat();
        colliderComp.capsuleHalfHeight = d[7]["data"].GetFloat();
        colliderComp.cylinderRadius = d[8]["data"].GetFloat();
        colliderComp.cylinderHalfHeight = d[9]["data"].GetFloat();
    }
}

void Serializer::DeserializeParentComponent(ParentComponent& parentComp, const rapidjson::Value& parentJSON) {
    const auto& d = parentJSON["data"];

    // Use helper function to extract parent GUID
    GUID_string parentGUIDStr = extractGUIDString(d[0]);
    parentComp.parent = GUIDUtilities::ConvertStringToGUID128(parentGUIDStr);
}

void Serializer::DeserializeChildrenComponent(ChildrenComponent& childComp, const rapidjson::Value& _childJSON) {
    if (_childJSON.HasMember("data")) {
        //childComp.children.clear();
        const auto& childrenVectorJSON = _childJSON["data"][0]["data"].GetArray();
        for (const auto& childJSON : childrenVectorJSON) {
            // Use helper function to extract child GUIDs
            GUID_string childGUIDStr = extractGUIDString(childJSON);
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

void Serializer::DeserializeSiblingIndexComponent(SiblingIndexComponent& siblingComp, const rapidjson::Value& siblingJSON) {
    if (siblingJSON.HasMember("siblingIndex") && siblingJSON["siblingIndex"].IsInt()) {
        siblingComp.siblingIndex = siblingJSON["siblingIndex"].GetInt();
    }
}

void Serializer::DeserializeCameraComponent(CameraComponent& cameraComp, const rapidjson::Value& cameraJSON) {
    if (cameraJSON.HasMember("data") && cameraJSON["data"].IsArray()) {
        const auto& d = cameraJSON["data"];

        rapidjson::SizeType idx = 0;
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.enabled = d[idx++]["data"].GetBool();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.isActive = d[idx++]["data"].GetBool();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.priority = d[idx++]["data"].GetInt();

        // Skip target and up in the old format (they're not in the reflection data array)
        // These are handled separately by custom serialization

        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.yaw = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.pitch = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.useFreeRotation = d[idx++]["data"].GetBool();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.fov = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.nearPlane = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.farPlane = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.orthoSize = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.movementSpeed = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.mouseSensitivity = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.minZoom = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.maxZoom = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.zoomSpeed = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.shakeIntensity = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.shakeDuration = d[idx++]["data"].GetFloat();
        if (d.Size() > idx && d[idx].HasMember("data")) cameraComp.shakeFrequency = d[idx++]["data"].GetFloat();
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
                    } else {
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
        rapidjson::SizeType idx = 0;
        if (d.Size() > idx && d[idx].HasMember("data")) activeComp.isActive = d[idx++]["data"].GetBool();
    }
}

void Serializer::DeserializeBrainComponent(BrainComponent& brainComp, const rapidjson::Value& brainJSON) {
    if (brainJSON.HasMember("data") && brainJSON["data"].IsArray()) {
        const auto& d = brainJSON["data"];
        brainComp.kindInt = d[0]["data"].GetInt();
        brainComp.kind = static_cast<BrainKind>(brainComp.kindInt);
        brainComp.started = false;
        brainComp.activeState = d[1]["data"].GetString();
        brainComp.enabled = d[2]["data"].GetBool();
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
                    } else if (bd.Size() >= 4) {
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

        if (d.Size() > 1 && d[1].HasMember("data") && d[1]["data"].IsBool())
            buttonComp.interactable = d[1]["data"].GetBool();
    }
}
