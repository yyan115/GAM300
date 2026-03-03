#include "pch.h"
#include "Graphics/PostProcessing/Bloom/BloomEffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Graphics/PostProcessing/PostProcessingManager.hpp>

BloomEffect::BloomEffect()
    : PostProcessEffect("Bloom"),
    shader(nullptr),
    threshold(1.0f),
    intensity(1.0f),
    blurPasses(3)
{
}

BloomEffect::~BloomEffect()
{
    Shutdown();
}

bool BloomEffect::Initialize()
{
    ENGINE_PRINT("[BloomEffect] Initializing...\n");

    std::string shaderPath = ResourceManager::GetPlatformShaderPath("bloom");
    GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);
    shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(shaderGUID, shaderPath);

    if (!shader)
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Failed to load bloom shader from path: ", shaderPath, "\n");
        return false;
    }

    ENGINE_PRINT("[BloomEffect] Initialized successfully\n");
    return true;
}

void BloomEffect::Shutdown()
{
    DeleteFBOs();
    ENGINE_PRINT("[BloomEffect] Shutdown complete\n");
}

void BloomEffect::CreateFBOs(int w, int h)
{
    if (extractFBO != 0)
        DeleteFBOs();

    fboWidth = w;
    fboHeight = h;

    // Brightness extraction FBO
    glGenFramebuffers(1, &extractFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, extractFBO);
    glGenTextures(1, &extractTexture);
    glBindTexture(GL_TEXTURE_2D, extractTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, extractTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Extract FBO not complete!\n");

    // Ping FBO
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
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Ping FBO not complete!\n");

    // Pong FBO
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
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Pong FBO not complete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomEffect::DeleteFBOs()
{
    if (extractTexture != 0) { glDeleteTextures(1, &extractTexture); extractTexture = 0; }
    if (extractFBO != 0) { glDeleteFramebuffers(1, &extractFBO); extractFBO = 0; }
    if (pingTexture != 0) { glDeleteTextures(1, &pingTexture); pingTexture = 0; }
    if (pongTexture != 0) { glDeleteTextures(1, &pongTexture); pongTexture = 0; }
    if (pingFBO != 0) { glDeleteFramebuffers(1, &pingFBO); pingFBO = 0; }
    if (pongFBO != 0) { glDeleteFramebuffers(1, &pongFBO); pongFBO = 0; }
    fboWidth = 0;
    fboHeight = 0;
}

void BloomEffect::Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
{
    if (!shader || !enabled || intensity < 0.01f)
        return;

    // Create or resize FBOs if needed
    if (pingFBO == 0 || fboWidth != width || fboHeight != height)
        CreateFBOs(width, height);

    glDisable(GL_DEPTH_TEST);
    shader->Activate();
    shader->setInt("inputTexture", 0);
    shader->setVec2("texelSize", 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

    // Use bloom emission texture from MRT (per-entity bloom only, no threshold)
    // If no emission texture is set, fall back to threshold extraction from HDR
    unsigned int currentInput;
    if (bloomEmissionTexture != 0)
    {
        // Skip threshold extraction — use the emission buffer directly
        currentInput = bloomEmissionTexture;
    }
    else
    {
        // Fallback: threshold extraction from HDR buffer
        glBindFramebuffer(GL_FRAMEBUFFER, extractFBO);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->setInt("passType", 0); // extract
        shader->setFloat("threshold", threshold);
        shader->setFloat("intensity", intensity);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTexture);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        currentInput = extractTexture;
    }

    // Blur the bloom source (ping-pong)
    for (int i = 0; i < blurPasses; ++i)
    {
        // Horizontal blur
        glBindFramebuffer(GL_FRAMEBUFFER, pingFBO);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->setInt("passType", 1); // horizontal blur
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentInput);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        // Vertical blur
        glBindFramebuffer(GL_FRAMEBUFFER, pongFBO);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->setInt("passType", 2); // vertical blur
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingTexture);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        currentInput = pongTexture;
    }

    // Additive composite back onto HDR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, width, height);

    // Only write to attachment 0 (main color) during composite
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // Additive blending

    shader->setInt("passType", 3); // composite
    shader->setFloat("intensity", intensity);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pongTexture);
    PostProcessingManager::GetInstance().RenderScreenQuad();

    glDisable(GL_BLEND);

    // Cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}
