#pragma once

#include "Entity.hpp"
#include <unordered_map>
#include <optional>
#include <iostream>
#include <assert.h>
#include "Logging.hpp"

class IComponentArray {
public:
    /**
     * \brief Virtual destructor.
     */
    virtual ~IComponentArray() = default;

    /**
     * \brief Handles the removal of a component for a destroyed entity.
     * \param entity The entity that was destroyed.
     */
    virtual void EntityDestroyed(Entity entity) = 0;
    virtual void AllEntitiesDestroyed() = 0;
};

template<typename T>
class ComponentArray : public IComponentArray {
public:
    inline void InsertComponent(Entity entity, T component) {
        if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "Component added to same entity more than once.\n");
			//std::cerr << "Component added to same entity more than once." << std::endl;
            return;
        }

        size_t newIndex = size;
        entityToIndexMap[entity] = newIndex;
        indexToEntityMap[newIndex] = entity;
        componentArray[newIndex] = component;
		++size;
    }

    inline void RemoveComponent(Entity entity) {
        if (entityToIndexMap.find(entity) == entityToIndexMap.end()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "Removing non-existent component.\n");
			//std::cerr << "Removing non-existent component." << std::endl;
            return;
        }

		// Replace the component to be removed with the last component to maintain density.
        size_t indexOfRemovedEntity = entityToIndexMap[entity];
        size_t indexOfLastElement = size - 1;
        componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

		// Update the maps to reflect this change.
        Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
        entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
        indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

        entityToIndexMap.erase(entity);
        indexToEntityMap.erase(indexOfLastElement);

        --size;
    }

    inline T& GetComponent(Entity entity) {
        assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Retrieving non-existent component.");
		return componentArray[entityToIndexMap[entity]];
    }

    inline std::optional<std::reference_wrapper<T>> TryGetComponent(Entity entity) {
        if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
            return componentArray[entityToIndexMap[entity]];
        }
        return std::nullopt;
    }

    inline void EntityDestroyed(Entity entity) override {
        // Remove the component if the entity has the component.
        if (entityToIndexMap.find(entity) != entityToIndexMap.end())
            RemoveComponent(entity);
    }

    inline void AllEntitiesDestroyed() override {
        entityToIndexMap.clear();
        indexToEntityMap.clear();
        std::fill(componentArray.begin(), componentArray.end(), T{});
        size = 0;
    }

private:
	std::array<T, MAX_ENTITIES> componentArray{}; // Array that stores densely-packed components of type T for all entities that have this component.
	std::unordered_map<Entity, size_t> entityToIndexMap{}; // Map from an entity ID to a component array index.
	std::unordered_map<size_t, Entity> indexToEntityMap{}; // Map from a component array index to an entity ID.

    size_t size{}; // The number of components of type T currently in the component array.
};