#include "pch.h"
#include "Graphics/Instancing/InstancingManager.hpp"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Graphics/Material.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Frustum/Frustum.hpp"
#include "Graphics/Lights/LightingSystem.hpp"
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

    GraphicsManager& graphics = GraphicsManager::GetInstance();
    m_frameCamera = graphics.GetCurrentCamera();
    ECSManager& ecsManager =
        ECSRegistry::GetInstance().GetActiveECSManager();
    m_frameLightingSystem = ecsManager.lightingSystem.get();

    for (InstanceBatch* batch : m_batchList)
    {
        batch->Clear();
    }

    m_sortedBatches.clear();
}

void InstancingManager::EndFrame()
{
    m_stats.batchCount = 0;

    for (const InstanceBatch* batch : m_batchList)
    {
        if (!batch->IsEmpty())
        {
            m_stats.batchCount++;
        }
    }
}

void InstancingManager::Clear()
{
    m_sortedBatches.clear();
    for (InstanceBatch* batch : m_batchList) {
        if (Model* model = batch->GetModel()) {
            for (Mesh& mesh : model->meshes) {
                mesh.InvalidateInstanceAttributes();
            }
        }
    }
    m_batchList.clear();
    m_batches.clear();
    m_frameCamera = nullptr;
    m_frameLightingSystem = nullptr;
    m_frustum = nullptr;
    m_stats.Reset();
}

InstanceSubmissionResult InstancingManager::TryAddInstance(
    const ModelRenderComponent& component,
    const glm::mat4& worldMatrix,
    const AABB& worldBounds,
    const glm::vec3& bloomColor,
    float bloomIntensity,
    std::uint32_t lightMask)
{
    m_stats.totalObjects++;

    if (!m_enabled)
    {
        return InstanceSubmissionResult::NotInstanced;
    }

    if (!IsInstanceable(component))
    {
        m_stats.nonInstancedObjects++;
        return InstanceSubmissionResult::NotInstanced;
    }

    if (m_frustum)
    {
        if (!m_frustum->IsBoxVisible(worldBounds))
        {
            m_stats.culledObjects++;
            return InstanceSubmissionResult::Culled;
        }
    }

    BatchKey key{
        component.model.get(),
        component.material.get(),
        component.shader.get()
    };

    InstanceBatch& batch = GetOrCreateBatch(key, component.model, component.material, component.shader);

    float cameraDistanceSq = 0.0f;
    if (const Camera* camera = m_frameCamera) {
        const glm::vec3 closest = glm::clamp(
            camera->Position, worldBounds.min, worldBounds.max);
        const glm::vec3 cameraDelta = camera->Position - closest;
        cameraDistanceSq = glm::dot(cameraDelta, cameraDelta);
    }

    batch.AddInstance(
        worldMatrix, bloomColor, bloomIntensity, lightMask,
        cameraDistanceSq);
    m_stats.instancedObjects++;

    return InstanceSubmissionResult::Added;
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
    if (!component.material) {
        for (const auto& mesh : component.model->meshes) {
            if (mesh.material && mesh.material->GetOpacity() < 1.0f) {
                return false;
            }
        }
    }

    return true;
}

InstanceBatch& InstancingManager::GetOrCreateBatch(
    const BatchKey& key,
    const std::shared_ptr<Model>& model,
    const std::shared_ptr<Material>& material,
    const std::shared_ptr<Shader>& shader)
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
    m_batchList.push_back(&newBatch);

    // 3. Initialize and Prewarm the permanent batch
    newBatch.Initialize(model, material, shader);
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
    for (InstanceBatch* batch : m_batchList) {
        // Collect only batches populated by this frame's culling pass.
        if (!batch->IsEmpty())
        {
#ifdef ANDROID
            // Depth sorting only needs the closest instance in each batch.
            // Convert its squared distance once per batch, not once per object.
            batch->FinalizeDepthBucket();
#endif
            m_sortedBatches.push_back(batch);
        }
    }

    GraphicsManager& graphics = GraphicsManager::GetInstance();
#ifdef ANDROID
    const bool groupBloomOutput = graphics.IsBloomTargetPrepared();
