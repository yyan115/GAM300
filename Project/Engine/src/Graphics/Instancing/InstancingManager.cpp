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

    return true;
}

InstanceBatch& InstancingManager::GetOrCreateBatch(const BatchKey& key, std::shared_ptr<Model> model, std::shared_ptr<Material> material, std::shared_ptr<Shader> shader)
{
    auto it = m_batches.find(key);
    if (it != m_batches.end())
    {
        return it->second;
    }

    InstanceBatch batch;
    batch.Initialize(model, material, shader);

    auto [insertIt, inserted] = m_batches.emplace(key, std::move(batch));
    return insertIt->second;
}


void InstancingManager::RenderBatches(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) 
{
    if (!m_enabled) 
    {
        return;
    }

    // Build sorted batch list (only non-empty batches)
    m_sortedBatches.clear();
    for (auto& [key, batch] : m_batches) {
        // Render ALL non-empty batches, not just those >= minInstances
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