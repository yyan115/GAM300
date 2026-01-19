
#include "pch.h"
#include "Animation/AnimationSystem.hpp"
#include "Animation/AnimatorController.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "TimeManager.hpp"
#include "Engine.h"

bool AnimationSystem::Initialise()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities)
	{
		auto modelCompOpt = ecsManager.TryGetComponent<ModelRenderComponent>(entity);
		auto animCompOpt = ecsManager.TryGetComponent<AnimationComponent>(entity);
		if (modelCompOpt.has_value() && animCompOpt.has_value())
		{
			auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
			if (modelComp.model == nullptr)
				continue;
			auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);

			try {
				InitialiseAnimationComponent(entity, modelComp, animComp);
			}
			catch (const std::exception& e) {
				ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimationSystem] Exception initializing entity ", entity, ": ", e.what(), "\n");
			}
		}
	}

	ENGINE_PRINT("[AnimationSystem] Initialized\n");
	return true;
}

void AnimationSystem::InitialiseAnimationComponent(Entity entity, ModelRenderComponent& modelComp, AnimationComponent& animComp) {
	// Load animator controller if path is set
	if (!animComp.controllerPath.empty()) {
		AnimatorController controller;
		if (controller.LoadFromFile(animComp.controllerPath)) {
			// Apply state machine configuration
			AnimationStateMachine* stateMachine = animComp.EnsureStateMachine();
			controller.ApplyToStateMachine(stateMachine);

			// Copy clip paths FROM the controller (not from scene data)
			const auto& ctrlClipPaths = controller.GetClipPaths();
			animComp.clipPaths = ctrlClipPaths;
			animComp.clipCount = static_cast<int>(ctrlClipPaths.size());

			ENGINE_PRINT("[AnimationSystem] Loaded controller: ", animComp.controllerPath, " with ", ctrlClipPaths.size(), " clips\n");
		}
		else {
			ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[AnimationSystem] Failed to load controller: ", animComp.controllerPath, "\n");
		}
	}

	Animator* animator = animComp.EnsureAnimator();
	modelComp.SetAnimator(animator);

	if (modelComp.model && !animComp.clipPaths.empty()) {
		animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount(), entity);
		if (!animComp.GetClips().empty()) {
			animator->PlayAnimation(animComp.GetClips()[0].get(), entity);
		}
	}

	ENGINE_PRINT("[AnimationSystem] AnimationComponent initialized for entity ", entity, "\n");
}

void AnimationSystem::Update()
{
	if (Engine::IsEditMode()) {
		return;
	}

	float dt = static_cast<float>(TimeManager::GetDeltaTime());

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities)
	{
		// Skip entities that are inactive in hierarchy (checks parents too)
		if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
			continue;
		}

		auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);

		if (!animComp.enabled) {
			continue;
		}

		if(auto* fsm = animComp.GetStateMachine())
		{
			fsm->Update(dt, entity);
		}

		animComp.Update(dt, entity);
	}
}