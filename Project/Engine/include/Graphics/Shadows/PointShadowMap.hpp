#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>

class Shader;

class PointShadowMap {
public:
    PointShadowMap() = default;
    ~PointShadowMap() { Shutdown(); }

    bool Initialize(int resolution = 1024);
    void Shutdown();

    // Render shadow map for a point light
    void Render(const glm::vec3& lightPos, float farPlane, std::function<void(Shader&)> renderCallback);

    // Apply shadow uniforms to a shader
    void Apply(Shader& shader, int textureUnit, int shadowIndex);

    // Getters
    GLuint GetDepthCubemap() const { return depthCubemap; }
    GLuint GetFBO() const { return depthMapFBO; }
    bool IsInitialized() const { return initialized; }
    int GetResolution() const { return resolution; }

    // Shadow parameters
    float bias = 0.05f;

private:
    std::vector<glm::mat4> GetLightSpaceMatrices(const glm::vec3& lightPos, float nearPlane, float farPlane);

    GLuint depthMapFBO = 0;
    GLuint depthCubemap = 0;
    int resolution = 1024;
    bool initialized = false;
    std::shared_ptr<Shader> depthShader;
    float currentFarPlane = 25.0f;
};