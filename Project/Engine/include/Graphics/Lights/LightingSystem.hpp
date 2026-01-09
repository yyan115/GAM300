#pragma once
#include "ECS/System.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Shadows/ShadowMap.hpp"

struct DirectionalLightComponent;
struct PointLightComponent;
struct SpotLightComponent;
class Camera;

class LightingSystem : public System {
public:
    int MAX_POINT_LIGHTS = 32;
    int MAX_SPOT_LIGHTS = 16;

    LightingSystem() = default;
    ~LightingSystem() = default;

    int GetMaxPointLights() const { return MAX_POINT_LIGHTS; }
    int GetMaxSpotLights() const { return MAX_SPOT_LIGHTS; }

    bool Initialise();
    void Update();
    void Shutdown();

    void ApplyLighting(Shader& shader);

    // ========================================================================
    // SHADOW MAPPING - NEW
    // ========================================================================

    /**
     * @brief Render the shadow pass for all shadow-casting lights
     * Call this BEFORE the main render pass
     */
    void RenderShadowMaps();

    /**
     * @brief Bind shadow maps and set shadow uniforms for a shader
     * Call this during the main render pass, after ApplyLighting
     */
    void ApplyShadows(Shader& shader);

    /**
     * @brief Get the depth shader for shadow pass rendering
     */
    Shader* GetShadowDepthShader() const { return shadowDepthShader.get(); }

    /**
     * @brief Get the directional light space matrix for external use
     */
    const glm::mat4& GetDirectionalLightSpaceMatrix() const;

    // Shadow settings
    bool shadowsEnabled = true;
    int shadowMapResolution = 2048;
    float shadowDistance = 50.0f;  // How far shadows extend from camera

    // ========================================================================
    // AMBIENT LIGHTING
    // ========================================================================
    enum class AmbientMode {
        Color,
        Gradient,
        Skybox
    };

    AmbientMode ambientMode = AmbientMode::Color;
    glm::vec3 ambientSky = glm::vec3(0.7f, 0.7f, 0.7f);
    glm::vec3 ambientEquator = glm::vec3(0.5f, 0.5f, 0.5f);
    glm::vec3 ambientGround = glm::vec3(0.3f, 0.3f, 0.3f);
    float ambientIntensity = 1.0f;

    void SetAmbientMode(AmbientMode mode) { ambientMode = mode; }
    void SetAmbientSky(glm::vec3 color) { ambientSky = color; }
    void SetAmbientEquator(glm::vec3 color) { ambientEquator = color; }
    void SetAmbientGround(glm::vec3 color) { ambientGround = color; }
    void SetAmbientIntensity(float intensity) { ambientIntensity = intensity; }

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
        std::vector<float> intensity;
    } pointLightData;

    // Single directional light data
    struct {
        bool hasDirectionalLight = false;
        glm::vec3 direction;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float intensity = 1.0f;
        //bool castShadows = true;  // NEW
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
        std::vector<float> intensity;
    } spotLightData;

    void CollectLightData();

    // ========================================================================
    // SHADOW MAPPING RESOURCES - NEW
    // ========================================================================

    DirectionalShadowMap directionalShadowMap;
    std::shared_ptr<Shader> shadowDepthShader;

    bool InitializeShadowMaps();
    void RenderDirectionalShadowMap();

    // Callback to render scene for shadow pass (set by GraphicsManager)
    std::function<void(Shader&)> shadowRenderCallback;

public:
    /**
     * @brief Set the callback that renders the scene for shadow maps
     * GraphicsManager should set this during initialization
     */
    void SetShadowRenderCallback(std::function<void(Shader&)> callback) {
        shadowRenderCallback = callback;
    }
};