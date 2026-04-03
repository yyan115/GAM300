#include "pch.h"
#include "Graphics/Instancing/InstancingManager.hpp"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Graphics/Material.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Frustum/Frustum.hpp"
#include "Logging.hpp"
#include <ECS/ECSRegistry.hpp>
#include "Graphics/GraphicsManager.hpp"

InstancingManager& InstancingManager::GetInstance() 
{
    static InstancingManager instance;
    return instance;
}

void InstancingManager::BeginFrame()
{
    m_stats.Reset();

    for (auto& [key, batch] : m_batches)
    {
        batch.Clear();
    }

    m_sortedBatches.clear();
}

void InstancingManager::EndFrame()
{
    m_stats.batchCount = 0;

    for (auto& [key, batch] : m_batches)
    {
        if (!batch.IsEmpty())
        {
            m_stats.batchCount++;
        }
    }
}

bool InstancingManager::TryAddInstance(const ModelRenderComponent& component, const glm::mat4& worldMatrix, const glm::vec3& bloomColor, float bloomIntensity)
{
    m_stats.totalObjects++;

    if (!m_enabled)
    {
        return false;
    }

    if (!IsInstanceable(component))
    {
        m_stats.nonInstancedObjects++;
        return false;
    }

    if (m_frustum && component.model)
    {
        AABB worldBounds = component.model->GetBoundingBox().Transform(worldMatrix);
        if (!m_frustum->IsBoxVisible(worldBounds))
        {
            m_stats.culledObjects++;
            return true;  // Return true to indicate "handled" (culled)
        }
    }

    BatchKey key{
        component.model.get(),
        component.material.get(),
        component.shader.get()
    };

    InstanceBatch& batch = GetOrCreateBatch(key, component.model, component.material, component.shader);

    batch.AddInstance(worldMatrix, bloomColor, bloomIntensity);
    m_stats.instancedObjects++;

    return true;
}

bool InstancingManager::IsInstanceable(const ModelRenderComponent& component) const
{
    if (component.HasAnimation()) return false;
    if (!component.model || !component.shader) return false;
    if (!component.model->mBoneInfoMap.empty()) return false;
    //if (component.depthOffset) return false;

    // Transparent and fading objects need per-instance opacity — cannot be instanced
    if (component.distanceFadeOpacity < 1.0f) return false;
    if (component.material && component.material->GetOpacity() < 1.0f) return false;

    return true;
}

InstanceBatch& InstancingManager::GetOrCreateBatch(const BatchKey& key, std::shared_ptr<Model> model, std::shared_ptr<Material> material, std::shared_ptr<Shader> shader)
{
    auto it = m_batches.find(key);
    if (it != m_batches.end())
    {
        return it->second;
    }

    // 1. Create the empty batch directly inside the map memory (Zero copies/moves!)
    auto [insertIt, inserted] = m_batches.emplace(key, InstanceBatch());
    //if (inserted) {
    //    ENGINE_LOG_INFO("[Instancing] New batch created: model=" +
    //        (model ? model->modelPath : "null") +
    //        " material=" + material.get()->GetName() +
    //        " total batches=" + std::to_string(m_batches.size()));
    //}

    // 2. Grab a reference to the permanent batch
    InstanceBatch& newBatch = insertIt->second;

    // 3. Initialize and Prewarm the permanent batch
    newBatch.Initialize(model, material, shader);
    newBatch.Prewarm();

    if (model) {
        for (auto& mesh : model->meshes) {
            mesh.Prewarm();  // Upload vertex data to GPU now, not mid-frame
        }
    }

    return newBatch;
}

