#pragma once

#include "Entity.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <new>
#include <memory>
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

// Fixed addresses with element lifetimes limited to slots that have been used.
// The unused byte storage is deliberately left uninitialized.
template<typename T, std::size_t Capacity>
class ComponentSlots {
    struct Slot { alignas(T) std::byte bytes[sizeof(T)]; };
    static_assert(sizeof(Slot) == sizeof(T));
    std::array<Slot, Capacity> slots;
    std::size_t constructed = 0;
public:
    ComponentSlots() noexcept {}
    ComponentSlots(const ComponentSlots&) = delete;
    ComponentSlots& operator=(const ComponentSlots&) = delete;
    ~ComponentSlots() {
        while (constructed != 0) std::destroy_at(&(*this)[--constructed]);
    }
    std::size_t size() const noexcept { return constructed; }
    T& operator[](std::size_t index) noexcept {
        return *std::launder(reinterpret_cast<T*>(slots[index].bytes));
    }
    void emplace_back() {
        assert(constructed < Capacity);
        ::new (static_cast<void*>(slots[constructed].bytes)) T();
        ++constructed;
    }
    void Reset(const T& value) {
        for (std::size_t i = 0; i < constructed; ++i) (*this)[i] = value;
    }
};

template<typename T>
class ComponentArray : public IComponentArray {
public:
    ComponentArray() {
        entityToIndex.fill(INVALID_ENTITY);
    }
    ComponentArray(const ComponentArray&) = delete;
    ComponentArray& operator=(const ComponentArray&) = delete;

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
        if constexpr (constructOnDemand) {
            // Keep previously constructed slots on removal, matching dense-array reuse.
            if (newIndex == componentArray.size()) componentArray.emplace_back();
        }
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
        if constexpr (constructOnDemand) componentArray.Reset(T{});
        else std::fill(componentArray.begin(), componentArray.end(), T{});
        size = 0;
    }

private:
    // Trivial components retain inline storage. Resource-owning components avoid
    // constructing thousands of unused vectors, strings and other owned objects.
    static constexpr bool constructOnDemand = !std::is_trivially_destructible_v<T>;
    using Storage = std::conditional_t<constructOnDemand, ComponentSlots<T, MAX_ENTITIES>, std::array<T, MAX_ENTITIES>>;
    Storage componentArray{};
	std::array<Entity, MAX_ENTITIES> entityToIndex{}; // Sparse entity ID to dense component index lookup.
	std::vector<Entity> indexToEntity{}; // Dense component index to entity ID lookup.

    size_t size{}; // The number of components of type T currently in the component array.
};
