#include "pch.h"
#include <iostream>
#include <assert.h>
#include "ECS/ECSRegistry.hpp"

ECSRegistry& ECSRegistry::GetInstance() {
	static ECSRegistry instance;
	return instance;
}

ECSManager& ECSRegistry::CreateECSManager(const std::string& name) {
	assert(ecsManagers.find(name) == ecsManagers.end() && "ECSManager with the given name already exists.");

	ecsManagers[name] = std::make_unique<ECSManager>();

	// If there's no active ECSManager, set the newly created one as active.
	if (activeECSManagerName.empty()) {
		SetActiveECSManager(name);
	}

	std::cout << "[ECSRegistry] Created ECSManager '" << name << "'." << std::endl;
	return *ecsManagers[name];
}

ECSManager& ECSRegistry::GetECSManager(const std::string& name) {
	assert(ecsManagers.find(name) != ecsManagers.end() && "ECSManager with the given name does not exist.");
	return *ecsManagers[name];
}

void ECSRegistry::DestroyECSManager(const std::string& name) {
	assert(ecsManagers.find(name) != ecsManagers.end() && "ECSManager with the given name does not exist.");
	assert(name != activeECSManagerName && "Destroying active ECS manager.");
	
	ecsManagers.erase(name);
}

void ECSRegistry::SetActiveECSManager(const std::string& name) {
	assert(ecsManagers.find(name) != ecsManagers.end() && "ECSManager with the given name does not exist.");
	activeECSManagerName = name;
}

ECSManager& ECSRegistry::GetActiveECSManager() {
	assert(!activeECSManagerName.empty() && "No active ECSManager set.");
	return *ecsManagers[activeECSManagerName];
}
