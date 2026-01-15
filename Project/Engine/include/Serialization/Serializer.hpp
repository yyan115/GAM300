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

class Serializer {
public:
	static void ENGINE_API SerializeScene(const std::string& scenePath);
	static void ENGINE_API DeserializeScene(const std::string& scenePath);
	static void ENGINE_API ReloadScene(const std::string& tempScenePath, const std::string& currentScenePath);

	static Entity CreateEntityViaGUID(const rapidjson::Value& entityJSON);
	static void DeserializeNameComponent(NameComponent& nameComp, const rapidjson::Value& nameJSON);
	static void DeserializeTransformComponent(Entity newEnt, const rapidjson::Value& t);
	static void DeserializeModelComponent(ModelRenderComponent& modelComp, const rapidjson::Value& modelJSON);
	static void DeserializeSpriteComponent(SpriteRenderComponent& spriteComp, const rapidjson::Value& spriteJSON);
	static void DeserializeSpriteAnimationComponent(SpriteAnimationComponent& animComp, const rapidjson::Value& animJSON);
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
	static void DeserializeParentComponent(ParentComponent& parentComp, const rapidjson::Value& parentJSON);
	static void DeserializeChildrenComponent(ChildrenComponent& childComp, const rapidjson::Value& childJSON);
	static void DeserializeTagComponent(TagComponent& tagComp, const rapidjson::Value& tagJSON);
	static void DeserializeLayerComponent(LayerComponent& layerComp, const rapidjson::Value& layerJSON);
	static void DeserializeSiblingIndexComponent(SiblingIndexComponent& siblingComp, const rapidjson::Value& siblingJSON);
	static void DeserializeCameraComponent(CameraComponent& cameraComp, const rapidjson::Value& cameraJSON);
	static void DeserializeScriptComponent(Entity entity, const rapidjson::Value& scriptJSON);
	static void DeserializeActiveComponent(ActiveComponent& activeComp, const rapidjson::Value& activeJSON);
	static void DeserializeBrainComponent(BrainComponent& brainComp, const rapidjson::Value& brainJSON);
	static void DeserializeButtonComponent(ButtonComponent& buttonComp, const rapidjson::Value& buttonJSON);
	static void DeserializeSliderComponent(SliderComponent& sliderComp, const rapidjson::Value& sliderJSON);

private:
	Serializer() = delete;
	~Serializer() = delete;
};