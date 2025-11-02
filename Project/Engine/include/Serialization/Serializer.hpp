#pragma once
#include <string>
#include <ECS/NameComponent.hpp>
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/layerManager.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Sprite/SpriteRenderComponent.hpp>
#include <Graphics/Particle/ParticleComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>
#include <Sound/AudioComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include <Physics/ColliderComponent.hpp>
#include <Graphics/Camera/CameraComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include <ECS/ActiveComponent.hpp>

class Serializer {
public:
	static void SerializeScene(const std::string& scenePath);
	static void DeserializeScene(const std::string& scenePath);
	static void ReloadScene(const std::string& tempScenePath, const std::string& currentScenePath);

	static Entity CreateEntityViaGUID(const rapidjson::Value& entityJSON);
	static void DeserializeNameComponent(NameComponent& nameComp, const rapidjson::Value& nameJSON);
	static void DeserializeTransformComponent(Entity newEnt, const rapidjson::Value& t);
	static void DeserializeModelComponent(ModelRenderComponent& modelComp, const rapidjson::Value& modelJSON);
	static void DeserializeSpriteComponent(SpriteRenderComponent& spriteComp, const rapidjson::Value& spriteJSON);
	static void DeserializeTextComponent(TextRenderComponent& textComp, const rapidjson::Value& textJSON);
	static void DeserializeParticleComponent(ParticleComponent& particleComp, const rapidjson::Value& particleJSON);
	static void DeserializeDirLightComponent(DirectionalLightComponent& dirLightComp, const rapidjson::Value& dirLightJSON);
	static void DeserializeSpotLightComponent(SpotLightComponent& spotLightComp, const rapidjson::Value& spotLightJSON);
	static void DeserializePointLightComponent(PointLightComponent& pointLightComp, const rapidjson::Value& pointLightJSON);
	static void DeserializeAudioComponent(AudioComponent& audioComp, const rapidjson::Value& audioJSON);
	static void DeserializeRigidBodyComponent(RigidBodyComponent& rbComp, const rapidjson::Value& rbJSON);
	static void DeserializeColliderComponent(ColliderComponent& colliderComp, const rapidjson::Value& colliderJSON);
	static void DeserializeParentComponent(ParentComponent& parentComp, const rapidjson::Value& parentJSON);
	static void DeserializeChildrenComponent(ChildrenComponent& childComp, const rapidjson::Value& childJSON);
	static void DeserializeTagComponent(TagComponent& tagComp, const rapidjson::Value& tagJSON);
	static void DeserializeLayerComponent(LayerComponent& layerComp, const rapidjson::Value& layerJSON);
	static void DeserializeCameraComponent(CameraComponent& cameraComp, const rapidjson::Value& cameraJSON);
	static void DeserializeActiveComponent(ActiveComponent& activeComp, const rapidjson::Value& activeJSON);

private:
	Serializer() = delete;
	~Serializer() = delete;
};