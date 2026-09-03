#pragma once

#include <optional>
#include <assert.h>
#include <array>
#include <string>

#include "Component.hpp"
#include "ComponentArray.hpp"

namespace {
	template <typename T>
	const std::string& GetReadableTypeName() {
		static const std::string typeName = []() {
			const char* rawName = typeid(T).name();
			std::string name = rawName;
			if (name.find("struct ") == 0) {
				name = name.substr(7);
			}
			else if (name.find("class ") == 0) {
				name = name.substr(6);
			}
			return name;
		}();
		return typeName;
	}
}

class ComponentManager {
public:
	template <typename T>
	void RegisterComponent() {
		const std::string& typeName = GetReadableTypeName<T>();
		assert(components.find(typeName) == components.end() && "Registering component type more than once.");
		assert(nextComponentID < MAX_COMPONENTS && "Exceeded maximum registered component count.");

		auto componentArray = std::make_shared<ComponentArray<T>>();
		components[typeName] = nextComponentID;
		componentArrays[typeName] = componentArray;
		componentArraysByID[nextComponentID] = componentArray.get();
		++nextComponentID;
	}

	template<typename T>
	bool IsRegistered() const
	{
		const std::string& typeName = GetReadableTypeName<T>();
		return components.find(typeName) != components.end();
	}

	template <typename T>
	ComponentID GetComponentID() {
		return GetCachedComponentID<T>();
	}

	template <typename T>
	void AddComponent(Entity entity, T component) {
		GetComponentArray<T>()->InsertComponent(entity, std::move(component));
	}

	template <typename T>
	void RemoveComponent(Entity entity) {
		GetComponentArray<T>()->RemoveComponent(entity);
	}

	template <typename T>
	T& GetComponent(Entity entity) {
		return GetComponentArray<T>()->GetComponent(entity);
	}

	template <typename T>
	std::optional<std::reference_wrapper<T>> TryGetComponent(Entity entity) {
		return GetComponentArray<T>()->TryGetComponent(entity);
	}

	template <typename T>
	bool HasComponent(Entity entity) {
		return GetComponentArray<T>()->Contains(entity);
	}

	void EntityDestroyed(Entity entity) {
		for (auto const& pair : componentArrays) {
			auto const& componentArray = pair.second;
			componentArray->EntityDestroyed(entity);
		}
	}

	void AllEntitiesDestroyed() {
		for (auto const& pair : componentArrays) {
			auto const& componentArray = pair.second;
			componentArray->AllEntitiesDestroyed();
		}
	}

private:
	std::unordered_map<std::string, ComponentID> components{}; // Map from component type name to component ID.
	std::unordered_map<std::string, std::shared_ptr<IComponentArray>> componentArrays{}; // Map from component type name to component array.
	std::array<IComponentArray*, MAX_COMPONENTS> componentArraysByID{}; // Non-owning fast lookup by stable component ID.
	ComponentID nextComponentID{}; // The next available component ID to assign.

	template<typename T>
	ComponentID GetCachedComponentID() const {
		// Every ECSManager registers components in the same order, which is already
		// required for entity signatures. Resolve each type once per binary, then
		// use its compact ID for all frame-time component access.
		static const ComponentID cachedID = [this]() {
			const std::string& typeName = GetReadableTypeName<T>();
			auto it = components.find(typeName);
			assert(it != components.end() && "Component not registered before use.");
			return it->second;
		}();

#if !defined(NDEBUG) && !defined(ANDROID)
		const std::string& typeName = GetReadableTypeName<T>();
		auto it = components.find(typeName);
		assert(it != components.end() && it->second == cachedID &&
			"Component registration order differs between ECS managers.");
#endif
		return cachedID;
	}

	template<typename T>
	ComponentArray<T>* GetComponentArray() {
		const ComponentID componentID = GetCachedComponentID<T>();
		IComponentArray* componentArray = componentArraysByID[componentID];
		assert(componentArray != nullptr && "Component not registered before use.");
		return static_cast<ComponentArray<T>*>(componentArray);
	}
};