void InstancingManager::RenderBatches(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos)
{
    PROFILE_FUNCTION();

    if (!m_enabled)
    {
        return;
    }

    // Build sorted batch list (only non-empty batches)
    m_sortedBatches.clear();
    for (auto& [key, batch] : m_batches) {
        // Enforce the threshold! Small batches will be skipped here.
        if (!batch.IsEmpty())
        {
            m_sortedBatches.push_back(&batch);
        }
    }

    // Sort batches by shader -> material -> model to minimize state changes
    std::sort(m_sortedBatches.begin(), m_sortedBatches.end(),
        [](const InstanceBatch* a, const InstanceBatch* b) {
            // Sort by shader first
            if (a->GetShader() != b->GetShader()) 
            {
                return a->GetShader() < b->GetShader();
            }
            // Then by material
            if (a->GetMaterial() != b->GetMaterial()) 
            {
                return a->GetMaterial() < b->GetMaterial();
            }
            // Then by model
            return a->GetModel() < b->GetModel();
        });

    // Track current state to avoid redundant switches
    Shader* currentShader = nullptr;
    Material* currentMaterial = nullptr;

    int batchIndex = 0;
    for (InstanceBatch* batch : m_sortedBatches) 
      {
        // Check if we need to switch shader
        if (batch->GetShader() != currentShader) 
        {
            batch->GetShader()->Activate();
            batch->GetShader()->setMat4("view", view);
            batch->GetShader()->setMat4("projection", projection);
            batch->GetShader()->setVec3("cameraPos", cameraPos);
            batch->GetShader()->setBool("useInstancing", true);
            batch->GetShader()->setBool("hasBones", false);

            // Apply lighting on shader switch
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
            if (ecsManager.lightingSystem)
            {
                ecsManager.lightingSystem->ApplyLighting(*batch->GetShader());
                ecsManager.lightingSystem->ApplyShadows(*batch->GetShader());
            }

            // Environment reflections (skybox already bound to texture unit 12 by GraphicsManager)
            auto& gfx = GraphicsManager::GetInstance();
            batch->GetShader()->setBool("hasEnvMap", gfx.IsEnvReflectionActive());
            if (gfx.IsEnvReflectionActive()) {
                batch->GetShader()->setInt("envMap", 12);
                batch->GetShader()->setFloat("envReflectionIntensity", gfx.GetEnvReflectionIntensity());
            }

            currentShader = batch->GetShader();
            currentMaterial = nullptr;  // Force material rebind on shader change
        }

        // Check if we need to switch material
        if (batch->GetMaterial() != currentMaterial) 
        {
            if (batch->GetMaterial()) 
            {
                batch->GetMaterial()->ApplyToShader(*currentShader);
            }
            currentMaterial = batch->GetMaterial();
        }

        // Render the batch
        batch->Render(view, projection, cameraPos);
        ++batchIndex;
        m_stats.drawCalls += static_cast<int>(batch->GetModel()->meshes.size());
    }

    // Reset instancing flag
    if (currentShader) 
    {
        currentShader->setBool("useInstancing", false);
    }
}

void InstancingManager::RenderBatchesDepthOnly(const glm::mat4& lightSpaceMatrix)
{
    if (!m_enabled)
    {
        return;
    }

    for (auto& [key, batch] : m_batches)
    {
        if (batch.GetInstanceCount() >= static_cast<size_t>(m_minInstancesForBatching))
        {
            batch.RenderDepthOnly(lightSpaceMatrix);
        }
    }
}

void InstancingManager::RenderBatchesDepthPrepass(const glm::mat4& view, const glm::mat4& projection, Shader& depthShader)
{
    if (!m_enabled) return;

    depthShader.Activate();
    depthShader.setMat4("view", view);
    depthShader.setMat4("projection", projection);
    depthShader.setBool("useInstancing", true);
    depthShader.setBool("isAnimated", false);   // instanced batches are never animated
    depthShader.setBool("hasDiffuseMap", false); // alpha-cutout handled conservatively

    for (auto& [key, batch] : m_batches)
    {
        if (batch.IsEmpty()) continue;
        // RenderDepthOnly uses whatever shader is currently bound — that's our prepass shader
        batch.RenderDepthOnly(glm::mat4(1.0f));
    }

    depthShader.setBool("useInstancing", false);
}

void InstancingManager::PrewarmScene(ECSManager& ecsManager)
{
    // Loop through every model in the scene, regardless of where the camera is
    for (Entity entity : ecsManager.GetAllEntities())
    {
        if (ecsManager.HasComponent<ModelRenderComponent>(entity))
        {
            auto& component = ecsManager.GetComponent<ModelRenderComponent>(entity);

            if (IsInstanceable(component))
            {
                BatchKey key{
                    component.model.get(),
                    component.material.get(),
                    component.shader.get()
                };

                // This forces the batch to be created and Prewarmed() in memory
                // while the loading screen is still up!
                GetOrCreateBatch(key, component.model, component.material, component.shader);
            }
        }
    }

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    glm::mat4 dummyMatrix = glm::mat4(1.0f);
    glm::vec3 dummyPos = glm::vec3(0.0f);

    for (auto& [key, batch] : m_batches) {
        // This forces the Instanced Shader to compile and the instanced VBO to map!
        batch.Render(dummyMatrix, dummyMatrix, dummyPos);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
}

bool InstancingManager::WasRenderedInstanced(const ModelRenderComponent& component) const
{
    // If instancing is off or it's an animated/invalid model, it definitely wasn't instanced
    if (!m_enabled || !IsInstanceable(component)) return false;

    BatchKey key{
        component.model.get(),
        component.material.get(),
        component.shader.get()
    };

    auto it = m_batches.find(key);
    if (it != m_batches.end())
    {
        // It was ONLY rendered if its batch met the threshold this frame!
        return it->second.GetInstanceCount() >= static_cast<size_t>(m_minInstancesForBatching);
    }

    return false;
}