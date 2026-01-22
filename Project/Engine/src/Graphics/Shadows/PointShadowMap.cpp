#include "pch.h"
#include "Graphics/Shadows/PointShadowMap.hpp"
#include "Graphics/ShaderClass.h"
#include "Asset Manager/ResourceManager.hpp"
#include <glm/gtc/matrix_transform.hpp>

bool PointShadowMap::Initialize(int res)
{
    resolution = res;

    // Create framebuffer
    glGenFramebuffers(1, &depthMapFBO);

    // Create cubemap texture
    glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

    // Allocate storage for each of the 6 faces
    for (int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
            resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);

#ifdef __ANDROID__
    // On Android/ES, we attach one face at a time during rendering
    // Just verify framebuffer can be created
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X, depthCubemap, 0);
#else
    // On desktop, attach entire cubemap (for geometry shader approach)
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
#endif

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "[PointShadowMap] Framebuffer not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load shader
#ifdef __ANDROID__
    // Android uses simple vertex/fragment shader (no geometry shader)
    std::string shaderPath = ResourceManager::GetPlatformShaderPath("shadow_depth_point_es");
#else
    // Desktop uses geometry shader for single-pass rendering
    std::string shaderPath = ResourceManager::GetPlatformShaderPath("shadow_depth_point");
#endif

    depthShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

    if (!depthShader)
    {
        std::cout << "[PointShadowMap] Failed to load shader: " << shaderPath << std::endl;
        return false;
    }

    initialized = true;
    std::cout << "[PointShadowMap] Initialized - Resolution: " << resolution
        << ", FBO: " << depthMapFBO << ", Cubemap: " << depthCubemap << std::endl;
    return true;
}

void PointShadowMap::Shutdown()
{
    if (depthCubemap != 0)
    {
        glDeleteTextures(1, &depthCubemap);
        depthCubemap = 0;
    }
    if (depthMapFBO != 0)
    {
        glDeleteFramebuffers(1, &depthMapFBO);
        depthMapFBO = 0;
    }
    depthShader = nullptr;
    initialized = false;
}

std::vector<glm::mat4> PointShadowMap::GetLightSpaceMatrices(const glm::vec3& lightPos, float nearPlane, float farPlane)
{
    // 90 degree FOV to cover each cube face exactly
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

    std::vector<glm::mat4> shadowTransforms;

    // +X (right)
    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
    // -X (left)
    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
    // +Y (up)
    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
    // -Y (down)
    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
    // +Z (front)
    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
    // -Z (back)
    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

    return shadowTransforms;
}

void PointShadowMap::Render(const glm::vec3& lightPos, float farPlane, std::function<void(Shader&)> renderCallback)
{
    if (!initialized || !depthShader || !renderCallback)
    {
        return;
    }

    currentFarPlane = farPlane;

    // Store current state
    GLint previousFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Get light space matrices for all 6 faces
    std::vector<glm::mat4> shadowTransforms = GetLightSpaceMatrices(lightPos, 0.1f, farPlane);

    // Bind framebuffer and set viewport
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, resolution, resolution);

#ifdef __ANDROID__
    // ========================================================================
    // ANDROID: 6-Pass Rendering (no geometry shader)
    // Render scene once for each cubemap face
    // ========================================================================

    depthShader->Activate();
    depthShader->setVec3("lightPos", lightPos);
    depthShader->setFloat("farPlane", farPlane);

    // Render each face separately
    for (int face = 0; face < 6; ++face)
    {
        // Attach this face of the cubemap to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depthCubemap, 0);

        // Clear depth buffer for this face
        glClear(GL_DEPTH_BUFFER_BIT);

        // Set the light space matrix for this face
        depthShader->setMat4("lightSpaceMatrix", shadowTransforms[face]);

        // Render scene
        renderCallback(*depthShader);
    }

#else
    // ========================================================================
    // DESKTOP: Single-Pass Rendering (with geometry shader)
    // Geometry shader duplicates geometry to all 6 faces
    // ========================================================================

    glClear(GL_DEPTH_BUFFER_BIT);

    depthShader->Activate();
    for (int i = 0; i < 6; ++i)
    {
        depthShader->setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
    }
    depthShader->setVec3("lightPos", lightPos);
    depthShader->setFloat("farPlane", farPlane);

    // Render scene once - geometry shader handles all 6 faces
    renderCallback(*depthShader);

#endif

    // Restore state
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void PointShadowMap::Apply(Shader& shader, int textureUnit, int shadowIndex)
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    shader.setInt("pointShadowMaps[" + std::to_string(shadowIndex) + "]", textureUnit);
}