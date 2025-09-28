#include "pch.h"
#include "Graphics/LightManager.hpp"
#include <glm/gtc/constants.hpp>

LightManager& LightManager::getInstance() 
{
    static LightManager instance;
    return instance;
}

void LightManager::setDirectionalLight(const glm::vec3& direction, const glm::vec3& ambient, const glm::vec3 diffuse, const glm::vec3& specular)
{
    directionalLight.direction = direction;
    directionalLight.ambient = ambient;
    directionalLight.diffuse = diffuse;
    directionalLight.specular = specular;
}

void LightManager::setDirectionalLight(const glm::vec3 direction, const glm::vec3& color)
{
    setDirectionalLight(direction, color * 0.2f, color, color);
}

void LightManager::addPointLight(const glm::vec3& position, const glm::vec3& color, float intensity)
{
    PointLight light;
    light.position = position;
    light.ambient = color * 0.05f;
    light.diffuse = color * intensity;
    light.specular = color;
    light.constant = 1.0f;
    light.linear = 0.09f;
    light.quadratic = 0.032f;

    pointLights.push_back(light);
}

void LightManager::addPointLight(const PointLight& light)
{
    pointLights.push_back(light);
}

void LightManager::removePointLight(size_t index)
{
    if (index < pointLights.size()) 
    {
        pointLights.erase(pointLights.begin() + index);
    }
    else 
    {
        std::cerr << "[LightManager] ERROR: Invalid point light index " << index << std::endl;
    }
}

void LightManager::clearPointLights()
{
    pointLights.clear();
}

void LightManager::setSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float innerCone, float outerCone)
{
    spotLight.position = position;
    spotLight.direction = glm::normalize(direction);
    spotLight.ambient = glm::vec3(0.0f);
    spotLight.diffuse = color;
    spotLight.specular = color;
    spotLight.constant = 1.0f;
    spotLight.linear = 0.09f;
    spotLight.quadratic = 0.032f;
    spotLight.cutOff = glm::cos(glm::radians(innerCone));
    spotLight.outerCutOff = glm::cos(glm::radians(outerCone));
}

void LightManager::setSpotLight(const SpotLight& light)
{
    spotLight = light;
}

void LightManager::clearAllLights()
{
    clearPointLights();

    // Reset directional light to default
    directionalLight = DirectionalLight();

    // Reset spotlight to default
    spotLight = SpotLight();
    spotLightEnabled = true;
}

void LightManager::printLightStats() const
{
    std::cout << "\n[LightManager] Light Statistics:" << std::endl;
    std::cout << "  Directional Light: direction(" << directionalLight.direction.x << ", " << directionalLight.direction.y << ", " << directionalLight.direction.z << ")" << std::endl;

    std::cout << "  Point Lights: " << pointLights.size() << std::endl;
    for (size_t i = 0; i < pointLights.size(); i++) 
    {
        const auto& light = pointLights[i];
        std::cout << "    Light " << i << ": position(" << light.position.x << ", " << light.position.y << ", " << light.position.z << "), color(" << light.diffuse.x << ", " << light.diffuse.y << ", " << light.diffuse.z << ")" << std::endl;
    }

    std::cout << "  Spotlight: " << (spotLightEnabled ? "enabled" : "disabled") << " at (" << spotLight.position.x << ", " << spotLight.position.y << ", " << spotLight.position.z << ")" << std::endl; std::cout << std::endl;
}
