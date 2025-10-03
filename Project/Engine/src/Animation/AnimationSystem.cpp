
#include "pch.h"#include "Animation/AnimationSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "TimeManager.hpp"

bool AnimationSystem::Initialise()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities)
	{
		auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
		auto& animComp = ecsManager.GetComponent<AnimationComponent>(entity);
		if(modelComp.model)
		{
			animComp.SetModel(&*modelComp.model);
			animComp.AddClipFromFile("Resources/Models/kachujin/Animation/KachujinAnimation.fbx");
			animComp.Play();
		}
		else
			std::cout << "[AnimationSystem] Warning: Entity " << entity << " has no model assigned for AnimationComponent.\n";
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