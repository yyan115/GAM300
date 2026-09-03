#include "pch.h"
#include "ECS/EntityManager.hpp"
#include <algorithm>
#include <assert.h>

EntityManager::EntityManager() {
	activeEntityList.reserve(MAX_ENTITIES);
	for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
		availableEntities.push(entity);
	}
}

Entity EntityManager::CreateEntity() {
	//assert(activeEntityCount < MAX_ENTITIES && "Too many entities in existence.");

	if (activeEntityCount >= MAX_ENTITIES) {
		ENGINE_LOG_WARN("[EntityManager] Failed to create entity: Max limit reached!");
		return INVALID_ENTITY; // Return an ID that is out of bounds (Invalid)
	}

	Entity entity = availableEntities.front();
	//ENGINE_LOG_INFO("[EntityManager] Created entity " + std::to_string(entity));
	availableEntities.pop();
	++activeEntityCount;
	//ENGINE_LOG_INFO("[EntityManager] Active entity count: " + std::to_string(activeEntityCount));

	activeEntities[entity] = true;
	if (activeEntityList.empty() || activeEntityList.back() < entity) {
		activeEntityList.push_back(entity);
	}
	else {
		activeEntityList.insert(
			std::lower_bound(activeEntityList.begin(), activeEntityList.end(), entity),
			entity);
	}

	return entity;
}

void EntityManager::DestroyEntity(Entity entity) {
	//assert(entity < MAX_ENTITIES && "Entity out of range.");

	if (entity >= MAX_ENTITIES) {
		ENGINE_LOG_WARN("[EntityManager] Failed to destroy entity " + std::to_string(entity));
		return;
	}

	//ENGINE_LOG_INFO("[EntityManager] Destroying entity " + std::to_string(entity));

	entitySignatures[entity].reset();

	availableEntities.push(entity);
	--activeEntityCount;

	//ENGINE_LOG_INFO("[EntityManager] Active entity count: " + std::to_string(activeEntityCount));

	activeEntities[entity] = false;
	const auto entityIt = std::lower_bound(activeEntityList.begin(), activeEntityList.end(), entity);
	if (entityIt != activeEntityList.end() && *entityIt == entity) {
		activeEntityList.erase(entityIt);
	}
}

Signature EntityManager::GetEntitySignature(Entity entity) const {
	if (entity >= MAX_ENTITIES) {
		// This stops the function immediately
		throw std::runtime_error("[EntityManager] Failed to get entity signature for entity " + std::to_string(entity));
	}

	//assert(entity < MAX_ENTITIES && "Entity out of range.");
	return entitySignatures[entity];
}

void EntityManager::SetEntitySignature(Entity entity, Signature signature) {
	if (entity >= MAX_ENTITIES) {
		// This stops the function immediately
		throw std::runtime_error("[EntityManager] Failed to set entity signature for entity " + std::to_string(entity));
	}

	//assert(entity < MAX_ENTITIES && "Entity out of range.");
	entitySignatures[entity] = signature;
}

uint32_t EntityManager::GetActiveEntityCount() const {
	return activeEntityCount;
}

void EntityManager::DestroyAllEntities() {
	for (auto& signature : entitySignatures) {
		signature.reset();
	}

	activeEntities.reset();
	activeEntityList.clear();
	activeEntityCount = 0;

	while (!availableEntities.empty()) {
		availableEntities.pop();
	}

	for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
		availableEntities.push(entity);
	}
}

void EntityManager::SetActive(Entity entity, bool isActive) {
	if (entity >= MAX_ENTITIES) {
		ENGINE_LOG_WARN("[EntityManager] Failed to set active entity " + std::to_string(entity));
		return;
	}

	//assert(entity < MAX_ENTITIES && "Entity out of range.");
	const bool wasActive = activeEntities[entity];
	if (wasActive == isActive) {
		return;
	}

	activeEntities[entity] = isActive;
	const auto entityIt = std::lower_bound(activeEntityList.begin(), activeEntityList.end(), entity);
	if (isActive) {
		activeEntityList.insert(entityIt, entity);
	}
	else if (entityIt != activeEntityList.end() && *entityIt == entity) {
		activeEntityList.erase(entityIt);
	}
}

bool EntityManager::IsActive(Entity entity) const {
	if (entity >= MAX_ENTITIES) {
		// This stops the function immediately
		throw std::runtime_error("[EntityManager] Failed to check is active for entity " + std::to_string(entity));
	}

	//assert(entity < MAX_ENTITIES && "Entity out of range.");
	return activeEntities[entity];
}

std::vector<Entity> EntityManager::GetActiveEntities() const {
	return activeEntityList;
}

std::vector<Entity> EntityManager::GetAllEntities() const {
	return activeEntityList;
}

const std::vector<Entity>& EntityManager::GetActiveEntitiesView() const noexcept {
	return activeEntityList;
}

const std::vector<Entity>& EntityManager::GetAllEntitiesView() const noexcept {
	return activeEntityList;
}
