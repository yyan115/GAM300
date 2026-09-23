#pragma once

#include <array>
#include <cstdint>
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

	std::vector<Entity> ENGINE_API GetActiveEntities() const;

	std::vector<Entity> ENGINE_API GetAllEntities() const;

private:
	std::queue<Entity> availableEntities{}; // Queue of available entity IDs.
	static constexpr Entity entitiesPerWord = 64;
	std::array<std::uint64_t, (MAX_ENTITIES + entitiesPerWord - 1) / entitiesPerWord> activeEntities{};

	void SetActiveBit(Entity entity, bool active) {
		auto& word = activeEntities[entity / entitiesPerWord];
		const auto bit = std::uint64_t{1} << (entity % entitiesPerWord);
		if (active) word |= bit;
		else word &= ~bit;
	}

	std::array<Signature, MAX_ENTITIES> entitySignatures{}; // Signatures for each entity.

	uint32_t activeEntityCount{}; // Count of currently active entities.
};
