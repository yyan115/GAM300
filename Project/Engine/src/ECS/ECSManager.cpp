#include "pch.h"
#include "ECS/ECSRegistry.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
#include <Transform/TransformComponent.hpp>
#include <Math/Vector3D.hpp>
#include <Graphics/Model/ModelSystem.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include "ECS/NameComponent.hpp"
#include <Graphics/Lights/LightComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "Animation/AnimationComponent.hpp"
#include "PrefabLinkComponent.hpp"
#include "Logging.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Game AI/BrainComponent.hpp"

#include <Physics/ColliderComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include <Physics/PhysicsSystem.hpp>
#include <Physics/Kinematics/CharacterControllerSystem.hpp>

#include "Script/ScriptComponentData.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include <ECS/TagComponent.hpp>
#include <ECS/LayerComponent.hpp>
#include <ECS/SiblingIndexComponent.hpp>
#include "UI/Button/ButtonComponent.hpp"
#include "UI/Slider/SliderComponent.hpp"
#include "UI/Slider/SliderSystem.hpp"
#include <Graphics/Sprite/SpriteAnimationComponent.hpp>
#include "Video/VideoComponent.hpp"

void ECSManager::Initialize() {
	entityManager = std::make_unique<EntityManager>();
	componentManager = std::make_unique<ComponentManager>();
	systemManager = std::make_unique<SystemManager>();

	// REGISTER ALL COMPONENTS HERE
	// e.g., 
	RegisterComponent<Transform>();
	RegisterComponent<ModelRenderComponent>();
	RegisterComponent<TextRenderComponent>();
	RegisterComponent<DebugDrawComponent>();
	RegisterComponent<NameComponent>();
	RegisterComponent<ActiveComponent>();
	RegisterComponent<ColliderComponent>();
	RegisterComponent<RigidBodyComponent>();
	RegisterComponent<VideoComponent>();
	RegisterComponent<LightComponent>();
	RegisterComponent<DirectionalLightComponent>();
	RegisterComponent<PointLightComponent>();
	RegisterComponent<SpotLightComponent>();
	RegisterComponent<ParentComponent>();
	RegisterComponent<ChildrenComponent>();
	RegisterComponent<AudioComponent>();
	RegisterComponent<AudioListenerComponent>();
	RegisterComponent<AudioReverbZoneComponent>();
	RegisterComponent<SpriteRenderComponent>();
	RegisterComponent<ParticleComponent>();
	RegisterComponent<AnimationComponent>();
	RegisterComponent<PrefabLinkComponent>();
	RegisterComponent<CameraComponent>();
	RegisterComponent<TagComponent>();
	RegisterComponent<LayerComponent>();
	RegisterComponent<SiblingIndexComponent>();
	RegisterComponent<ScriptComponentData>();
	RegisterComponent<BrainComponent>();
	RegisterComponent<SpriteAnimationComponent>();
	RegisterComponent<ButtonComponent>();
	RegisterComponent<SliderComponent>();

	// REGISTER ALL SYSTEMS AND ITS SIGNATURES HERE
	// e.g.,
	transformSystem = RegisterSystem<TransformSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<Transform>());
		SetSystemSignature<TransformSystem>(signature);
	}

	modelSystem = RegisterSystem<ModelSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ModelRenderComponent>());
		SetSystemSignature<ModelSystem>(signature);
	}

	textSystem = RegisterSystem<TextRenderingSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<TextRenderComponent>());
		SetSystemSignature<TextRenderingSystem>(signature);
	}

	debugDrawSystem = RegisterSystem<DebugDrawSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<DebugDrawComponent>());
		SetSystemSignature<DebugDrawSystem>(signature);
	}

	physicsSystem = RegisterSystem<PhysicsSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ColliderComponent>());
		signature.set(GetComponentID<RigidBodyComponent>());
		SetSystemSignature<PhysicsSystem>(signature);
	}
 
	characterControllerSystem = RegisterSystem<CharacterControllerSystem>();
	{
		Signature signature;

		signature.set(GetComponentID<ColliderComponent>());
		signature.set(GetComponentID<RigidBodyComponent>());
		SetSystemSignature<CharacterControllerSystem>(signature);
	}

	if (physicsSystem && characterControllerSystem)
	{
		JPH::PhysicsSystem* joltPhysics = &physicsSystem->GetJoltSystem();
		if (joltPhysics)
		{
			characterControllerSystem->SetPhysicsSystem(joltPhysics);
		}
	}


	lightingSystem = RegisterSystem<LightingSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<DirectionalLightComponent>());
		signature.set(GetComponentID<PointLightComponent>());
		signature.set(GetComponentID<SpotLightComponent>());
		SetSystemSignature<LightingSystem>(signature);
	}

	spriteSystem = RegisterSystem<SpriteSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<SpriteRenderComponent>());
		SetSystemSignature<SpriteSystem>(signature);
	}

	particleSystem = RegisterSystem<ParticleSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ParticleComponent>());
		SetSystemSignature<ParticleSystem>(signature);
	}
	
	audioSystem = RegisterSystem<AudioSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<AudioComponent>());
		signature.set(GetComponentID<AudioListenerComponent>());
		SetSystemSignature<AudioSystem>(signature);
	}

	animationSystem = RegisterSystem<AnimationSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<AnimationComponent>());
		SetSystemSignature<AnimationSystem>(signature);
	}
	
	cameraSystem = RegisterSystem<CameraSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<CameraComponent>()); 
		SetSystemSignature<CameraSystem>(signature); 
	}

	scriptSystem = RegisterSystem<ScriptSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ScriptComponentData>());
		SetSystemSignature<ScriptSystem>(signature);
	}

	spriteAnimationSystem = RegisterSystem<SpriteAnimationSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<SpriteAnimationComponent>());
		SetSystemSignature<SpriteAnimationSystem>(signature);
	}

	buttonSystem = RegisterSystem<ButtonSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ButtonComponent>());
		SetSystemSignature<ButtonSystem>(signature);
	}

	sliderSystem = RegisterSystem<SliderSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<SliderComponent>());
		SetSystemSignature<SliderSystem>(signature);
	}

}

