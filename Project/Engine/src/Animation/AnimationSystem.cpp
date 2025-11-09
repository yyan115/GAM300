
#include "pch.h"
#include "Animation/AnimationSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
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
			auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);

			Animator* animator = animComp.EnsureAnimator();
			modelComp.SetAnimator(animator);

			if (modelComp.model && !animComp.clipPaths.empty()) {
				animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount());
				if (!animComp.GetClips().empty()) {
					animComp.Play();
				}
			}

			ENGINE_PRINT("[AnimationSystem] AnimationComponent initialized for entity ", entity, "\n");
		}
	}

	ENGINE_PRINT("[AnimationSystem] Initialized\n");
	return true;
}

void AnimationSystem::Update()
{
	if (Engine::IsEditMode()) {
		return;
	}

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities)
	{
		// Skip inactive entities
		if (ecsManager.HasComponent<ActiveComponent>(entity)) {
			auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
			if (!activeComp.isActive) {
				continue;
			}
		}

		auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);

		if (!animComp.enabled) {
			continue;
		}

		animComp.Update(static_cast<float>(TimeManager::GetDeltaTime()));
	}
}