#include "pch.h"
#include "Graphics/Lights/LightingSystem.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Asset Manager/ResourceManager.hpp"

bool LightingSystem::Initialise()
{
    ENGINE_PRINT("[LightingSystem] Initializing...");

    // Initialize directional shadow map
    if (!directionalShadowMap.Initialize(shadowMapResolution))
    {
        ENGINE_PRINT("[LightingSystem] Warning: Directional shadow map failed");
        shadowsEnabled = false;
    }

    // Initialize point light shadow maps
    pointShadowMaps.resize(MAX_POINT_LIGHT_SHADOWS);
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (!pointShadowMaps[i].Initialize(pointShadowMapResolution))
        {
            ENGINE_PRINT("[LightingSystem] Warning: Point shadow map {} failed", i);
        }
    }

    ENGINE_PRINT("[LightingSystem] Initialized");
    return true;
}


void LightingSystem::Update()
{
    PROFILE_FUNCTION();
    CollectLightData();
}

void LightingSystem::Shutdown()
{
    directionalShadowMap.Shutdown();

    for (auto& psm : pointShadowMaps)
    {
        psm.Shutdown();
    }
    pointShadowMaps.clear();

    ENGINE_PRINT("[LightingSystem] Shutdown");
}

void LightingSystem::RenderShadowMaps()
{
    if (!shadowsEnabled || !shadowRenderCallback)
    {
        return;
    }

    // Render directional shadow
    if (directionalLightData.hasDirectionalLight)
    {
        Camera* camera = GraphicsManager::GetInstance().GetCurrentCamera();
        glm::vec3 sceneCenter = camera ? camera->Position : glm::vec3(0.0f);

        directionalShadowMap.Render(
            directionalLightData.direction,
            sceneCenter,
            shadowDistance,
            shadowRenderCallback
        );
    }

    // =========================================================================
    // POINT LIGHT SHADOWS WITH CACHING
    // =========================================================================

    // Increment frame counters for ALL shadow maps
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        pointShadowMaps[i].IncrementFrameCounter();
    }

    // LOGGING: Track updates this frame
    int updatedCount = 0;
    int skippedCount = 0;

    // Render only shadows that need updating
    int shadowIndex = 0;
    for (size_t i = 0; i < pointLightData.positions.size() && shadowIndex < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (pointLightData.shadowIndex[i] >= 0)
        {
            glm::vec3 lightPos = pointLightData.positions[i];

            if (pointShadowMaps[shadowIndex].NeedsUpdate(lightPos, pointLightShadowFarPlane))
            {
                pointShadowMaps[shadowIndex].Render(lightPos, pointLightShadowFarPlane, shadowRenderCallback);
                pointShadowMaps[shadowIndex].MarkUpdated(lightPos, pointLightShadowFarPlane);
                updatedCount++;
            }
            else
            {
                skippedCount++;
            }

            shadowIndex++;
        }
    }

}

