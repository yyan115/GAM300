#pragma once

#include <array>
#include <queue>
#include <vector>

#include "Entity.hpp"
#include "Signature.hpp"
#include "../Engine.h"  // For ENGINE_API macro

class ENGINE_API EntityManager {
public:
	EntityManager();

	Entity CreateEntity();

	void DestroyEntity(Entity entity);

	Signature GetEntitySignature(Entity entity) const;

	void SetEntitySignature(Entity entity, Signature signature);

	uint32_t GetActiveEntityCount() const;

	void DestroyAllEntities();

	void SetActive(Entity entity, bool isActive);

	bool IsActive(Entity entity) const;

	std::vector<Entity> GetActiveEntities() const;

	std::vector<Entity> GetAllEntities() const;

private:
	std::queue<Entity> availableEntities{}; // Queue of available entity IDs.
	std::bitset<MAX_ENTITIES> activeEntities; // Bitset to track active entities.

	std::array<Signature, MAX_ENTITIES> entitySignatures{}; // Signatures for each entity.

	uint32_t activeEntityCount{}; // Count of currently active entities.
};