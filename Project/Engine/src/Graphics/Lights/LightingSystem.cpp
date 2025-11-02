#include "pch.h"
#include "Graphics/Lights/LightingSystem.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include <Graphics/GraphicsManager.hpp>
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ActiveComponent.hpp"

bool LightingSystem::Initialise()
{
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
	std::cout << "[LightingSystem] Shutdown" << std::endl;
}

void LightingSystem::ApplyLighting(Shader& shader)
{
    // Apply directional light
    if (directionalLightData.hasDirectionalLight)
    {
        shader.setVec3("dirLight.direction", directionalLightData.direction);
        shader.setVec3("dirLight.ambient", directionalLightData.ambient);
        shader.setVec3("dirLight.diffuse", directionalLightData.diffuse);
        shader.setVec3("dirLight.specular", directionalLightData.specular);
    }
    else
    {
        shader.setVec3("dirLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader.setVec3("dirLight.ambient", glm::vec3(0.0f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("dirLight.specular", glm::vec3(0.0f));
    }

    // Send counts to shader so it only processes active lights
    shader.setInt("numPointLights", static_cast<int>(pointLightData.positions.size()));
    shader.setInt("numSpotLights", static_cast<int>(spotLightData.positions.size()));

    // Only loop through and set active point lights
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
    }

    // Only loop through and set active spot lights
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

    for (const auto& entity : entities)
    {
        // Skip inactive entities (Unity-like behavior)
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue; // Don't render inactive entities
            }
        }

        // Collect directional light (first one only)
        if (ecsManager.HasComponent<DirectionalLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<DirectionalLightComponent>(entity);
            if (light.enabled && !directionalLightData.hasDirectionalLight)
            {
                directionalLightData.hasDirectionalLight = true;

                // FIX 1: Transform direction to world space if entity has rotation
                glm::vec3 direction = light.direction.ConvertToGLM();
                if (ecsManager.HasComponent<Transform>(entity))
                {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);
                    glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                    // Extract rotation part and apply to direction
                    glm::mat3 rotationMatrix = glm::mat3(worldMat);
                    direction = glm::normalize(rotationMatrix * direction);
                }

                directionalLightData.direction = direction;
                directionalLightData.ambient = light.ambient.ConvertToGLM();
                directionalLightData.diffuse = light.diffuse.ConvertToGLM();
                directionalLightData.specular = light.specular.ConvertToGLM();
            }
        }

        // Collect point lights with limit warning
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
                        // FIX 2: Use world position from world matrix
                        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                        position = glm::vec3(worldMat[3]); // Extract translation from 4th column
                    }

                    pointLightData.positions.push_back(position);
                    pointLightData.ambient.push_back(light.ambient.ConvertToGLM());
                    pointLightData.diffuse.push_back(light.diffuse.ConvertToGLM());
                    pointLightData.specular.push_back(light.specular.ConvertToGLM());
                    pointLightData.constant.push_back(light.constant);
                    pointLightData.linear.push_back(light.linear);
                    pointLightData.quadratic.push_back(light.quadratic);
                }
                else
                {
                    // Only warn once when limit is hit
                    static bool pointLightWarningShown = false;
                    if (!pointLightWarningShown) {
                        std::cout << "[LightingSystem] Warning: Maximum point lights (" << MAX_POINT_LIGHTS
                            << ") reached. Additional point lights will be ignored." << std::endl;
                        pointLightWarningShown = true;
                    }
                }
            }
        }

        // Collect spot lights with limit warning
        if (ecsManager.HasComponent<SpotLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<SpotLightComponent>(entity);

            if (light.enabled)
            {
                if (spotLightData.positions.size() < MAX_SPOT_LIGHTS)
                {
                    glm::vec3 position(0.0f);
                    glm::vec3 direction = light.direction.ConvertToGLM();

                    if (ecsManager.HasComponent<Transform>(entity))
                    {
                        auto& transform = ecsManager.GetComponent<Transform>(entity);
                        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();

                        // FIX 3: Use world position from world matrix
                        position = glm::vec3(worldMat[3]); // Extract translation from 4th column

                        // FIX 4: Transform direction to world space
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
                }
                else
                {
                    // Only warn once when limit is hit
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