void LightingSystem::ApplyLighting(Shader& shader)
{
    shader.setInt("ambientMode", static_cast<int>(ambientMode));
    shader.setVec3("ambientSky", ambientSky);
    shader.setVec3("ambientEquator", ambientEquator);
    shader.setVec3("ambientGround", ambientGround);

    // Apply directional light
    if (directionalLightData.hasDirectionalLight)
    {
        shader.setVec3("dirLight.direction", directionalLightData.direction);
        shader.setVec3("dirLight.ambient", directionalLightData.ambient);
        shader.setVec3("dirLight.diffuse", directionalLightData.diffuse);
        shader.setVec3("dirLight.specular", directionalLightData.specular);
        shader.setFloat("dirLight.intensity", directionalLightData.intensity);
    }
    else
    {
        shader.setVec3("dirLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader.setVec3("dirLight.ambient", glm::vec3(0.0f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("dirLight.specular", glm::vec3(0.0f));
        shader.setFloat("dirLight.intensity", 0.0f);
    }

    // Send counts to shader
    shader.setInt("numPointLights", static_cast<int>(pointLightData.positions.size()));
    shader.setInt("numSpotLights", static_cast<int>(spotLightData.positions.size()));

    // Set active point lights
    for (size_t i = 0; i < pointLightData.positions.size(); i++)
    {
        std::string base = "pointLights[" + std::to_string(i) + "]";
        shader.setVec3(base + ".position", pointLightData.positions[i]);
        shader.setVec3(base + ".ambient", pointLightData.ambient[i]);
        shader.setVec3(base + ".diffuse", pointLightData.diffuse[i]);
        shader.setVec3(base + ".specular", pointLightData.specular[i]);
        shader.setFloat(base + ".constant", pointLightData.constant[i]);
        shader.setFloat(base + ".linear", pointLightData.linear[i]);
        shader.setFloat(base + ".quadratic", pointLightData.quadratic[i]);
        shader.setFloat(base + ".intensity", pointLightData.intensity[i]);
    }

    // Set active spot lights
    for (size_t i = 0; i < spotLightData.positions.size(); i++)
    {
        std::string base = "spotLights[" + std::to_string(i) + "]";
        shader.setVec3(base + ".position", spotLightData.positions[i]);
        shader.setVec3(base + ".direction", spotLightData.directions[i]);
        shader.setVec3(base + ".ambient", spotLightData.ambient[i]);
        shader.setVec3(base + ".diffuse", spotLightData.diffuse[i]);
        shader.setVec3(base + ".specular", spotLightData.specular[i]);
        shader.setFloat(base + ".constant", spotLightData.constant[i]);
        shader.setFloat(base + ".linear", spotLightData.linear[i]);
        shader.setFloat(base + ".quadratic", spotLightData.quadratic[i]);
        shader.setFloat(base + ".cutOff", spotLightData.cutOff[i]);
        shader.setFloat(base + ".outerCutOff", spotLightData.outerCutOff[i]);
        shader.setFloat(base + ".intensity", spotLightData.intensity[i]);
    }
}

void LightingSystem::ApplyShadows(Shader& shader)
{
    // CRITICAL: Always set samplerCube uniforms to their dedicated texture units (9-12)
    // to prevent conflict with sampler2D textures at units 0-7.
    // If these aren't set, they default to 0 which causes "samplers of different type
    // assigned to same texture unit" error on OpenGL ES.
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        shader.setInt("pointShadowMaps[" + std::to_string(i) + "]", 9 + i);
    }

    if (!shadowsEnabled)
    {
        shader.setBool("shadowsEnabled", false);
        return;
    }

    shader.setBool("shadowsEnabled", true);

    // Directional shadow
    if (directionalLightData.hasDirectionalLight)
    {
        directionalShadowMap.Apply(shader, 8);
    }

    // Point light shadows
    shader.setFloat("pointShadowFarPlane", pointLightShadowFarPlane);

    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (i < pointShadowMaps.size() && pointShadowMaps[i].IsInitialized())
        {
            pointShadowMaps[i].Apply(shader, 9 + i, i);  // Texture units 9, 10, 11, 12
        }
    }

    // Send shadow indices for each point light
    for (size_t i = 0; i < pointLightData.shadowIndex.size(); ++i)
    {
        shader.setInt("pointLights[" + std::to_string(i) + "].shadowIndex", pointLightData.shadowIndex[i]);
    }
}

