#include "pch.h"
#include "Graphics/LightManager.hpp"
#include "Graphics/Model/ModelSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <Transform/TransformComponent.hpp>

#ifdef ANDROID
#include <android/log.h>
#endif

bool ModelSystem::Initialise() 
{
    std::cout << "[ModelSystem] Initialized" << std::endl;
    return true;
}

void ModelSystem::Update()
{
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem::Update() called");
#endif
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem entities count: %zu", entities.size());
#endif

    // Submit all visible models to the graphics manager
    for (const auto& entity : entities)
    {
#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Processing entity: %u", entity);
#endif
        auto& modelComponent = ecsManager.GetComponent<ModelRenderComponent>(entity);

#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Entity %u: isVisible=%d, model=%p, shader=%p",
         //                 entity, modelComponent.isVisible, modelComponent.model.get(), modelComponent.shader.get());
#endif
        if (modelComponent.isVisible && modelComponent.model && modelComponent.shader)
        {
#ifdef ANDROID
           // __android_log_print(ANDROID_LOG_INFO, "GAM300", "Submitting model for entity: %u", entity);
#endif
            auto modelRenderItem = std::make_unique<ModelRenderComponent>(modelComponent);
            modelRenderItem->transform = gfxManager.ConvertMatrix4x4ToGLM(ecsManager.GetComponent<Transform>(entity).worldMatrix);

            gfxManager.Submit(std::move(modelRenderItem));
        }
#ifdef ANDROID
        else {
            //__android_log_print(ANDROID_LOG_WARN, "GAM300", "Entity %u: model not visible or missing components - isVisible=%d, model=%p, shader=%p",
                       //       entity, modelComponent.isVisible, modelComponent.model.get(), modelComponent.shader.get());
        }
#endif
    }
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem::Update() completed");
#endif
}

void ModelSystem::Shutdown() 
{
    std::cout << "[ModelSystem] Shutdown" << std::endl;
}