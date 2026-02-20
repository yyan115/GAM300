
#include "pch.h"
#include "Animation/AnimationSystem.hpp"
#include "Animation/AnimatorController.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "TimeManager.hpp"
#include "Engine.h"
#include <execution>

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
				animComp.ResetPreview(entity);
				animComp.ResetForPlay(entity); // Reset animator to time 0 for fresh start
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
	Animator* animator = animComp.EnsureAnimator();

	if (animComp.GetClips().size() < animComp.clipPaths.size()) {
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

		modelComp.SetAnimator(animator);

		if (modelComp.model && !animComp.clipPaths.empty()) {
			animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount(), entity);
		}
	}

	// Play the entry state's animation clip (not just first clip)
	if (!animComp.GetClips().empty()) {
		size_t clipToPlay = 0;
		AnimationStateMachine* sm = animComp.GetStateMachine();
		if (sm) {
			std::string entryState = sm->GetEntryState();
			const AnimStateConfig* entryConfig = sm->GetState(entryState);
			if (entryConfig && entryConfig->clipIndex < animComp.GetClips().size()) {
				clipToPlay = entryConfig->clipIndex;
			}
			// Initialize the state machine to the entry state
			sm->SetInitialState(entryState, entity);
		}
		animComp.SetClip(clipToPlay, entity);
		animator->PlayAnimation(animComp.GetClips()[clipToPlay].get(), entity);
	}

	ENGINE_PRINT("[AnimationSystem] AnimationComponent initialized for entity ", entity, "\n");
}

void AnimationSystem::Update()
{
	PROFILE_FUNCTION();
	if (Engine::IsEditMode()) {
		return;
	}

	float dt = static_cast<float>(TimeManager::GetDeltaTime());

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	//// ------------------------------------------------------------------------
	//// STEP 1: COLLECTION PHASE (Sequential)
	//// ------------------------------------------------------------------------
	//// Safely gather all entities that actually need animation updates.
	//// We do this sequentially because checking hierarchy and pushing to a vector
	//// is extremely fast, and doing it in parallel requires expensive mutex locks.

	//std::vector<Entity> activeEntities;
	//activeEntities.reserve(entities.size()); // Prevent reallocation

	//for (const auto& entity : entities)
	//{
	//	if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
	//		continue;
	//	}

	//	auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);
	//	if (!animComp.enabled) {
	//		continue;
	//	}

	//	auto& transform = ecsManager.GetComponent<Transform>(entity);
	//	auto camera = ecsManager.cameraSystem->GetActiveCamera(); // Assuming you can get the camera pos

	//	// Simple distance-based frustum check
	//	float distSq = glm::length(transform.worldPosition.ConvertToGLM() - camera->Position);

	//	// If the entity is more than 30 units away, don't update its animation this frame!
	//	if (distSq > 30.0f) {
	//		//std::cout << "[AnimationSystem] Entity > 100 units away, skipping animation update." << std::endl;
	//		continue;
	//	}

	//	activeEntities.push_back(entity);
	//}

	//// ------------------------------------------------------------------------
	//// STEP 2: PROCESSING PHASE (Parallel)
	//// ------------------------------------------------------------------------
	//// Divide the heavy matrix math across all available CPU cores.

	//std::for_each(std::execution::par, activeEntities.begin(), activeEntities.end(),
	//	[&ecsManager, dt](Entity entity)
	//	{
	//		// This lambda is executed simultaneously across multiple threads
	//		auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);

	//		if (auto* fsm = animComp.GetStateMachine())
	//		{
	//			fsm->Update(dt, entity);
	//		}

	//		animComp.Update(dt, entity);
	//	});

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

		auto& transform = ecsManager.GetComponent<Transform>(entity);
		auto camera = ecsManager.cameraSystem->GetActiveCamera(); // Assuming you can get the camera pos

		// Simple distance-based frustum check
		float distSq = glm::length(transform.worldPosition.ConvertToGLM() - camera->Position);

		// If the entity is more than 30 units away, don't update its animation this frame!
		if (distSq > 30.0f) {
			//std::cout << "[AnimationSystem] Entity > 100 units away, skipping animation update." << std::endl;
			continue;
		}

		if(auto* fsm = animComp.GetStateMachine())
		{
			fsm->Update(dt, entity);
		}

		animComp.Update(dt, entity);
	}
}