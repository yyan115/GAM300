#pragma once

#include <unordered_map>

#include "Signature.hpp"
#include "System.hpp"
#include <memory>

class SystemManager {
public:
	template <typename T>
	std::shared_ptr<T> RegisterSystem() {
		std::string typeName = typeid(T).name();

		assert(systems.find(typeName) == systems.end() && "Registering system more than once.");

		// Create a shared pointer for the system and return it.
		auto system = std::make_shared<T>();
		systems[typeName] = system;
		return system;
	}

	template <typename T>
	void SetSignature(Signature signature) {
		std::string typeName = typeid(T).name();

		assert(systems.find(typeName) != systems.end() && "System used before registered.");

		signatures[typeName] = signature;
	}

	void EntityDestroyed(Entity entity) {
		for (auto const& pair : systems) {
			auto const& system = pair.second;
			system->entities.erase(entity);
		}
	}

	void AllEntitiesDestroyed() {
		for (auto const& pair : systems) {
			auto const& system = pair.second;
			system->entities.clear();
		}
	}

	void OnEntitySignatureChanged(Entity entity, Signature entitySignature) {
		for (const auto& pair : systems) {
			const auto& typeName = pair.first;
			const auto& system = pair.second;
			const auto& systemSignature = signatures[typeName];

			// If the entity's signature matches ANY of the system's signature, add it to the set.
			// If the entity has ANY of the required components for the system, add it to the set.
			if ((entitySignature & systemSignature).any()) {
				system->entities.insert(entity);
			} else {
				system->entities.erase(entity);
			}
		}
	}

	// Get all registered systems (for profiling UI)
	const std::unordered_map<std::string, std::shared_ptr<System>>& GetAllSystems() const {
		return systems;
	}
	
	template <typename T>
	std::shared_ptr<T> GetSystem() const {
		std::string typeName = typeid(T).name();
		auto it = systems.find(typeName);
		if (it == systems.end()) return nullptr;
		return std::static_pointer_cast<T>(it->second);
	}

private:
	std::unordered_map<std::string, Signature> signatures{}; // Map from system type name to its signature.
	std::unordered_map<std::string, std::shared_ptr<System>> systems{}; // Map from system type name to a system instance.
};