void LightingSystem::CollectLightData()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    // Clear previous frame data
    pointLightData.positions.clear();
    pointLightData.ambient.clear();
    pointLightData.diffuse.clear();
    pointLightData.specular.clear();
    pointLightData.constant.clear();
    pointLightData.linear.clear();
    pointLightData.quadratic.clear();
    pointLightData.intensity.clear();
    pointLightData.shadowIndex.clear();

    directionalLightData.hasDirectionalLight = false;

    spotLightData.positions.clear();
    spotLightData.directions.clear();
    spotLightData.ambient.clear();
    spotLightData.diffuse.clear();
    spotLightData.specular.clear();
    spotLightData.constant.clear();
    spotLightData.linear.clear();
    spotLightData.quadratic.clear();
    spotLightData.cutOff.clear();
    spotLightData.outerCutOff.clear();
    spotLightData.intensity.clear();

    // =========================================================================
    // GET CAMERA POSITION FOR DISTANCE CULLING
    // =========================================================================
    Camera* camera = GraphicsManager::GetInstance().GetCurrentCamera();
    glm::vec3 camPos = camera ? camera->Position : glm::vec3(0.0f);

    // =========================================================================
    // TEMPORARY STORAGE FOR ALL POINT LIGHTS (before culling)
    // =========================================================================
    struct PointLightCandidate {
        glm::vec3 position;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float constant;
        float linear;
        float quadratic;
        float intensity;
        bool castShadows;
        float distanceToCamera;
    };
    std::vector<PointLightCandidate> allPointLights;

    for (const auto& entity : entities)
    {
        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        // Collect directional light (first one only)
        if (ecsManager.HasComponent<DirectionalLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<DirectionalLightComponent>(entity);
            if (light.enabled && !directionalLightData.hasDirectionalLight)
            {
                directionalLightData.hasDirectionalLight = true;

                glm::vec3 direction(0.2f, -1.0f, -0.3f);
                if (ecsManager.HasComponent<Transform>(entity))
                {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);
                    glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                    glm::mat3 rotationMatrix = glm::mat3(worldMat);
                    direction = glm::normalize(rotationMatrix * direction);
                }

                directionalLightData.direction = direction;
                directionalLightData.ambient = light.ambient.ConvertToGLM();
                directionalLightData.diffuse = light.diffuse.ConvertToGLM();
                directionalLightData.specular = light.specular.ConvertToGLM();
                directionalLightData.intensity = light.intensity;
            }
        }

        // =====================================================================
        // COLLECT ALL POINT LIGHTS INTO TEMPORARY VECTOR
        // =====================================================================
        if (ecsManager.HasComponent<PointLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<PointLightComponent>(entity);

            if (light.enabled)
            {
                glm::vec3 position(0.0f);
                if (ecsManager.HasComponent<Transform>(entity))
                {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);
                    glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                    position = glm::vec3(worldMat[3]);
                }

                float dist = glm::distance(position, camPos);

                allPointLights.push_back({
                    position,
                    light.ambient.ConvertToGLM(),
                    light.diffuse.ConvertToGLM(),
                    light.specular.ConvertToGLM(),
                    light.constant,
                    light.linear,
                    light.quadratic,
                    light.intensity,
                    light.castShadows,
                    dist
                    });
            }
        }

        // Collect spot lights (unchanged)
        if (ecsManager.HasComponent<SpotLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<SpotLightComponent>(entity);

            if (light.enabled)
            {
                if (spotLightData.positions.size() < MAX_SPOT_LIGHTS)
                {
                    glm::vec3 position(0.0f);
                    glm::vec3 direction(0.0f, 0.0f, -1.0f);

                    if (ecsManager.HasComponent<Transform>(entity))
                    {
                        auto& transform = ecsManager.GetComponent<Transform>(entity);
                        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                        position = glm::vec3(worldMat[3]);
                        glm::mat3 rotationMatrix = glm::mat3(worldMat);
                        direction = glm::normalize(rotationMatrix * direction);
                    }

                    spotLightData.positions.push_back(position);
                    spotLightData.directions.push_back(direction);
                    spotLightData.ambient.push_back(light.ambient.ConvertToGLM());
                    spotLightData.diffuse.push_back(light.diffuse.ConvertToGLM());
                    spotLightData.specular.push_back(light.specular.ConvertToGLM());
                    spotLightData.constant.push_back(light.constant);
                    spotLightData.linear.push_back(light.linear);
                    spotLightData.quadratic.push_back(light.quadratic);
                    spotLightData.cutOff.push_back(light.cutOff);
                    spotLightData.outerCutOff.push_back(light.outerCutOff);
                    spotLightData.intensity.push_back(light.intensity);
                }
                else
                {
                    static bool spotLightWarningShown = false;
                    if (!spotLightWarningShown) {
                        ENGINE_PRINT("[LightingSystem] Warning: Maximum spot lights ({}) reached. Additional spot lights will be ignored.", MAX_SPOT_LIGHTS);
                        spotLightWarningShown = true;
                    }
                }
            }
        }
    }

    // =========================================================================
    // POINT LIGHT DISTANCE CULLING
    // Sort all point lights by distance, keep only closest MAX_VISIBLE_POINT_LIGHTS
    // =========================================================================

    // Sort by distance (closest first)
    std::sort(allPointLights.begin(), allPointLights.end(),
        [](const PointLightCandidate& a, const PointLightCandidate& b) {
            return a.distanceToCamera < b.distanceToCamera;
        });

    // Determine how many lights to keep
    size_t numLightsToKeep = std::min(allPointLights.size(), static_cast<size_t>(MAX_VISIBLE_POINT_LIGHTS));

    // Also respect the shader's maximum
    numLightsToKeep = std::min(numLightsToKeep, static_cast<size_t>(MAX_POINT_LIGHTS));

    // =========================================================================
    // BUILD FINAL POINT LIGHT ARRAYS (only closest N lights)
    // =========================================================================

    struct ShadowCandidate {
        size_t lightIndex;
        float distanceToCamera;
    };
    std::vector<ShadowCandidate> shadowCandidates;

    for (size_t i = 0; i < numLightsToKeep; ++i)
    {
        const auto& light = allPointLights[i];

        pointLightData.positions.push_back(light.position);
        pointLightData.ambient.push_back(light.ambient);
        pointLightData.diffuse.push_back(light.diffuse);
        pointLightData.specular.push_back(light.specular);
        pointLightData.constant.push_back(light.constant);
        pointLightData.linear.push_back(light.linear);
        pointLightData.quadratic.push_back(light.quadratic);
        pointLightData.intensity.push_back(light.intensity);
        pointLightData.shadowIndex.push_back(-1);  // Will be assigned below

        // Track shadow candidates
        if (light.castShadows)
        {
            shadowCandidates.push_back({
                pointLightData.positions.size() - 1,
                light.distanceToCamera
                });
        }
    }

    // =========================================================================
    // SHADOW DISTANCE CULLING
    // From visible lights, assign shadows to closest MAX_POINT_LIGHT_SHADOWS
    // =========================================================================

    // Already sorted by distance (inherited from point light sort)
    std::sort(shadowCandidates.begin(), shadowCandidates.end(),
        [](const ShadowCandidate& a, const ShadowCandidate& b) {
            return a.distanceToCamera < b.distanceToCamera;
        });

    int pointShadowCount = 0;
    for (size_t i = 0; i < shadowCandidates.size() && pointShadowCount < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        size_t lightIndex = shadowCandidates[i].lightIndex;
        pointLightData.shadowIndex[lightIndex] = pointShadowCount;
        pointShadowCount++;
    }

    // Update active shadow caster count for editor
    activeShadowCasterCount = pointShadowCount;

    // =========================================================================
    // DEBUG LOGGING (comment out in production)
    // =========================================================================
    /*static int debugFrameCounter = 0;
    debugFrameCounter++;
    if (debugFrameCounter % 60 == 0)
    {
        if (allPointLights.size() > numLightsToKeep)
        {
            std::cout << "[Light Culling] " << allPointLights.size() << " point lights in scene, "
                      << numLightsToKeep << " visible (culled "
                      << (allPointLights.size() - numLightsToKeep) << ")" << std::endl;
        }
    
        if (!shadowCandidates.empty())
        {
            std::cout << "[Shadow Culling] " << shadowCandidates.size() << " lights want shadows, "
                      << pointShadowCount << " assigned" << std::endl;
        }
    }*/
}