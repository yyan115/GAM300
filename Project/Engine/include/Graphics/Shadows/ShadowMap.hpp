#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declarations
class Shader;

/**
 * @brief Manages a single directional shadow map
 *
 * This class encapsulates the framebuffer, depth texture, and light-space matrix
 * needed for directional shadow mapping.
 */
class DirectionalShadowMap
{
public:
    DirectionalShadowMap() = default;
    ~DirectionalShadowMap();

    // Disable copying (OpenGL resources)
    DirectionalShadowMap(const DirectionalShadowMap&) = delete;
    DirectionalShadowMap& operator=(const DirectionalShadowMap&) = delete;

    // Allow moving
    DirectionalShadowMap(DirectionalShadowMap&& other) noexcept;
    DirectionalShadowMap& operator=(DirectionalShadowMap&& other) noexcept;

    /**
     * @brief Initialize the shadow map with given resolution
     * @param resolution Width and height of the shadow map (square texture)
     * @return true if initialization succeeded
     */
    bool Initialize(int resolution = 2048);

    /**
     * @brief Clean up OpenGL resources
     */
    void Shutdown();

    /**
     * @brief Bind the shadow map FBO for rendering the shadow pass
     */
    void BindForWriting();

    /**
     * @brief Bind the shadow map texture for reading during the main render pass
     * @param textureUnit The texture unit to bind to (e.g., GL_TEXTURE1)
     */
    void BindForReading(unsigned int textureUnit);

    /**
     * @brief Unbind the FBO (restore default framebuffer)
     */
    void Unbind();

    /**
     * @brief Calculate the light-space matrix for a directional light
     * @param lightDirection The direction the light is pointing
     * @param sceneCenter The center of the scene to focus on
     * @param sceneRadius How far from center to include in shadow map
     */
    void CalculateLightSpaceMatrix(const glm::vec3& lightDirection,
        const glm::vec3& sceneCenter = glm::vec3(0.0f),
        float sceneRadius = 25.0f);

    /**
     * @brief Get the light-space transformation matrix
     */
    const glm::mat4& GetLightSpaceMatrix() const { return lightSpaceMatrix; }

    /**
     * @brief Get the shadow map texture ID
     */
    unsigned int GetDepthTexture() const { return depthTexture; }

    /**
     * @brief Get the shadow map resolution
     */
    int GetResolution() const { return resolution; }

    /**
     * @brief Check if the shadow map is initialized
     */
    bool IsInitialized() const { return initialized; }

    // Shadow parameters (can be adjusted per-light)
    float bias = 0.005f;           // Depth bias to prevent shadow acne
    float normalBias = 0.02f;      // Normal-based bias for additional acne prevention
    float softness = 1.0f;         // PCF sampling radius (1.0 = 1 texel)

private:
    unsigned int depthMapFBO = 0;
    unsigned int depthTexture = 0;
    int resolution = 2048;
    bool initialized = false;

    glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
    glm::mat4 lightProjection = glm::mat4(1.0f);
    glm::mat4 lightView = glm::mat4(1.0f);
};