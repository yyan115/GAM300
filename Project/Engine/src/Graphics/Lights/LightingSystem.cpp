#include "pch.h"
#include "Graphics/Lights/LightingSystem.hpp"
#include "Graphics/Lights/LightComponent.hpp" 
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include <Graphics/GraphicsManager.hpp>

bool LightingSystem::Initialise()
{
	std::cout << "[LightingSystem] Initialized" << std::endl;
	return true;
}

void LightingSystem::Update()
{
	CollectLightData();
}

void LightingSystem::Shutdown()
{
	std::cout << "[LightingSystem] Shutdown" << std::endl;
}

void LightingSystem::ApplyLighting(Shader& shader)
{
    // Apply directional light from collected data
    if (directionalLightData.hasDirectionalLight) 
    {
        shader.setVec3("dirLight.direction", directionalLightData.direction);
        shader.setVec3("dirLight.ambient", directionalLightData.ambient);
        shader.setVec3("dirLight.diffuse", directionalLightData.diffuse);
        shader.setVec3("dirLight.specular", directionalLightData.specular);
    }
    else 
    {
        // Disable directional light if none exists
        shader.setVec3("dirLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader.setVec3("dirLight.ambient", glm::vec3(0.0f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("dirLight.specular", glm::vec3(0.0f));
    }

    // Apply point lights from collected data
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) 
    {
        std::string base = "pointLights[" + std::to_string(i) + "]";

        if (i < pointLightData.positions.size()) 
        {
            // Active point light from ECS data
            shader.setVec3(base + ".position", pointLightData.positions[i]);
            shader.setVec3(base + ".ambient", pointLightData.ambient[i]);
            shader.setVec3(base + ".diffuse", pointLightData.diffuse[i]);
            shader.setVec3(base + ".specular", pointLightData.specular[i]);
            shader.setFloat(base + ".constant", pointLightData.constant[i]);
            shader.setFloat(base + ".linear", pointLightData.linear[i]);
            shader.setFloat(base + ".quadratic", pointLightData.quadratic[i]);
        }
        else 
        {
            // Disabled point light
            shader.setVec3(base + ".position", glm::vec3(0.0f));
            shader.setVec3(base + ".ambient", glm::vec3(0.0f));
            shader.setVec3(base + ".diffuse", glm::vec3(0.0f));
            shader.setVec3(base + ".specular", glm::vec3(0.0f));
            shader.setFloat(base + ".constant", 1.0f);
            shader.setFloat(base + ".linear", 0.0f);
            shader.setFloat(base + ".quadratic", 0.0f);
        }
    }

    // Apply spot lights from collected data
    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) 
    {
        std::string base = "spotLights[" + std::to_string(i) + "]";

        if (i < spotLightData.positions.size()) {

            // Active spot light from ECS data
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
        else 
        {
            // Disabled spot light
            shader.setVec3(base + ".position", glm::vec3(0.0f));
            shader.setVec3(base + ".direction", glm::vec3(0.0f, 0.0f, -1.0f));
            shader.setVec3(base + ".ambient", glm::vec3(0.0f));
            shader.setVec3(base + ".diffuse", glm::vec3(0.0f));
            shader.setVec3(base + ".specular", glm::vec3(0.0f));
            shader.setFloat(base + ".constant", 1.0f);
            shader.setFloat(base + ".linear", 0.0f);
            shader.setFloat(base + ".quadratic", 0.0f);
            shader.setFloat(base + ".cutOff", 0.0f);
            shader.setFloat(base + ".outerCutOff", 0.0f);
        }
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

    // Clear spot light data
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

    // Go through all entities with light components
    for (const auto& entity : entities) 
    {

        // Collect directional lights (just take the first one)
        if (ecsManager.HasComponent<DirectionalLightComponent>(entity)) 
        {
            auto& light = ecsManager.GetComponent<DirectionalLightComponent>(entity);

            if (light.enabled && !directionalLightData.hasDirectionalLight) 
            {
                directionalLightData.hasDirectionalLight = true;
                directionalLightData.direction = light.direction;
                directionalLightData.ambient = light.ambient;
                directionalLightData.diffuse = light.diffuse;
                directionalLightData.specular = light.specular;
            }
        }

        // Collect point lights (up to max)
        if (ecsManager.HasComponent<PointLightComponent>(entity)) 
        {
            auto& light = ecsManager.GetComponent<PointLightComponent>(entity);

            if (light.enabled && pointLightData.positions.size() < MAX_POINT_LIGHTS) 
            {
                // Get position from transform
                glm::vec3 position(0.0f);
                if (ecsManager.HasComponent<Transform>(entity)) 
                {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);
                    position = glm::vec3(transform.position.x, transform.position.y, transform.position.z);
                }

                pointLightData.positions.push_back(position);
                pointLightData.ambient.push_back(light.ambient);
                pointLightData.diffuse.push_back(light.diffuse);
                pointLightData.specular.push_back(light.specular);
                pointLightData.constant.push_back(light.constant);
                pointLightData.linear.push_back(light.linear);
                pointLightData.quadratic.push_back(light.quadratic);
            }
        }

        // Collect spot lights (up to max)
        if (ecsManager.HasComponent<SpotLightComponent>(entity)) 
        {
            auto& light = ecsManager.GetComponent<SpotLightComponent>(entity);

            if (light.enabled && spotLightData.positions.size() < MAX_SPOT_LIGHTS) 
            {
                // Get position from transform
                glm::vec3 position(0.0f);
                if (ecsManager.HasComponent<Transform>(entity)) 
                {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);
                    position = glm::vec3(transform.position.x, transform.position.y, transform.position.z);;
                }

                spotLightData.positions.push_back(position);
                spotLightData.directions.push_back(light.direction);
                spotLightData.ambient.push_back(light.ambient);
                spotLightData.diffuse.push_back(light.diffuse);
                spotLightData.specular.push_back(light.specular);
                spotLightData.constant.push_back(light.constant);
                spotLightData.linear.push_back(light.linear);
                spotLightData.quadratic.push_back(light.quadratic);
                spotLightData.cutOff.push_back(light.cutOff);
                spotLightData.outerCutOff.push_back(light.outerCutOff);
            }
        }
    }
}
