#pragma once

#include <unordered_map>
#include "ECSManager.hpp"
#include "../Engine.h"  // For ENGINE_API macro
#include "Logging.hpp"

/**
 * \class ECSRegistry
 * \brief Singleton registry that manages all active ECSManager instances (worlds/scenes) in the application.
 *
 * The ECSRegistry stores and organizes multiple ECSManager instances, each representing an independent world or scene.
 * This design improves engine flexibility by enabling better scene management, isolated simulations, and potential
 * parallel execution of different worlds.
 */
class ENGINE_API ECSRegistry {
public:
	// Delete copy constructor and assignment operator to enforce singleton pattern.
	ECSRegistry(const ECSRegistry&) = delete;
	ECSRegistry& operator=(const ECSRegistry&) = delete;

	static ECSRegistry& GetInstance();

	ECSManager& CreateECSManager(const std::string& name);
	void DestroyECSManager(const std::string& name);
	ECSManager& GetECSManager(const std::string& name);

	void SetActiveECSManager(const std::string& name);
	ECSManager& GetActiveECSManager();

	void RenameECSManager(const std::string& oldName, const std::string& newName) {
		if (oldName == newName) return;

		assert(ecsManagers.find(oldName) != ecsManagers.end() && "ECSManager with the given old name does not exist.");
		assert(ecsManagers.find(newName) == ecsManagers.end() && "ECSManager with the given new name already exists.");
		
		ecsManagers[newName] = std::move(ecsManagers[oldName]);
		ecsManagers.erase(oldName);

		if (oldName == activeECSManagerName) {
			activeECSManagerName = newName;
		}
		ENGINE_PRINT("[ECSRegistry] Renamed ECSManager from '", oldName, "' to '", newName, "'.\n");
	}

private:
	ECSRegistry() {};
	~ECSRegistry() {};

	std::unordered_map<std::string, std::unique_ptr<ECSManager>> ecsManagers; // Map from scene name to ECSManager instance.
	std::string activeECSManagerName{}; // Name of the currently active ECSManager.
};