#include "pch.h"
#include "Multi-threading/SequentialSystemOrchestrator.hpp"
#include <ECS/ECSRegistry.hpp>
#include <Physics/PhysicsSystem.hpp>
#include <Physics/Kinematics/CharacterControllerSystem.hpp>
#include <TimeManager.hpp>

void SequentialSystemOrchestrator::Update() {
	auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

	// Clear per-frame cache so all systems share the same hierarchy lookups
	mainECS.ClearActiveHierarchyCache();

	// Update systems.
	// Use actual delta time, not fixed - these are called once per frame, not in a fixed timestep loop
	mainECS.physicsSystem->Update((float)TimeManager::GetDeltaTime(), mainECS);
	mainECS.characterControllerSystem->Update((float)TimeManager::GetDeltaTime(), mainECS);
	mainECS.transformSystem->Update();

	mainECS.animationSystem->Update();

	mainECS.cameraSystem->Update();
	mainECS.lightingSystem->Update();
	mainECS.scriptSystem->Update();
	// Scripts may have changed entity active states; flush cache so subsequent systems see updates
	mainECS.ClearActiveHierarchyCache();
	mainECS.uiAnchorSystem->Update();  // Must run before button/slider to update positions
	mainECS.buttonSystem->Update();
	mainECS.sliderSystem->Update();
	mainECS.videoSystem->Update((float)TimeManager::GetFixedDeltaTime());
	mainECS.spriteAnimationSystem->Update();

	// Update audio (handles AudioManager FMOD update + AudioComponent updates)
	if (mainECS.audioSystem)
	{
		mainECS.audioSystem->Update((float)TimeManager::GetDeltaTime());
	}
}

void SequentialSystemOrchestrator::Draw() {
	auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

	if (mainECS.modelSystem)
	{
		mainECS.modelSystem->Update();
	}
	if (mainECS.textSystem)
	{
		mainECS.textSystem->Update();
	}

	if (mainECS.spriteSystem)
	{
		mainECS.spriteSystem->Update();
	}

	if (mainECS.particleSystem)
	{
		mainECS.particleSystem->Update();
	}

	if (mainECS.debugDrawSystem)
	{
		mainECS.debugDrawSystem->Update();
	}
}