Entity ECSManager::CreateEntity() {
	// Register the entity with a new GUID
	GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
	GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);

	return CreateEntityWithGUID(guid);
}

Entity ECSManager::CreateEntityWithGUID(const GUID_128& guid) {
	Entity entity = entityManager->CreateEntity();
	EntityGUIDRegistry::GetInstance().Register(entity, guid);
	ENGINE_PRINT("[ECSManager] Created entity ", entity, ". Total active entities: ", entityManager->GetActiveEntityCount(), "\n");

	// Add default components here (e.g. Name, Transform, etc.)
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	ecsManager.AddComponent<NameComponent>(entity, NameComponent("Entity_" + std::to_string(entity)));
	ecsManager.AddComponent<ActiveComponent>(entity, ActiveComponent(true)); // Entity active by default

	Transform defaultTransform;
	defaultTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
	defaultTransform.localScale = Vector3D(1.0f, 1.0f, 1.0f);
	defaultTransform.localRotation = Quaternion();
	defaultTransform.isDirty = true;

	ecsManager.AddComponent<Transform>(entity, defaultTransform);

	// Add default tag and layer components
	ecsManager.AddComponent<TagComponent>(entity, TagComponent(0)); // Default to first tag
	ecsManager.AddComponent<LayerComponent>(entity, LayerComponent(0)); // Default to first layer

	return entity;
}

void ECSManager::DestroyEntity(Entity entity) {
	// Recursively destroy children first to avoid dangling ParentComponent references
	if (HasComponent<ChildrenComponent>(entity)) {
		auto childrenCopy = GetComponent<ChildrenComponent>(entity).children;
		for (const auto& childGuid : childrenCopy) {
			Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGuid);
			if (child != static_cast<Entity>(-1) && child != entity) {
				DestroyEntity(child);
			}
		}
	}

	// Remove the destroyed entity from its parent's children component, if it was a child.
	if (HasComponent<ParentComponent>(entity)) {
		GUID_128 entityGUID = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(entity);
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(GetComponent<ParentComponent>(entity).parent);
		if (parent != static_cast<Entity>(-1) && HasComponent<ChildrenComponent>(parent)) {
			auto& childrenComp = GetComponent<ChildrenComponent>(parent);
			auto it = std::find(childrenComp.children.begin(), childrenComp.children.end(), entityGUID);
			if (it != childrenComp.children.end()) {
				childrenComp.children.erase(it);
			}
			if (childrenComp.children.empty()) {
				RemoveComponent<ChildrenComponent>(parent);
			}
		}
	}

	EntityGUIDRegistry::GetInstance().Unregister(entity);
	entityManager->DestroyEntity(entity);
	componentManager->EntityDestroyed(entity);
	systemManager->EntityDestroyed(entity);
	ENGINE_PRINT("[ECSManager] Destroyed entity " , entity , ". Total active entities: " , entityManager->GetActiveEntityCount() , "\n");
}

void ECSManager::ClearAllEntities() {
	entityManager->DestroyAllEntities();
	componentManager->AllEntitiesDestroyed();
	systemManager->AllEntitiesDestroyed();
	ENGINE_PRINT("[ECSManager] Cleared all entities. Total active entities: " , entityManager->GetActiveEntityCount(), "\n");
}

bool ECSManager::IsEntityActiveInHierarchy(Entity entity) {
	// Check if entity itself is active
	if (HasComponent<ActiveComponent>(entity)) {
		auto& activeComp = GetComponent<ActiveComponent>(entity);
		if (!activeComp.isActive) {
			return false;
		}
	}

	// Traverse up the parent hierarchy and check each ancestor
	Entity currentEntity = entity;
	auto& guidRegistry = EntityGUIDRegistry::GetInstance();

	while (HasComponent<ParentComponent>(currentEntity)) {
		// Get parent entity
		auto& parentComp = GetComponent<ParentComponent>(currentEntity);
		Entity parentEntity = guidRegistry.GetEntityByGUID(parentComp.parent);

		// Check if parent is valid
		if (parentEntity == UINT32_MAX) {
			break; // Invalid parent, stop traversal
		}

		// Check if parent is active
		if (HasComponent<ActiveComponent>(parentEntity)) {
			auto& parentActiveComp = GetComponent<ActiveComponent>(parentEntity);
			if (!parentActiveComp.isActive) {
				return false; // Parent is inactive, so this entity is inactive in hierarchy
			}
		}

		// Move up to the parent
		currentEntity = parentEntity;
	}

	return true; // Entity and all ancestors are active
}