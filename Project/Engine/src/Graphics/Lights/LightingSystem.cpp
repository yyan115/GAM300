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
    std::cout << "[LightingSystem] Initializing..." << std::endl;

    // Initialize directional shadow map
    if (!directionalShadowMap.Initialize(shadowMapResolution))
    {
        std::cout << "[LightingSystem] Warning: Directional shadow map failed" << std::endl;
        shadowsEnabled = false;
    }

    // Initialize point light shadow maps
    pointShadowMaps.resize(MAX_POINT_LIGHT_SHADOWS);
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (!pointShadowMaps[i].Initialize(pointShadowMapResolution))
        {
            std::cout << "[LightingSystem] Warning: Point shadow map " << i << " failed" << std::endl;
        }
    }

    std::cout << "[LightingSystem] Initialized" << std::endl;
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

    std::cout << "[LightingSystem] Shutdown" << std::endl;
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

    // Render point light shadows
    int shadowIndex = 0;
    for (size_t i = 0; i < pointLightData.positions.size() && shadowIndex < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (pointLightData.shadowIndex[i] >= 0)
        {
            pointShadowMaps[shadowIndex].Render(
                pointLightData.positions[i],
                pointLightShadowFarPlane,
                shadowRenderCallback
            );
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
    shader.setFloat("ambientIntensity", ambientIntensity);

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

    int pointShadowCount = 0;
    pointLightData.shadowIndex.clear();

    // Clear previous frame data
    pointLightData.positions.clear();
    pointLightData.ambient.clear();
    pointLightData.diffuse.clear();
    pointLightData.specular.clear();
    pointLightData.constant.clear();
    pointLightData.linear.clear();
    pointLightData.quadratic.clear();
    pointLightData.intensity.clear();

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

    for (const auto& entity : entities)
    {
        // Skip inactive entities
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue;
            }
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
                // directionalLightData.castShadows = light.castShadows;  // Add this to component if needed
            }
        }

        // Collect point lights
        if (ecsManager.HasComponent<PointLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<PointLightComponent>(entity);

            if (light.enabled)
            {
                if (pointLightData.positions.size() < MAX_POINT_LIGHTS)
                {
                    glm::vec3 position(0.0f);
                    if (ecsManager.HasComponent<Transform>(entity))
                    {
                        auto& transform = ecsManager.GetComponent<Transform>(entity);
                        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                        position = glm::vec3(worldMat[3]);
                    }

                    pointLightData.positions.push_back(position);
                    pointLightData.ambient.push_back(light.ambient.ConvertToGLM());
                    pointLightData.diffuse.push_back(light.diffuse.ConvertToGLM());
                    pointLightData.specular.push_back(light.specular.ConvertToGLM());
                    pointLightData.constant.push_back(light.constant);
                    pointLightData.linear.push_back(light.linear);
                    pointLightData.quadratic.push_back(light.quadratic);
                    pointLightData.intensity.push_back(light.intensity);

                    // Assign shadow index if we have available shadow maps
                    if (pointShadowCount < MAX_POINT_LIGHT_SHADOWS)
                    {
                        pointLightData.shadowIndex.push_back(pointShadowCount);
                        pointShadowCount++;
                    }
                    else
                    {
                        pointLightData.shadowIndex.push_back(-1);  // No shadow for this light
                    }
                }
                else
                {
                    static bool pointLightWarningShown = false;
                    if (!pointLightWarningShown) {
                        std::cout << "[LightingSystem] Warning: Maximum point lights (" << MAX_POINT_LIGHTS
                            << ") reached. Additional point lights will be ignored." << std::endl;
                        pointLightWarningShown = true;
                    }
                }
            }
        }

        // Collect spot lights
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
                        std::cout << "[LightingSystem] Warning: Maximum spot lights (" << MAX_SPOT_LIGHTS
                            << ") reached. Additional spot lights will be ignored." << std::endl;
                        spotLightWarningShown = true;
                    }
                }
            }
        }
    }
}