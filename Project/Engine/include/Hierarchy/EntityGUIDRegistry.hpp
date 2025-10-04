#pragma once
#include <unordered_map>
#include "../ECS/Entity.hpp"
#include "../Utilities/GUID.hpp"
#include <iostream>

class EntityGUIDRegistry {
public:
	ENGINE_API static EntityGUIDRegistry& GetInstance();

	void Register(Entity entityID, const GUID_128& guid) {
		guidToEntityMap[guid] = entityID;
		entityToGuidMap[entityID] = guid;
	}

	void Unregister(Entity entityID) {
		auto it = entityToGuidMap.find(entityID);
		if (it != entityToGuidMap.end()) {
			guidToEntityMap.erase(it->second);
			entityToGuidMap.erase(it);
		}
	}

	Entity GetEntityByGUID(const GUID_128& guid) const {
		auto it = guidToEntityMap.find(guid);
		if (it != guidToEntityMap.end()) {
			return it->second;
		}

		std::cerr << "[EntityGUIDRegistry] ERROR: GUID not found in registry." << std::endl;
		return -1; // or some invalid entity value
	}

	GUID_128 GetGUIDByEntity(Entity entityID) const {
		auto it = entityToGuidMap.find(entityID);
		if (it != entityToGuidMap.end()) {
			return it->second;
		}

		std::cerr << "[EntityGUIDRegistry] ERROR: Entity ID not found in registry." << std::endl;
		return GUID_128{ 0, 0 }; // or some invalid GUID value
	}

private:
	EntityGUIDRegistry() = default;

	std::unordered_map<GUID_128, Entity> guidToEntityMap;
	std::unordered_map<Entity, GUID_128> entityToGuidMap;
};