#endif

    // Android has no depth prepass, so coarse front-to-back ordering lets
    // early depth rejection avoid expensive PBR work while preserving state
    // grouping for batches at similar depths. Desktop keeps pure state order.
    std::sort(m_sortedBatches.begin(), m_sortedBatches.end(),
#ifdef ANDROID
        [groupBloomOutput](const InstanceBatch* a, const InstanceBatch* b) {
#else
        [](const InstanceBatch* a, const InstanceBatch* b) {
#endif
#ifdef ANDROID
            if (a->GetDepthBucket() != b->GetDepthBucket()) {
                return a->GetDepthBucket() < b->GetDepthBucket();
            }
#endif
            // Sort by shader first
            if (a->GetShader() != b->GetShader()) 
            {
                return a->GetShader() < b->GetShader();
            }
#ifdef ANDROID
            // glDrawBuffers changes can be expensive on tile-based GPUs.
            // Group bloom writes without disturbing coarse depth ordering.
            if (groupBloomOutput &&
                a->HasBloomEmission() != b->HasBloomEmission()) {
                return !a->HasBloomEmission();
            }
#endif
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
    const bool hasEnvironmentMap = graphics.IsEnvReflectionActive();
    const float environmentIntensity =
        graphics.GetEnvReflectionIntensity();

    for (InstanceBatch* batch : m_sortedBatches) 
      {
        // Check if we need to switch shader
        if (batch->GetShader() != currentShader) 
        {
            batch->GetShader()->Activate();
            if (!batch->GetShader()->UsesCameraBlock()) {
                batch->GetShader()->setMat4("view", view);
                batch->GetShader()->setMat4("projection", projection);
                batch->GetShader()->setVec3("cameraPos", cameraPos);
            }
            batch->GetShader()->setBool("useInstancing", true);
            batch->GetShader()->setBool("hasBones", false);
            batch->GetShader()->setFloat("brightnessBoost", 1.0f);

            // Apply lighting on shader switch
            if (m_frameLightingSystem)
            {
                m_frameLightingSystem->ApplyLighting(*batch->GetShader());
                m_frameLightingSystem->ApplyShadows(*batch->GetShader());
            }

            // Environment reflections (skybox already bound to texture unit 12 by GraphicsManager)
            batch->GetShader()->setBool("hasEnvMap", hasEnvironmentMap);
            if (hasEnvironmentMap) {
                batch->GetShader()->setInt("envMap", 12);
                batch->GetShader()->setFloat(
                    "envReflectionIntensity", environmentIntensity);
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

        // Most batches do not emit bloom. Keep the second color attachment
        // disabled for those draws to save mobile tile/color bandwidth.
        graphics.SetBloomOutputEnabled(batch->HasBloomEmission());

        // Render the batch
        batch->Render();
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

    for (InstanceBatch* batch : m_batchList)
    {
        if (!batch->IsEmpty())
        {
            batch->RenderDepthOnly(lightSpaceMatrix);
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

    for (InstanceBatch* batch : m_batchList)
    {
        if (batch->IsEmpty()) continue;
        // RenderDepthOnly uses whatever shader is currently bound — that's our prepass shader
        batch->RenderDepthOnly(glm::mat4(1.0f));
    }

    depthShader.setBool("useInstancing", false);
}

void InstancingManager::PrewarmScene(ECSManager& ecsManager)
{
    std::unordered_map<BatchKey, size_t, BatchKeyHash> batchCounts;
    batchCounts.reserve(ecsManager.modelSystem
        ? ecsManager.modelSystem->entities.size()
        : 0);

    // Loop through every model in the scene, regardless of where the camera is
    for (Entity entity : ecsManager.GetAllEntitiesView())
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

                // Create each unique batch while the loading screen is up.
                GetOrCreateBatch(key, component.model, component.material, component.shader);
                ++batchCounts[key];
            }
        }
    }

    // Most scenes have many singleton/small material combinations and only a
    // few large repeated batches. Size each CPU/GPU instance buffer for its
    // authored count instead of allocating 512 records for every key.
    for (const auto& [key, instanceCount] : batchCounts) {
        auto batch = m_batches.find(key);
        if (batch != m_batches.end()) {
            batch->second.Prewarm(instanceCount);
        }
    }

    // ModelSystem separately prewarms shared shader and mesh pipelines with
    // its hidden draw.
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
        return !it->second.IsEmpty();
    }

    return false;
}
