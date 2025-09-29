#pragma once
#include "ECS/System.hpp"
#include "Graphics/ShaderClass.h"

struct DirectionalLightComponent;
struct PointLightComponent;
struct SpotLightComponent;
class Camera;

class LightingSystem : public System {
public:
    int MAX_POINT_LIGHTS = 4;
    int MAX_SPOT_LIGHTS = 1;

    LightingSystem() = default;
    ~LightingSystem() = default;

    void SetMaxPointLights(int maxPoints) { MAX_POINT_LIGHTS = maxPoints; }
    void SetMaxSpotLights(int maxSpots) { MAX_SPOT_LIGHTS = maxSpots; }
    int GetMaxPointLights() const { return MAX_POINT_LIGHTS; }
    int GetMaxSpotLights() const { return MAX_SPOT_LIGHTS; }

    bool Initialise();
    void Update();
    void Shutdown();

    void ApplyLighting(Shader& shader);

private:
    // Simple arrays to store light data
    struct {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> ambient;
        std::vector<glm::vec3> diffuse;
        std::vector<glm::vec3> specular;
        std::vector<float> constant;
        std::vector<float> linear;
        std::vector<float> quadratic;
    } pointLightData;

    // Single directional light data
    struct {
        bool hasDirectionalLight = false;
        glm::vec3 direction;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
    } directionalLightData;

    // Single spot light data -> Multiple spot light data
    struct {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> directions;
        std::vector<glm::vec3> ambient;
        std::vector<glm::vec3> diffuse;
        std::vector<glm::vec3> specular;
        std::vector<float> constant;
        std::vector<float> linear;
        std::vector<float> quadratic;
        std::vector<float> cutOff;
        std::vector<float> outerCutOff;
    } spotLightData;

    void CollectLightData();
};