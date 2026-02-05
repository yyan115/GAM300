#pragma once
#include <string>
#include <ECS/NameComponent.hpp>
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "ECS/SiblingIndexComponent.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include "ECS/SortingLayerManager.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Sprite/SpriteRenderComponent.hpp>
#include <Graphics/Sprite/SpriteAnimationComponent.hpp>
#include <Graphics/Particle/ParticleComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>
#include <Sound/AudioComponent.hpp>
#include <Sound/AudioListenerComponent.hpp>
#include <Sound/AudioReverbZoneComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include <Physics/ColliderComponent.hpp>
#include <Graphics/Camera/CameraComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include <ECS/ActiveComponent.hpp>
#include <Script/ScriptComponentData.hpp>
#include <Scripting.h>
#include "Engine.h"
#include <Game AI/BrainComponent.hpp>
#include <UI/Button/ButtonComponent.hpp>
#include <UI/Slider/SliderComponent.hpp>
#include <Prefab/PrefabLinkComponent.hpp>
#include <Video/VideoComponent.hpp>

class Serializer {
public:
	static void ENGINE_API SerializeScene(const std::string& scenePath);
    static rapidjson::Value SerializeEntityGUID(Entity entity, rapidjson::Document::AllocatorType& alloc);
    static rapidjson::Value& SerializeEntityGUID(Entity entity, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& entObj);

	// Save an entity and its children recursively.
    static void SerializeEntityRecursively(Entity rootEntity, rapidjson::Document::AllocatorType& alloc, rapidjson::Value& entitiesArr);
    static rapidjson::Value SerializeEntity(Entity entity, rapidjson::Document::AllocatorType& alloc, Entity prefabReferenceEntity = static_cast<Entity>(-1));

