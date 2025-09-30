#include "pch.h"
#include "ECS/ECSRegistry.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "ECS/NameComponent.hpp"
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
#include "Logging.hpp"
#include <Graphics/Sprite/SpriteRenderComponent.hpp>

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
	RegisterComponent<ParentComponent>();
	RegisterComponent<ChildrenComponent>();
	RegisterComponent<AudioComponent>();
	RegisterComponent<SpriteRenderComponent>();
	RegisterComponent<DirectionalLightComponent>();
	RegisterComponent<PointLightComponent>();
	RegisterComponent<SpotLightComponent>();

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

	audioSystem = RegisterSystem<AudioSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<AudioComponent>());
		SetSystemSignature<AudioSystem>(signature);
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

	Transform defaultTransform;
	defaultTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
	defaultTransform.localScale = Vector3D(1.0f, 1.0f, 1.0f);
	defaultTransform.localRotation = Quaternion();
	defaultTransform.isDirty = true;

	ecsManager.AddComponent<Transform>(entity, defaultTransform);

	return entity;
}

void ECSManager::DestroyEntity(Entity entity) {
	entityManager->DestroyEntity(entity);
	componentManager->EntityDestroyed(entity);
	systemManager->EntityDestroyed(entity);
	ENGINE_PRINT("[ECSManager] Destroyed entity " , entity , ". Total active entities: " , entityManager->GetActiveEntityCount() , "\n");
	//std::cout << "[ECSManager] Destroyed entity " << entity << ". Total active entities: " << entityManager->GetActiveEntityCount() << std::endl;
}

void ECSManager::ClearAllEntities() {
	entityManager->DestroyAllEntities();
	componentManager->AllEntitiesDestroyed();
	systemManager->AllEntitiesDestroyed();
	ENGINE_PRINT("[ECSManager] Cleared all entities. Total active entities: " , entityManager->GetActiveEntityCount(), "\n");
	//std::cout << "[ECSManager] Cleared all entities. Total active entities: " << entityManager->GetActiveEntityCount() << std::endl;
}