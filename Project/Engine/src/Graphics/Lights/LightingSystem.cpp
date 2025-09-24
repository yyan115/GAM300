#include "pch.h"
#include "Graphics/Lights/LightingSystem.hpp"
#include "Graphics/Lights/LightComponent.hpp" 
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"

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
    // Hardcoded directional light
    shader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
    shader.setVec3("dirLight.ambient", glm::vec3(0.05f));
    shader.setVec3("dirLight.diffuse", glm::vec3(0.4f));
    shader.setVec3("dirLight.specular", glm::vec3(0.5f));

    // Hardcoded point lights (4 lights matching your original positions)
    std::vector<glm::vec3> pointPositions = {
        glm::vec3(0.7f,  0.2f,  2.0f),
        glm::vec3(2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f,  2.0f, -12.0f),
        glm::vec3(0.0f,  0.0f, -3.0f)
    };

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        std::string base = "pointLights[" + std::to_string(i) + "]";

        if (i < pointPositions.size()) {
            // Active point light
            shader.setVec3(base + ".position", pointPositions[i]);
            shader.setVec3(base + ".ambient", glm::vec3(0.05f));
            shader.setVec3(base + ".diffuse", glm::vec3(0.8f));
            shader.setVec3(base + ".specular", glm::vec3(1.0f));
            shader.setFloat(base + ".constant", 1.0f);
            shader.setFloat(base + ".linear", 0.09f);
            shader.setFloat(base + ".quadratic", 0.032f);
        }
        else {
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

    // Hardcoded spot lights (array format to match updated shader)
    //for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
    //    std::string base = "spotLights[" + std::to_string(i) + "]";

    //    if (i == 0) {
    //        // Active spot light - using camera position/direction
    //        shader.setVec3(base + ".position", glm::vec3(0.0f, 0.0f, 3.0f)); // hardcoded camera pos
    //        shader.setVec3(base + ".direction", glm::vec3(0.0f, 0.0f, -1.0f)); // looking forward
    //        shader.setVec3(base + ".ambient", glm::vec3(0.0f));
    //        shader.setVec3(base + ".diffuse", glm::vec3(1.0f));
    //        shader.setVec3(base + ".specular", glm::vec3(1.0f));
    //        shader.setFloat(base + ".constant", 1.0f);
    //        shader.setFloat(base + ".linear", 0.09f);
    //        shader.setFloat(base + ".quadratic", 0.032f);
    //        shader.setFloat(base + ".cutOff", 0.976f); // cos(12.5 degrees)
    //        shader.setFloat(base + ".outerCutOff", 0.966f); // cos(15 degrees)
    //    }
    //    else {
    //        // Disabled spot light
    //        shader.setVec3(base + ".position", glm::vec3(0.0f));
    //        shader.setVec3(base + ".direction", glm::vec3(0.0f, 0.0f, -1.0f));
    //        shader.setVec3(base + ".ambient", glm::vec3(0.0f));
    //        shader.setVec3(base + ".diffuse", glm::vec3(0.0f));
    //        shader.setVec3(base + ".specular", glm::vec3(0.0f));
    //        shader.setFloat(base + ".constant", 1.0f);
    //        shader.setFloat(base + ".linear", 0.0f);
    //        shader.setFloat(base + ".quadratic", 0.0f);
    //        shader.setFloat(base + ".cutOff", 0.0f);
    //        shader.setFloat(base + ".outerCutOff", 0.0f);
    //    }
    //}
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
