#include "pch.h"
#include "Graphics/Shadows/ShadowMap.hpp"
#include "Graphics/ShaderClass.h"
#include "Asset Manager/ResourceManager.hpp"
#include <glm/gtc/matrix_transform.hpp>

bool DirectionalShadowMap::Initialize(int res)
{
    resolution = res;

    // Create framebuffer
    glGenFramebuffers(1, &depthMapFBO);

    // Create depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifdef __ANDROID__
    // OpenGL ES doesn't support GL_CLAMP_TO_BORDER
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // No border color in ES - skip glTexParameterfv
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
#endif

    // Attach to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
#ifdef __ANDROID__
    GLenum drawBuffers = GL_NONE;
    glDrawBuffers(1, &drawBuffers);
#else
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
#endif

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "[DirectionalShadowMap] Framebuffer not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load shader
    std::string shaderPath = ResourceManager::GetPlatformShaderPath("shadow_depth");
    depthShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

    if (!depthShader)
    {
        std::cout << "[DirectionalShadowMap] Failed to load shader: " << shaderPath << std::endl;
        return false;
    }

    initialized = true;
    std::cout << "[DirectionalShadowMap] Initialized - Resolution: " << resolution
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
    depthShader = nullptr;
    initialized = false;
}

void DirectionalShadowMap::CalculateLightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& sceneCenter, float shadowDistance)
{
    glm::vec3 lightDirection = glm::normalize(lightDir);
    glm::vec3 lightPos = sceneCenter - lightDirection * shadowDistance;

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDirection, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);
    float orthoSize = shadowDistance;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, shadowDistance * 2.5f);

    lightSpaceMatrix = lightProjection * lightView;
}

void DirectionalShadowMap::Render(const glm::vec3& lightDir, const glm::vec3& sceneCenter,
    float shadowDistance, std::function<void(Shader&)> renderCallback)
{
    if (!initialized || !depthShader || !renderCallback)
    {
        return;
    }

    // Calculate light space matrix
    CalculateLightSpaceMatrix(lightDir, sceneCenter, shadowDistance);

    // Store current state
    GLint previousFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Bind and clear shadow map
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, resolution, resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Render scene
    depthShader->Activate();
    depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    renderCallback(*depthShader);

    // Restore state
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void DirectionalShadowMap::Apply(Shader& shader, int textureUnit)
{
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    shader.setInt("shadowMap", textureUnit);

    shader.setFloat("shadowBias", bias);
    shader.setFloat("shadowNormalBias", normalBias);
    shader.setFloat("shadowSoftness", softness);
}