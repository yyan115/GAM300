#include "pch.h"
#include "Graphics/LightManager.hpp"
#include "Graphics/Model/ModelSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <Transform/TransformComponent.hpp>

bool ModelSystem::Initialise() 
{
    std::cout << "[ModelSystem] Initialized" << std::endl;
    return true;
}

void ModelSystem::Update() 
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Submit all visible models to the graphics manager
    for (const auto& entity : entities)
    {
        auto& modelComponent = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (modelComponent.isVisible && modelComponent.model && modelComponent.shader)
        {
            auto modelRenderItem = std::make_unique<ModelRenderComponent>(modelComponent);
            modelRenderItem->transform = gfxManager.ConvertMatrix4x4ToGLM(ecsManager.GetComponent<Transform>(entity).model);

            gfxManager.Submit(std::move(modelRenderItem));
        }
    }
}

void ModelSystem::Shutdown() 
{
    std::cout << "[ModelSystem] Shutdown" << std::endl;
}