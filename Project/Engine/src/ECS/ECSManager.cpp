#include "pch.h"
#include "ECS/ECSManager.hpp"
#include <Transform/TransformComponent.hpp>
#include <Graphics/Model/ModelSystem.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include "ECS/NameComponent.hpp"
#include <Graphics/Lights/LightComponent.hpp>

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
	RegisterComponent<LightComponent>();

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
		signature.set(GetComponentID<LightComponent>()); 
		SetSystemSignature<LightingSystem>(signature); 
	}
}

Entity ECSManager::CreateEntity() {
	Entity entity = entityManager->CreateEntity();
	std::cout << "[ECSManager] Created entity " << entity << ". Total active entities: " << entityManager->GetActiveEntityCount() << std::endl;

	// Add default components here (e.g. Name, Transform, etc.)

	return entity;
}

void ECSManager::DestroyEntity(Entity entity) {
	entityManager->DestroyEntity(entity);
	componentManager->EntityDestroyed(entity);
	systemManager->EntityDestroyed(entity);

	std::cout << "[ECSManager] Destroyed entity " << entity << ". Total active entities: " << entityManager->GetActiveEntityCount() << std::endl;
}

void ECSManager::ClearAllEntities() {
	entityManager->DestroyAllEntities();
	componentManager->AllEntitiesDestroyed();
	systemManager->AllEntitiesDestroyed();

	std::cout << "[ECSManager] Cleared all entities. Total active entities: " << entityManager->GetActiveEntityCount() << std::endl;
}