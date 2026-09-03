#pragma once

#include <array>
#include <queue>
#include <vector>

#include "Entity.hpp"
#include "Signature.hpp"
#include "../Engine.h"  // For ENGINE_API macro

class EntityManager {
public:
	EntityManager();

	Entity CreateEntity();

	void DestroyEntity(Entity entity);

	Signature ENGINE_API GetEntitySignature(Entity entity) const;

	void ENGINE_API SetEntitySignature(Entity entity, Signature signature);

	uint32_t GetActiveEntityCount() const;

	void DestroyAllEntities();

	void SetActive(Entity entity, bool isActive);

	bool IsActive(Entity entity) const;
	bool Exists(Entity entity) const noexcept {
		return entity < MAX_ENTITIES && activeEntities[entity];
	}

	std::vector<Entity> ENGINE_API GetActiveEntities() const;

	std::vector<Entity> ENGINE_API GetAllEntities() const;

	// Allocation-free views; invalidated by entity creation, destruction, or activation changes.
	const std::vector<Entity>& ENGINE_API GetActiveEntitiesView() const noexcept;

	const std::vector<Entity>& ENGINE_API GetAllEntitiesView() const noexcept;

private:
	std::queue<Entity> availableEntities{}; // Queue of available entity IDs.
	std::bitset<MAX_ENTITIES> activeEntities; // Bitset to track active entities.
	std::vector<Entity> activeEntityList; // Sorted dense view for allocation-free frame iteration.

	std::array<Signature, MAX_ENTITIES> entitySignatures{}; // Signatures for each entity.

	uint32_t activeEntityCount{}; // Count of currently active entities.
};
