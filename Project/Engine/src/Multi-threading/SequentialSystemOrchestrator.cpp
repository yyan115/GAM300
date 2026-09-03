#include "pch.h"
#include "Multi-threading/SequentialSystemOrchestrator.hpp"
#include <ECS/ECSRegistry.hpp>
#include <Physics/PhysicsSystem.hpp>
#include <Physics/Kinematics/CharacterControllerSystem.hpp>
#include "Graphics/GraphicsManager.hpp"
#include <TimeManager.hpp>
#include "Logging.hpp"

void SequentialSystemOrchestrator::Update() {
	auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

	// Clear per-frame cache so all systems share the same hierarchy lookups
	{
		PROFILE_SCOPED("HierarchyCache::Clear");
		mainECS.ClearActiveHierarchyCache();
	}

	// Update systems.
	PROFILE_PLOT_TIMED("Script",              mainECS.scriptSystem->Update());

	// Scripts may have changed entity active states. Refresh before any system
	// consumes the hierarchy cache.
	{
		PROFILE_SCOPED("HierarchyCache::PostScriptRefresh");
		mainECS.ClearActiveHierarchyCache();
	}

	PROFILE_PLOT_TIMED("Physics",             mainECS.physicsSystem->Update((float)TimeManager::GetDeltaTime(), mainECS));
	PROFILE_PLOT_TIMED("CharacterController", mainECS.characterControllerSystem->Update((float)TimeManager::GetDeltaTime(), mainECS));
	PROFILE_PLOT_TIMED("UIAnchor",            mainECS.uiAnchorSystem->Update());
	PROFILE_PLOT_TIMED("Transform",           mainECS.transformSystem->Update());
	PROFILE_PLOT_TIMED("Animation",           mainECS.animationSystem->Update());
	PROFILE_PLOT_TIMED("Camera",              mainECS.cameraSystem->Update());
	if (auto* activeCamera = mainECS.cameraSystem->GetActiveCamera())
	{
		auto& graphics = GraphicsManager::GetInstance();
		graphics.SetCamera(activeCamera);
		graphics.UpdateFrustum();
	}
	PROFILE_PLOT_TIMED("Lighting",            mainECS.lightingSystem->Update());

	PROFILE_PLOT_TIMED("Button",          mainECS.buttonSystem->Update());
	PROFILE_PLOT_TIMED("Slider",          mainECS.sliderSystem->Update());
	PROFILE_PLOT_TIMED("Video",           mainECS.videoSystem->Update((float)TimeManager::GetFixedDeltaTime()));
	PROFILE_PLOT_TIMED("Dialogue",        mainECS.dialogueSystem->Update((float)TimeManager::GetDeltaTime()));
	PROFILE_PLOT_TIMED("SpriteAnimation", mainECS.spriteAnimationSystem->Update());

	// UI callbacks and dialogue may toggle entities. Refresh after all of them so
	// audio and draw systems observe those changes during this frame.
	{
		PROFILE_SCOPED("HierarchyCache::DrawRefresh");
		mainECS.ClearActiveHierarchyCache();
	}

	// Update audio (handles AudioManager FMOD update + AudioComponent updates)
	if (mainECS.audioSystem)
	{
		PROFILE_PLOT_TIMED("Audio", mainECS.audioSystem->Update((float)TimeManager::GetDeltaTime()));
	}
}

void SequentialSystemOrchestrator::Draw() {
	auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

	if (mainECS.modelSystem)
		PROFILE_PLOT_TIMED("Model", mainECS.modelSystem->Update());
	if (mainECS.textSystem)
		PROFILE_PLOT_TIMED("Text", mainECS.textSystem->Update());
	if (mainECS.spriteSystem)
		PROFILE_PLOT_TIMED("Sprite", mainECS.spriteSystem->Update());
	if (mainECS.particleSystem)
		PROFILE_PLOT_TIMED("Particle", mainECS.particleSystem->Update());
#ifndef ANDROID
	if (mainECS.debugDrawSystem)
		PROFILE_PLOT_TIMED("DebugDraw", mainECS.debugDrawSystem->Update());
	if (mainECS.fogSystem)
		PROFILE_PLOT_TIMED("Fog", mainECS.fogSystem->Update());
#endif
}
