#pragma once

#include <optional>
#include <assert.h>
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
		components[typeName] = nextComponentID;
		componentArrays[typeName] = std::make_shared<ComponentArray<T>>();
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
		const std::string& typeName = GetReadableTypeName<T>();
		auto it = components.find(typeName);
		assert(it != components.end() && "Component not registered before use.");
		return it->second;
	}

	template <typename T>
	void AddComponent(Entity entity, T component) {
		GetComponentArray<T>()->InsertComponent(entity, component);
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
	ComponentID nextComponentID{}; // The next available component ID to assign.

	template<typename T>
	std::shared_ptr<ComponentArray<T>> GetComponentArray() {
		const std::string& typeName = GetReadableTypeName<T>();

		auto it = componentArrays.find(typeName);
		assert(it != componentArrays.end() && "Component not registered before use.");

		return std::static_pointer_cast<ComponentArray<T>>(it->second);
	}
};
