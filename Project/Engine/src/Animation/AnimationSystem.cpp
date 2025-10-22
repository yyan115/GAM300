
#include "pch.h"
#include "Animation/AnimationSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "TimeManager.hpp"

bool AnimationSystem::Initialise()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities)
	{
		auto modelCompOpt = ecsManager.TryGetComponent<ModelRenderComponent>(entity);
		if (modelCompOpt.has_value())
		{
			auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
			auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);
			if (modelComp.model)
			{
				animComp.SetModel(&*modelComp.model);
				animComp.AddClipFromFile("Resources/Models/kachujin/Animation/Slash.fbx");
				animComp.Play();
				std::cout << "[AnimationSystem] Initialized AnimationComponent for Entity " << entity << " with model and animation clip.\n";
			}
			else
				std::cout << "[AnimationSystem] Warning: Entity " << entity << " has no model assigned for AnimationComponent.\n";
		}
	}
	
	ENGINE_PRINT("[AnimationSystem] Initialized\n");
	return true;
}

void AnimationSystem::Update()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities) 
	{
		auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);
		animComp.Update(TimeManager::GetDeltaTime());
	}
}