    template <typename T>
    static rapidjson::Value SerializeComponentToValue(T& compInstance, rapidjson::Document::AllocatorType& alloc) {
        using CompT = std::decay_t<T>;
        rapidjson::Value val;
        val.SetNull();

        try {
            // Resolve the type descriptor
            TypeDescriptor* td = TypeResolver<CompT>::Get();

            if (!td) {
                std::cerr << "[SerializeComponentToValue] Error: TypeDescriptor not found for component.\n";
                return val;
            }

            // Serialize to string first
            std::stringstream ss;
            td->Serialize(&compInstance, ss);
            std::string s = ss.str();

            // Parse the serialized string into a temporary document
            rapidjson::Document tmp;
            if (tmp.Parse(s.c_str()).HasParseError()) {
                // If parse fails (perhaps it's raw text), store as a string value
                val.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
            }
            else {
                // If parse succeeds, deep copy the structure into 'val' using the provided allocator
                val.CopyFrom(tmp, alloc);
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "[SerializeComponentToValue] reflection serialize exception: " << ex.what() << "\n";
            val.SetNull();
        }
        catch (...) {
            std::cerr << "[SerializeComponentToValue] unknown exception during component serialization\n";
            val.SetNull();
        }

        return val;
    }

    // Helper: Checks if a component differs between Instance and Baseline.
    // If different (or if Instance has it and Baseline doesn't), it serializes the Instance's version.
    template <typename T, typename SerializerFunc>
    static void CheckAndSerializeDelta(
        const char* compName,
        ECSManager& sceneECS, Entity instanceEnt, Entity baselineEnt,
        rapidjson::Document::AllocatorType& alloc,
        rapidjson::Value& outComponentsArray,
        SerializerFunc serializer)
    {
        bool hasInst = sceneECS.HasComponent<T>(instanceEnt);

        // 2. Serialize both to compare
        // (We use a temporary document for the baseline to keep the allocators independent/clean)
        rapidjson::Value valInst;
        if (hasInst) {
            valInst = serializer(sceneECS.GetComponent<T>(instanceEnt), alloc);
        }

        // If there is no valid baselineEnt (i.e. prefab with a new child created), straightaway fully serialize the component.
        if (hasInst && baselineEnt == static_cast<Entity>(-1)) {
            rapidjson::Value wrapper(rapidjson::kObjectType);
            wrapper.AddMember(rapidjson::StringRef(compName), valInst, alloc);
            outComponentsArray.PushBack(wrapper, alloc);
            return;
        }

        bool hasBase = sceneECS.HasComponent<T>(baselineEnt);

        // 1. If neither has it, skip
        if (!hasInst && !hasBase) return;


        rapidjson::Value valBase;
        if (hasBase) {
            // We use the same allocator for comparison speed, but strictly 
            // we're only keeping valInst if they differ.
            valBase = serializer(sceneECS.GetComponent<T>(baselineEnt), alloc);
        }

        // 3. Compare: If they are different, we save the Instance version as an override
        // Note: If hasInst is false (component removed on instance), this logic currently 
        // doesn't record a "RemoveComponent" command. It simply omits it. 
        // To support removal, you'd need a separate "RemovedComponents" list.
        if (hasInst && valInst != valBase) {
            rapidjson::Value wrapper(rapidjson::kObjectType);
            wrapper.AddMember(rapidjson::StringRef(compName), valInst, alloc);
            outComponentsArray.PushBack(wrapper, alloc);
        }
    }

    static void SerializePrefabInstanceDelta(
        ECSManager& sceneECS, Entity instanceEnt, Entity baselineEnt,
        rapidjson::Document::AllocatorType& alloc,
        rapidjson::Value& outComponentsArray);

    static void SerializePrefabOverridesRecursive(
        ECSManager& sceneECS, Entity instanceEnt, Entity baselineEnt,
        rapidjson::Document::AllocatorType& alloc,
        rapidjson::Value& outEntityNode);

	// ============ DESERIALIZATION FUNCTIONS ============

    static void UpdateEntityGUID_Safe(ECSManager& ecs, Entity entity, GUID_128 newGUID);
    static void RestorePrefabHierarchy(ECSManager& ecs, Entity currentEntity, const rapidjson::Value& jsonNode);
    static void ApplyPrefabOverridesRecursive(ECSManager& ecs, Entity& currentEntity, const rapidjson::Value& jsonNode, bool isNewEntity = false);

    static Entity DeserializeEntity(ECSManager& ecs, const rapidjson::Value& entObj, bool isPrefab = false, Entity entity = MAX_ENTITIES, bool skipSpawnChildren = false, bool initialiseAnimation = true);
	static void ENGINE_API DeserializeScene(const std::string& scenePath);
	static void ENGINE_API ReloadScene(const std::string& tempScenePath, const std::string& currentScenePath);

    static GUID_128 DeserializeEntityGUID(const rapidjson::Value& entityJSON);
	static Entity CreateEntityViaGUID(const rapidjson::Value& entityJSON);
	static void DeserializeNameComponent(NameComponent& nameComp, const rapidjson::Value& nameJSON);
	static void DeserializeTransformComponent(Entity newEnt, const rapidjson::Value& t);
	static void DeserializeModelComponent(ModelRenderComponent& modelComp, const rapidjson::Value& modelJSON, Entity root, bool skipSpawnChildren = false);
	static void DeserializeSpriteComponent(SpriteRenderComponent& spriteComp, const rapidjson::Value& spriteJSON);
	static void DeserializeSpriteAnimationComponent(SpriteAnimationComponent& animComp, const rapidjson::Value& animJSON);
	static void DeserializeAnimationComponent(AnimationComponent& animComp, const rapidjson::Value& animJSON);
	static void DeserializeTextComponent(TextRenderComponent& textComp, const rapidjson::Value& textJSON);
	static void DeserializeParticleComponent(ParticleComponent& particleComp, const rapidjson::Value& particleJSON);
	static void DeserializeDirLightComponent(DirectionalLightComponent& dirLightComp, const rapidjson::Value& dirLightJSON);
	static void DeserializeSpotLightComponent(SpotLightComponent& spotLightComp, const rapidjson::Value& spotLightJSON);
	static void DeserializePointLightComponent(PointLightComponent& pointLightComp, const rapidjson::Value& pointLightJSON);
	static void DeserializeAudioComponent(AudioComponent& audioComp, const rapidjson::Value& audioJSON);
	static void DeserializeAudioListenerComponent(AudioListenerComponent& audioListenerComp, const rapidjson::Value& audioListenerJSON);
	static void DeserializeAudioReverbZoneComponent(AudioReverbZoneComponent& audioReverbZoneComp, const rapidjson::Value& audioReverbZoneJSON);
	static void DeserializeRigidBodyComponent(RigidBodyComponent& rbComp, const rapidjson::Value& rbJSON);
	static void DeserializeColliderComponent(ColliderComponent& colliderComp, const rapidjson::Value& colliderJSON);
	static void DeserializeParentComponent(ParentComponent& parentComp, const rapidjson::Value& parentJSON, std::unordered_map<GUID_128, GUID_128>* guidRemap = nullptr);
	static void DeserializeChildrenComponent(ChildrenComponent& childComp, const rapidjson::Value& childJSON, std::unordered_map<GUID_128, GUID_128>* guidRemap = nullptr);
	static void DeserializeTagComponent(TagComponent& tagComp, const rapidjson::Value& tagJSON);
	static void DeserializeLayerComponent(LayerComponent& layerComp, const rapidjson::Value& layerJSON);
	static void DeserializeSiblingIndexComponent(SiblingIndexComponent& siblingComp, const rapidjson::Value& siblingJSON);
	static void DeserializeCameraComponent(CameraComponent& cameraComp, const rapidjson::Value& cameraJSON);
	static void DeserializeScriptComponent(Entity entity, const rapidjson::Value& scriptJSON);
	static void DeserializeActiveComponent(ActiveComponent& activeComp, const rapidjson::Value& activeJSON);
	static void DeserializeBrainComponent(BrainComponent& brainComp, const rapidjson::Value& brainJSON);
	static void DeserializeButtonComponent(ButtonComponent& buttonComp, const rapidjson::Value& buttonJSON);
	static void DeserializeSliderComponent(SliderComponent& sliderComp, const rapidjson::Value& sliderJSON);
	static void DeserializePrefabLinkComponent(PrefabLinkComponent& prefabComp, const rapidjson::Value& prefabJSON);

    static void DeserializeVideoComponent(VideoComponent & videoComp, const rapidjson::Value& sliderJSON);
    // ==================================================================================
    // Boolean Helper
    // Handles: [true] OR [{"type": "bool", "data": true}]
    // ==================================================================================
    static bool GetBool(const rapidjson::Value& dataArray, size_t index, bool defaultValue = false)
    {
        // 1. Array Bounds Check
        if (!dataArray.IsArray() || index >= dataArray.Size()) {
            return defaultValue;
        }

        const rapidjson::Value& item = dataArray[index];

        // 2. Case A: Raw Primitive
        if (item.IsBool()) {
            return item.GetBool();
        }

        // 3. Case B: Wrapped Object ({"data": value})
        if (item.IsObject() && item.HasMember("data")) {
            const rapidjson::Value& data = item["data"];
            if (data.IsBool()) {
                return data.GetBool();
            }
        }

        return defaultValue;
    }

    // ==================================================================================
    // Float Helper
    // Handles: [1.5] OR [{"type": "float", "data": 1.5}]
    // ==================================================================================
    static float GetFloat(const rapidjson::Value& dataArray, size_t index, float defaultValue = 0.0f)
    {
        if (!dataArray.IsArray() || index >= dataArray.Size()) {
            return defaultValue;
        }

        const rapidjson::Value& item = dataArray[index];

        // Case A: Raw
        if (item.IsNumber()) {
            return item.GetFloat();
        }

        // Case B: Wrapped
        if (item.IsObject() && item.HasMember("data")) {
            const rapidjson::Value& data = item["data"];
            if (data.IsNumber()) {
                return data.GetFloat();
            }
        }

        return defaultValue;
    }

    // ==================================================================================
    // Int Helper
    // Handles: [42] OR [{"type": "int", "data": 42}]
    // ==================================================================================
    static int GetInt(const rapidjson::Value& dataArray, size_t index, int defaultValue = 0)
    {
        if (!dataArray.IsArray() || index >= dataArray.Size()) {
            return defaultValue;
        }

        const rapidjson::Value& item = dataArray[index];

        if (item.IsInt()) {
            return item.GetInt();
        }

        if (item.IsObject() && item.HasMember("data")) {
            const rapidjson::Value& data = item["data"];
            if (data.IsInt()) {
                return data.GetInt();
            }
        }

        return defaultValue;
    }

    // ==================================================================================
    // String Helper
    // Handles: ["GUID"] OR [{"type": "string", "data": "text"}]
    // ==================================================================================
    static std::string GetString(const rapidjson::Value& dataArray, size_t index, const std::string& defaultValue = "")
    {
        if (!dataArray.IsArray() || index >= dataArray.Size()) {
            return defaultValue;
        }

        const rapidjson::Value& item = dataArray[index];

        if (item.IsString()) {
            return item.GetString();
        }

        if (item.IsObject() && item.HasMember("data")) {
            const rapidjson::Value& data = item["data"];
            if (data.IsString()) {
                return data.GetString();
            }
        }

        return defaultValue;
    }

    static Vector3D GetVector3D(const rapidjson::Value& dataArray, size_t index, const Vector3D& defaultValue = Vector3D(0, 0, 0))
    {
        // 1. Basic Bounds Check
        if (!dataArray.IsArray() || index >= dataArray.Size()) {
            return defaultValue;
        }

        const rapidjson::Value& item = dataArray[index];

        // 2. Identify where the XYZ data is located
        const rapidjson::Value* vecData = nullptr;

        // Case A: The item is the wrapper object {"type": "Vector3D", "data": [...]}
        if (item.IsObject() && item.HasMember("data") && item["data"].IsArray()) {
            vecData = &item["data"];
        }
        // Case B: The item is a raw array [x, y, z]
        else if (item.IsArray()) {
            vecData = &item;
        }

        // 3. Validation: Must have at least 3 components
        if (!vecData || vecData->Size() < 3) {
            return defaultValue;
        }

        Vector3D result;
        float* outComponents[3] = { &result.x, &result.y, &result.z };

        // 4. Extract each component using your GetFloat-style logic
        for (size_t i = 0; i < 3; ++i) {
            const rapidjson::Value& val = (*vecData)[i];

            if (val.IsNumber()) {
                *outComponents[i] = val.GetFloat();
            }
            else if (val.IsObject() && val.HasMember("data") && val["data"].IsNumber()) {
                *outComponents[i] = val["data"].GetFloat();
            }
            else {
                // If any component is invalid, return the full default to avoid partial vectors
                return defaultValue;
            }
        }

        return result;
    }

private:
	Serializer() = delete;
	~Serializer() = delete;
};