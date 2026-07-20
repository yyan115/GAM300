#pragma once

#include "Entity.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <iostream>
#include <stdexcept>
#include <vector>
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
    ComponentArray() {
        entityToIndex.fill(INVALID_ENTITY);
    }

    inline void InsertComponent(Entity entity, T component) {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        if (entity >= MAX_ENTITIES) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "Adding component to invalid entity.\n");
            return;
        }
        if (entityToIndex[entity] != INVALID_ENTITY) {
            //ENGINE_PRINT(EngineLogging::LogLevel::Error, "Component added to same entity more than once.\n");
            return;
        }

        assert(size < MAX_ENTITIES && "Component array capacity exceeded.");
        const Entity newIndex = static_cast<Entity>(size);
        entityToIndex[entity] = newIndex;
        indexToEntity.push_back(entity);
        componentArray[newIndex] = component;
		++size;
    }

    inline void RemoveComponent(Entity entity) {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        if (entity >= MAX_ENTITIES) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "Removing component from invalid entity.\n");
            return;
        }
        const Entity indexOfRemovedEntity = entityToIndex[entity];
        if (indexOfRemovedEntity == INVALID_ENTITY) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "Removing non-existent component.\n");
            return;
        }

		// Replace the component to be removed with the last component to maintain density.
        const Entity indexOfLastElement = static_cast<Entity>(size - 1);
        if (indexOfRemovedEntity != indexOfLastElement) {
            componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

			// Update the sparse and dense indices for the moved component.
            const Entity entityOfLastElement = indexToEntity[indexOfLastElement];
            entityToIndex[entityOfLastElement] = indexOfRemovedEntity;
            indexToEntity[indexOfRemovedEntity] = entityOfLastElement;
        }

        entityToIndex[entity] = INVALID_ENTITY;
        indexToEntity.pop_back();
        --size;
    }

    inline T& GetComponent(Entity entity) {
		assert(entity < MAX_ENTITIES && "Entity out of range.");
		if (entity >= MAX_ENTITIES) {
			throw std::out_of_range("Retrieving component for invalid entity.");
		}
		const Entity index = entityToIndex[entity];
        assert(index != INVALID_ENTITY && "Retrieving non-existent component.");
        if (index == INVALID_ENTITY) {
            throw std::out_of_range("Retrieving non-existent component.");
        }
		return componentArray[index];
    }

    inline std::optional<std::reference_wrapper<T>> TryGetComponent(Entity entity) {
        if (entity < MAX_ENTITIES) {
			const Entity index = entityToIndex[entity];
			if (index != INVALID_ENTITY) {
				return componentArray[index];
			}
        }
        return std::nullopt;
    }

	inline bool Contains(Entity entity) const {
		return entity < MAX_ENTITIES && entityToIndex[entity] != INVALID_ENTITY;
	}

    inline void EntityDestroyed(Entity entity) override {
        // Remove the component if the entity has the component.
		if (Contains(entity))
            RemoveComponent(entity);
    }

    inline void AllEntitiesDestroyed() override {
        entityToIndex.fill(INVALID_ENTITY);
        indexToEntity.clear();
        std::fill(componentArray.begin(), componentArray.end(), T{});
        size = 0;
    }

private:
	std::array<T, MAX_ENTITIES> componentArray{}; // Array that stores densely-packed components of type T for all entities that have this component.
	std::array<Entity, MAX_ENTITIES> entityToIndex{}; // Sparse entity ID to dense component index lookup.
	std::vector<Entity> indexToEntity{}; // Dense component index to entity ID lookup.

    size_t size{}; // The number of components of type T currently in the component array.
};
