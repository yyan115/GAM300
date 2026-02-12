#include "pch.h"
#include "Graphics/Model/ModelSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <Transform/TransformComponent.hpp>
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Logging.hpp"
#include "Performance/PerformanceProfiler.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <Graphics/Model/ModelFactory.hpp>

bool ModelSystem::Initialise() 
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    for (const auto& entity : entities) {
        auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
        ENGINE_LOG_DEBUG("Loading model");
        std::string modelPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.modelGUID);
        if (!modelPath.empty())
            modelComp.model = ResourceManager::GetInstance().GetResourceFromGUID<Model>(modelComp.modelGUID, modelPath);
#ifndef ANDROID
        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.shaderGUID);
        if (!shaderPath.empty())
            modelComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(modelComp.shaderGUID, shaderPath);
#else
        ENGINE_LOG_DEBUG("Loading shader");
        std::string shaderPath = ResourceManager::GetPlatformShaderPath("default");
        if (!shaderPath.empty())
            modelComp.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
#endif
        ENGINE_LOG_DEBUG("Loading material");
        std::string materialPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelComp.materialGUID);
        if (!materialPath.empty()) {
            modelComp.material = ResourceManager::GetInstance().GetResourceFromGUID<Material>(modelComp.materialGUID, materialPath);
        }

        if (modelComp.model) {
            ModelFactory::PopulateBoneNameToEntityMap(entity, modelComp.boneNameToEntityMap, *modelComp.model);
            modelComp.childBonesSaved = true;
        }
    }

    ENGINE_PRINT("[ModelSystem] Initialized\n");
    return true;
}

void ModelSystem::Update()
{
    PROFILE_FUNCTION(); // Will automatically show as "Model" in profiler UI
    
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem::Update() called");
#endif
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Get current view mode and check if rendering for editor
    bool isRenderingForEditor = gfxManager.IsRenderingForEditor();
    bool is3DMode = gfxManager.Is3DMode();

    // Get frustum for culling
    const Frustum& frustum = gfxManager.GetFrustum();
    bool enableCulling = gfxManager.IsFrustumCullingEnabled() && !isRenderingForEditor;
    // Reset stats each frame
    cullingStats.Reset();

#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem entities count: %zu", entities.size());
#endif

    // Submit all visible models to the graphics manager
    for (const auto& entity : entities)
    {
        // Skip all 3D models in 2D mode ONLY when rendering for editor
        // Game window should always show all models
        if (isRenderingForEditor && !is3DMode) {
            continue;
        }

        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Processing entity: %u", entity);
#endif
        auto& modelComponent = ecsManager.GetComponent<ModelRenderComponent>(entity);

#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Entity %u: isVisible=%d, model=%p, shader=%p",
         //                 entity, modelComponent.isVisible, modelComponent.model.get(), modelComponent.shader.get());
#endif
        if (!modelComponent.isVisible || !modelComponent.model || !modelComponent.shader)
        {
            continue;
        }

        // FRUSTUM CULLING - ADD THIS BLOCK:
        if (enableCulling && modelComponent.model) 
        {
            // Get world transform
            Matrix4x4 worldMatrix = ecsManager.GetComponent<Transform>(entity).worldMatrix;

            // Get model's bounding box and transform to world space
            AABB worldBounds = modelComponent.model->GetBoundingBox().Transform(worldMatrix.ConvertToGLM());

            if (!frustum.IsBoxVisible(worldBounds))
            {
                cullingStats.culledObjects++;  // Count as culled

                // Log which entity was culled
#ifdef _DEBUG
                //std::cout << "Entity " << entity << " culled\n";
#endif
                continue;
            }
        }

        // Passed culling test, create and submit render item
        auto modelRenderItem = std::make_unique<ModelRenderComponent>(modelComponent);
        auto& entityTransform = ecsManager.GetComponent<Transform>(entity);
        modelRenderItem->transform = entityTransform.worldMatrix;

        // If model doesn't have an animation controller, allow manual manipulation of bone entities.
        if (!modelRenderItem->HasAnimation()) {
            glm::mat4 rootInverse = glm::inverse(entityTransform.worldMatrix.ConvertToGLM());
            for (const auto& [name, boneInfo] : modelRenderItem->model->mBoneInfoMap)
            {
                // Get the child entity representing this bone.
			    Entity boneEntity = modelRenderItem->boneNameToEntityMap[name];

			    // Get the transform of the bone entity.
                glm::mat4 currentWorld = ecsManager.GetComponent<Transform>(boneEntity).worldMatrix.ConvertToGLM();

			    // Write to the final bone matrices.
                modelRenderItem->mFinalBoneMatrices[boneInfo.id] =
                    rootInverse * currentWorld * boneInfo.offset;
		    }
        }

        gfxManager.Submit(std::move(modelRenderItem)); 
    }
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "ModelSystem::Update() completed");
#endif

    // Optional: Print stats every frame (remove in production!)
#ifdef _DEBUG
 //std::cout << "Culling: " << cullingStats.culledObjects << "/" 
 //          << cullingStats.totalObjects << " (" 
 //          << cullingStats.GetCulledPercentage() << "%)\n";
#endif
}

void ModelSystem::Shutdown() 
{
    ENGINE_PRINT("[ModelSystem] Shutdown\n");
}