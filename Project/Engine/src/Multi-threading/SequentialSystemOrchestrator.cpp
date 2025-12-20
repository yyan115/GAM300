#include "pch.h"
#include "Multi-threading/SequentialSystemOrchestrator.hpp"
#include <ECS/ECSRegistry.hpp>
#include <Physics/PhysicsSystem.hpp>
#include <Physics/Kinematics/CharacterControllerSystem.hpp>
#include <TimeManager.hpp>

void SequentialSystemOrchestrator::Update() {
	auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

	// Update systems.
	mainECS.physicsSystem->Update((float)TimeManager::GetFixedDeltaTime(), mainECS);
	mainECS.characterControllerSystem->Update((float)TimeManager::GetFixedDeltaTime());
	mainECS.transformSystem->Update();

	mainECS.animationSystem->Update();

	mainECS.cameraSystem->Update();
	mainECS.lightingSystem->Update();
	mainECS.scriptSystem->Update();
	mainECS.buttonSystem->Update();
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