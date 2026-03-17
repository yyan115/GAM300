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
#include "ECS/LayerComponent.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Graphics/BloomComponent.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <Graphics/Model/ModelFactory.hpp>
#include <Graphics/Instancing/InstancingManager.hpp>

bool ModelSystem::Initialise() 
{
    // DISABLE COLOR AND DEPTH WRITES BEFORE DUMMY DRAWING!
    // This stops exploding vertices from rendering to the editor viewport.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

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
            ModelFactory::PopulateBoneNameToEntityMap(entity, modelComp.boneNameToEntityMap, *modelComp.model, true);
            modelComp.childBonesSaved = true;

            // Force shader compilation / activation
            modelComp.shader->Activate();

            // Force textures to page into VRAM
            if (modelComp.material) {
                modelComp.material->ApplyToShader(*modelComp.shader);
            }

            for (auto& mesh : modelComp.model->meshes)
            {
                // This calls your setupMesh() and sets vaoSetup = true
                // while the loading screen is still up!
                mesh.Prewarm();

                // THE DUMMY DRAW: Force the driver to execute the pipeline!
                // We only draw 3 indices (1 triangle) to make it lightning fast.
                mesh.vao.Bind();
                glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
                mesh.vao.Unbind();
            }
        }
    }

    // Restore normal graphics state for gameplay
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Or whatever your default is

    ENGINE_PRINT("[ModelSystem] Dummy Draw Prewarm complete! Driver is fully warmed up.\n");

    ENGINE_PRINT("[ModelSystem] Initialized\n");
    return true;
}

void ModelSystem::Update()
{
    PROFILE_FUNCTION(); // Will automatically show as "Model" in profiler UI

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();
    InstancingManager& instancing = InstancingManager::GetInstance();

    // Get current view mode and check if rendering for editor
    bool isRenderingForEditor = gfxManager.IsRenderingForEditor();
    bool is3DMode = gfxManager.Is3DMode();

    // Get frustum for culling
    const Frustum& frustum = gfxManager.GetFrustum();
    bool enableCulling = gfxManager.IsFrustumCullingEnabled() && !isRenderingForEditor;
    // Reset stats each frame
    cullingStats.Reset();


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

        auto& modelComponent = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelComponent.isVisible || !modelComponent.model || !modelComponent.shader)
        {
            continue;
        }

        // Get world transform
        Matrix4x4 worldMatrix = ecsManager.GetComponent<Transform>(entity).worldMatrix;
        glm::mat4 glmWorldMatrix = worldMatrix.ConvertToGLM();

       
        if (instancing.IsEnabled())
        {
            // Gather per-entity bloom data for instancing
            glm::vec3 entityBloomColor(0.0f);
            float entityBloomIntensity = 0.0f;
            if (ecsManager.HasComponent<BloomComponent>(entity)) {
                auto& bloom = ecsManager.GetComponent<BloomComponent>(entity);
                if (bloom.enabled) {
                    entityBloomColor = bloom.bloomColor;
                    entityBloomIntensity = bloom.bloomIntensity;
                }
            }

            bool wasInstanced = instancing.TryAddInstance(modelComponent, glmWorldMatrix, entityBloomColor, entityBloomIntensity);

            if (wasInstanced)
            {
                // Instance was added to a batch (or culled), skip individual submission
                cullingStats.renderedObjects++;  // Count as handled
                continue;
            }
        }

        // =====================================================================
        // Fallback: Not instanceable, render individually
        // =====================================================================

        // Frustum culling for non-instanced objects
        if (enableCulling && modelComponent.model)
        {
            AABB worldBounds = modelComponent.model->GetBoundingBox().Transform(glmWorldMatrix);
            if (!frustum.IsBoxVisible(worldBounds))
            {
                cullingStats.culledObjects++;
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

        // Per-entity bloom emission
        if (ecsManager.HasComponent<BloomComponent>(entity)) {
            auto& bloom = ecsManager.GetComponent<BloomComponent>(entity);
            if (bloom.enabled) {
                modelRenderItem->bloomColor = bloom.bloomColor;
                modelRenderItem->bloomIntensity = bloom.bloomIntensity;
            }
        }

        // Tag items on excluded layers for deferred rendering
        uint32_t exMask = PostProcessingManager::GetInstance().GetExcludedLayerMask();
        if (exMask != 0) {
            int layerIdx = GetEffectiveLayerIndex(entity, ecsManager);
            if (exMask & (1u << layerIdx))
                modelRenderItem->excludeFromPostProcess = true;
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