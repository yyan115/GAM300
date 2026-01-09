#include "pch.h"
#include "Graphics/Shadows/ShadowMap.hpp"
#include <iostream>

DirectionalShadowMap::~DirectionalShadowMap()
{
    Shutdown();
}

DirectionalShadowMap::DirectionalShadowMap(DirectionalShadowMap&& other) noexcept
    : depthMapFBO(other.depthMapFBO)
    , depthTexture(other.depthTexture)
    , resolution(other.resolution)
    , initialized(other.initialized)
    , lightSpaceMatrix(other.lightSpaceMatrix)
    , lightProjection(other.lightProjection)
    , lightView(other.lightView)
    , bias(other.bias)
    , normalBias(other.normalBias)
    , softness(other.softness)
{
    // Prevent the moved-from object from deleting our resources
    other.depthMapFBO = 0;
    other.depthTexture = 0;
    other.initialized = false;
}

DirectionalShadowMap& DirectionalShadowMap::operator=(DirectionalShadowMap&& other) noexcept
{
    if (this != &other)
    {
        // Clean up our current resources
        Shutdown();

        // Take ownership of other's resources
        depthMapFBO = other.depthMapFBO;
        depthTexture = other.depthTexture;
        resolution = other.resolution;
        initialized = other.initialized;
        lightSpaceMatrix = other.lightSpaceMatrix;
        lightProjection = other.lightProjection;
        lightView = other.lightView;
        bias = other.bias;
        normalBias = other.normalBias;
        softness = other.softness;

        // Prevent double deletion
        other.depthMapFBO = 0;
        other.depthTexture = 0;
        other.initialized = false;
    }
    return *this;
}

bool DirectionalShadowMap::Initialize(int res)
{
    if (initialized)
    {
        std::cout << "[ShadowMap] Already initialized, shutting down first..." << std::endl;
        Shutdown();
    }

    resolution = res;

    // Create framebuffer
    glGenFramebuffers(1, &depthMapFBO);

    // Create depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    // Texture parameters for shadow mapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Use CLAMP_TO_BORDER with white border color
    // This ensures areas outside the shadow map are lit (not in shadow)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach depth texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    // We don't need color output for shadow map
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "[ShadowMap] ERROR: Framebuffer not complete! Status: " << status << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        Shutdown();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    initialized = true;

    std::cout << "[ShadowMap] Initialized - Resolution: " << resolution << "x" << resolution
        << ", FBO: " << depthMapFBO << ", Texture: " << depthTexture << std::endl;

    return true;
}

void DirectionalShadowMap::Shutdown()
{
    if (depthTexture != 0)
    {
        glDeleteTextures(1, &depthTexture);
        depthTexture = 0;
    }

    if (depthMapFBO != 0)
    {
        glDeleteFramebuffers(1, &depthMapFBO);
        depthMapFBO = 0;
    }

    initialized = false;
    std::cout << "[ShadowMap] Shutdown complete" << std::endl;
}

void DirectionalShadowMap::BindForWriting()
{
    glViewport(0, 0, resolution, resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void DirectionalShadowMap::BindForReading(unsigned int textureUnit)
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
}

void DirectionalShadowMap::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DirectionalShadowMap::CalculateLightSpaceMatrix(const glm::vec3& lightDirection,
    const glm::vec3& sceneCenter,
    float sceneRadius)
{
    // Normalize the light direction
    glm::vec3 lightDir = glm::normalize(lightDirection);

    // Position the light "camera" far enough back to see the whole scene
    // The light looks AT the scene center FROM a position along -lightDirection
    glm::vec3 lightPos = sceneCenter - lightDir * sceneRadius;

    // Create the view matrix (looking at scene center)
    // We need an up vector that's not parallel to lightDir
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    lightView = glm::lookAt(lightPos, sceneCenter, up);

    // Create orthographic projection that encompasses the scene
    // The bounds should be slightly larger than the scene to avoid clipping
    float orthoSize = sceneRadius * 1.5f;
    lightProjection = glm::ortho(-orthoSize, orthoSize,
        -orthoSize, orthoSize,
        0.1f, sceneRadius * 3.0f);

    // Combine into light-space matrix
    lightSpaceMatrix = lightProjection * lightView;
}