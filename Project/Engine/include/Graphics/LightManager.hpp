#pragma once
#include <vector>
#include "glm/glm.hpp"
#include "ECS/System.hpp"

struct DirectionalLight {
    glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    glm::vec3 ambient = glm::vec3(0.2f);
    glm::vec3 diffuse = glm::vec3(1.f);
    glm::vec3 specular = glm::vec3(1.f);
};

struct PointLight {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 ambient = glm::vec3(0.1f);
    glm::vec3 diffuse = glm::vec3(1.5f);
    glm::vec3 specular = glm::vec3(1.0f);
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
};

struct SpotLight {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 ambient = glm::vec3(0.0f);
    glm::vec3 diffuse = glm::vec3(1.0f);
    glm::vec3 specular = glm::vec3(1.0f);
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    float cutOff = 0.976f; // cos(12.5 degrees)
    float outerCutOff = 0.966f; // cos(15 degrees)
};

class LightManager : public System {
public:
    static LightManager& getInstance();

    void setDirectionalLight(const glm::vec3& direction, const glm::vec3& ambient, const glm::vec3 diffuse, const glm::vec3& specular);
    void setDirectionalLight(const glm::vec3 direction, const glm::vec3& color);

    void addPointLight(const glm::vec3& position, const glm::vec3& color, float intensity = 1.0f);
    void addPointLight(const PointLight& light); 
    void removePointLight(size_t index);
    void clearPointLights();

    void setSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float innerCone = 12.5f, float outerCone = 15.0f);
    void setSpotLight(const SpotLight& light);
    void enableSpotLight(bool enable) { spotLightEnabled = enable; }

    // Getters
    const DirectionalLight& getDirectionalLight() const { return directionalLight; }
    const std::vector<PointLight>& getPointLights() const { return pointLights; }
    const SpotLight& getSpotLight() const { return spotLight; }
    bool isSpotLightEnabled() const { return spotLightEnabled; }

    size_t getPointLightCount() const { return pointLights.size(); }
    void clearAllLights();

    void printLightStats() const;

    // Prevent copying
    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

private:
    LightManager() = default;
    ~LightManager() = default;

    DirectionalLight directionalLight;
    std::vector<PointLight> pointLights;
    SpotLight spotLight;
    bool spotLightEnabled = true;
};
