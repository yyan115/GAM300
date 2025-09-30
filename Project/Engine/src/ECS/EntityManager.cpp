#include "pch.h"
#include "ECS/EntityManager.hpp"
#include <assert.h>

EntityManager::EntityManager() {
	for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
		availableEntities.push(entity);
	}
}

Entity EntityManager::CreateEntity() {
	assert(activeEntityCount < MAX_ENTITIES && "Too many entities in existence.");

	Entity entity = availableEntities.front();
	availableEntities.pop();
	++activeEntityCount;

	activeEntities[entity] = true;

	return entity;
}

void EntityManager::DestroyEntity(Entity entity) {
	assert(entity < MAX_ENTITIES && "Entity out of range.");

	entitySignatures[entity].reset();

	availableEntities.push(entity);
	--activeEntityCount;

	activeEntities[entity] = false;
}

Signature EntityManager::GetEntitySignature(Entity entity) const {
	assert(entity < MAX_ENTITIES && "Entity out of range.");
	return entitySignatures[entity];
}

void EntityManager::SetEntitySignature(Entity entity, Signature signature) {
	assert(entity < MAX_ENTITIES && "Entity out of range.");
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
	activeEntityCount = 0;

	while (!availableEntities.empty()) {
		availableEntities.pop();
	}

	for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
		availableEntities.push(entity);
	}
}

void EntityManager::SetActive(Entity entity, bool isActive) {
	assert(entity < MAX_ENTITIES && "Entity out of range.");
	activeEntities[entity] = isActive;
}

bool EntityManager::IsActive(Entity entity) const {
	assert(entity < MAX_ENTITIES && "Entity out of range.");
	return activeEntities[entity];
}

std::vector<Entity> EntityManager::GetActiveEntities() const {
	std::vector<Entity> entities;
	for (Entity entity = 0; entity < activeEntityCount; ++entity) {
		if (activeEntities[entity]) {
			entities.push_back(entity);
		}
	}
	return entities;
}

std::vector<Entity> EntityManager::GetAllEntities() const {
	std::vector<Entity> entities;
	for (Entity entity = 0; entity < activeEntityCount; ++entity) {
		entities.push_back(entity);
	}
	return entities;
}
