#include "pch.h"
#include "Graphics/PostProcessing/Blur/BlurEffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Graphics/PostProcessing/PostProcessingManager.hpp>

BlurEffect::BlurEffect()
    : PostProcessEffect("Gaussian Blur"),
    shader(nullptr),
    intensity(0.0f),
    radius(2.0f),
    passes(2)
{
}

BlurEffect::~BlurEffect()
{
    Shutdown();
}

bool BlurEffect::Initialize()
{
    ENGINE_PRINT("[BlurEffect] Initializing...\n");

    std::string shaderPath = ResourceManager::GetPlatformShaderPath("blur");
    GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);
    shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(shaderGUID, shaderPath);

    if (!shader)
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BlurEffect] Failed to load blur shader from path: ", shaderPath, "\n");
        return false;
    }

    ENGINE_PRINT("[BlurEffect] Initialized successfully\n");
    return true;
}

void BlurEffect::Shutdown()
{
    DeletePingPongFBOs();
    ENGINE_PRINT("[BlurEffect] Shutdown complete\n");
}

void BlurEffect::CreatePingPongFBOs(int w, int h)
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
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BlurEffect] Ping FBO not complete!\n");

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
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BlurEffect] Pong FBO not complete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BlurEffect::DeletePingPongFBOs()
{
    if (pingTexture != 0) { glDeleteTextures(1, &pingTexture); pingTexture = 0; }
    if (pongTexture != 0) { glDeleteTextures(1, &pongTexture); pongTexture = 0; }
    if (pingFBO != 0) { glDeleteFramebuffers(1, &pingFBO); pingFBO = 0; }
    if (pongFBO != 0) { glDeleteFramebuffers(1, &pongFBO); pongFBO = 0; }
    fboWidth = 0;
    fboHeight = 0;
}

void BlurEffect::Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
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

    shader->Activate();
    shader->setInt("inputTexture", 0);
    shader->setFloat("blurRadius", radius);
    shader->setFloat("intensity", intensity);
    shader->setVec2("texelSize", 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

    unsigned int currentInput = inputTexture;

    for (int i = 0; i < passes; ++i)
    {
        // Horizontal pass: read currentInput -> write to pingFBO
        glBindFramebuffer(GL_FRAMEBUFFER, pingFBO);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->setBool("horizontal", true);
        // Only blend on the last pass
        shader->setFloat("intensity", (i == passes - 1) ? intensity : 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentInput);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        // Vertical pass: read pingTexture -> write to pongFBO
        glBindFramebuffer(GL_FRAMEBUFFER, pongFBO);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->setBool("horizontal", false);
        shader->setFloat("intensity", (i == passes - 1) ? intensity : 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingTexture);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        // Use pong result as input for next pass
        currentInput = pongTexture;
    }

    // Blit final result back to outputFBO (the HDR framebuffer)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, pongFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outputFBO);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}
