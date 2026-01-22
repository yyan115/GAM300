#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <functional>

class Shader;

class DirectionalShadowMap {
public:
    DirectionalShadowMap() = default;
    ~DirectionalShadowMap() { Shutdown(); }

    bool Initialize(int resolution = 2048);
    void Shutdown();

    // Render shadow map using the provided callback to draw scene
    void Render(const glm::vec3& lightDir, const glm::vec3& sceneCenter,
        float shadowDistance, std::function<void(Shader&)> renderCallback);

    // Apply shadow uniforms to a shader
    void Apply(Shader& shader, int textureUnit = 8);

    // Getters
    GLuint GetDepthTexture() const { return depthTexture; }
    GLuint GetFBO() const { return depthMapFBO; }
    bool IsInitialized() const { return initialized; }
    const glm::mat4& GetLightSpaceMatrix() const { return lightSpaceMatrix; }

    // Shadow parameters (public for editor tweaking)
    float bias = 0.005f;
    float normalBias = 0.02f;
    float softness = 1.0f;

private:
    void CalculateLightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& sceneCenter, float shadowDistance);

    GLuint depthMapFBO = 0;
    GLuint depthTexture = 0;
    int resolution = 2048;
    bool initialized = false;
    glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
    std::shared_ptr<Shader> depthShader;
};