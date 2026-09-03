#pragma once
#include <unordered_map>
#include <array>
#include <cstdint>
#include "../ECS/Entity.hpp"
#include "../Utilities/GUID.hpp"
#include <iostream>

class EntityGUIDRegistry {
public:
	ENGINE_API static EntityGUIDRegistry& GetInstance();

	void Register(Entity entityID, const GUID_128& guid) {
			guidToEntityMap[guid] = entityID;
			entityToGuidMap[entityID] = guid;
			++revision;
		}

	void Unregister(Entity entityID) {
		auto it = entityToGuidMap.find(entityID);
			if (it != entityToGuidMap.end()) {
				guidToEntityMap.erase(it->second);
				entityToGuidMap.erase(it);
				++revision;
			}
	}

	void Unregister(const GUID_128& guid) {
		auto it = guidToEntityMap.find(guid);
			if (it != guidToEntityMap.end()) {
				entityToGuidMap.erase(it->second);
				guidToEntityMap.erase(it);
				++revision;
			}
	}
	
	void Clear() {
			guidToEntityMap.clear();
			entityToGuidMap.clear();
			++revision;
		}

	Entity GetEntityByGUID(const GUID_128& guid) const {
		// Hierarchy traversal resolves the same GUIDs thousands of times per
		// frame. A small per-thread direct-mapped cache avoids unordered_map
		// pointer chasing while remaining lock-free for parallel read phases.
		struct CacheEntry {
			GUID_128 guid{};
			Entity entity = INVALID_ENTITY;
			std::uint64_t registryRevision = 0;
		};
		constexpr std::size_t cacheSize = 2048;
		static_assert((cacheSize & (cacheSize - 1)) == 0);
		thread_local std::array<CacheEntry, cacheSize> cache{};

		const std::size_t cacheIndex =
			std::hash<GUID_128>{}(guid) & (cacheSize - 1);
		CacheEntry& cached = cache[cacheIndex];
		if (cached.registryRevision == revision && cached.guid == guid) {
			return cached.entity;
		}

			auto it = guidToEntityMap.find(guid);
			const Entity entity = it != guidToEntityMap.end()
				? it->second
				: INVALID_ENTITY;
			cached = CacheEntry{guid, entity, revision};
			return entity;

			//ENGINE_LOG_ERROR("[EntityGUIDRegistry] ERROR: GUID not found in registry.");
		}

	std::uint64_t GetRevision() const noexcept { return revision; }

	GUID_128 GetGUIDByEntity(Entity entityID) const {
		auto it = entityToGuidMap.find(entityID);
		if (it != entityToGuidMap.end()) {
			return it->second;
		}

		//ENGINE_LOG_ERROR("[EntityGUIDRegistry] ERROR: Entity ID not found in registry.");
		return GUID_128{ 0, 0 }; // or some invalid GUID value
	}

	void Reset() {
			guidToEntityMap.clear();
			entityToGuidMap.clear();
			++revision;
		}

private:
	EntityGUIDRegistry() = default;

	std::unordered_map<GUID_128, Entity> guidToEntityMap;
	std::unordered_map<Entity, GUID_128> entityToGuidMap;
	std::uint64_t revision = 1;
};
