#include "pch.h"
#include "Graphics/PostProcessing/Blur/DirectionalBlurEffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Graphics/PostProcessing/PostProcessingManager.hpp>

DirectionalBlurEffect::DirectionalBlurEffect()
    : PostProcessEffect("Directional Blur"),
    shader(nullptr),
    intensity(0.0f),
    strength(5.0f),
    angle(0.0f),
    samples(8)
{
}

DirectionalBlurEffect::~DirectionalBlurEffect()
{
    Shutdown();
}

bool DirectionalBlurEffect::Initialize()
{
    ENGINE_PRINT("[DirectionalBlurEffect] Initializing...\n");

    std::string shaderPath = ResourceManager::GetPlatformShaderPath("directional_blur");
    GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);
    shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(shaderGUID, shaderPath);

    if (!shader)
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[DirectionalBlurEffect] Failed to load directional_blur shader from path: ", shaderPath, "\n");
        return false;
    }

    ENGINE_PRINT("[DirectionalBlurEffect] Initialized successfully\n");
    return true;
}

void DirectionalBlurEffect::Shutdown()
{
    DeletePingPongFBOs();
    ENGINE_PRINT("[DirectionalBlurEffect] Shutdown complete\n");
}

// Mirrors BlurEffect::CreatePingPongFBOs exactly
void DirectionalBlurEffect::CreatePingPongFBOs(int w, int h)
{
    // Delete existing if size changed
    if (pingFBO != 0)
        DeletePingPongFBOs();

    fboWidth = w;
    fboHeight = h;

    // Create ping FBO + texture
    glGenFramebuffers(1, &pingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, pingFBO);

    glGenTextures(1, &pingTexture);
    glBindTexture(GL_TEXTURE_2D, pingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[DirectionalBlurEffect] Ping FBO not complete!\n");

    // Create pong FBO + texture
    glGenFramebuffers(1, &pongFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, pongFBO);

    glGenTextures(1, &pongTexture);
    glBindTexture(GL_TEXTURE_2D, pongTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pongTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[DirectionalBlurEffect] Pong FBO not complete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DirectionalBlurEffect::DeletePingPongFBOs()
{
    if (pingTexture != 0) { glDeleteTextures(1, &pingTexture); pingTexture = 0; }
    if (pongTexture != 0) { glDeleteTextures(1, &pongTexture); pongTexture = 0; }
    if (pingFBO != 0) { glDeleteFramebuffers(1, &pingFBO); pingFBO = 0; }
    if (pongFBO != 0) { glDeleteFramebuffers(1, &pongFBO); pongFBO = 0; }
    fboWidth = 0;
    fboHeight = 0;
}

// Mirrors BlurEffect::Apply structure exactly
void DirectionalBlurEffect::Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
{
    if (!shader)
        return;

    // Early out if no blur needed
    if (intensity < 0.01f)
        return;

    // Create or resize ping-pong FBOs if needed
    if (pingFBO == 0 || fboWidth != width || fboHeight != height)
        CreatePingPongFBOs(width, height);

    glDisable(GL_DEPTH_TEST);

    // Convert angle (degrees) to normalized direction vector
    float rad = glm::radians(angle);
    float dirX = cos(rad);
    float dirY = sin(rad);

    shader->Activate();
    shader->setInt("inputTexture", 0);
    shader->setFloat("intensity", intensity);
    shader->setFloat("blurStrength", strength);
    shader->setVec2("blurDirection", dirX, dirY);
    shader->setVec2("texelSize", 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
    shader->setInt("samples", samples);

    // Pass 1: read inputTexture -> write to pingFBO
    glBindFramebuffer(GL_FRAMEBUFFER, pingFBO);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    PostProcessingManager::GetInstance().RenderScreenQuad();

    // Blit final result back to outputFBO (the HDR framebuffer)
    // Identical to BlurEffect lines 153-156
    glBindFramebuffer(GL_READ_FRAMEBUFFER, pingFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outputFBO);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Cleanup - identical to BlurEffect lines 158-161
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}
