#include "pch.h"
#include "Graphics/PostProcessing/HDR/HDREffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>

HDREffect::HDREffect()
    : PostProcessEffect("HDR Tone Mapping"),
    shader(nullptr),
    exposure(1.0f),
    gamma(2.2f),
    toneMappingMode(ToneMappingMode::REINHARD)
{
}

HDREffect::~HDREffect()
{
    Shutdown();
}

bool HDREffect::Initialize()
{
    ENGINE_PRINT("[HDREffect] Initializing...\n");

    // Load tone mapping shader
    std::string shaderPath = "Resources/Shaders/tonemapping";
    auto shaderPtr = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

    if (!shaderPtr) 
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[HDREffect] Failed to load tone mapping shader!\n");
        return false;
    }

    shader = std::make_unique<Shader>(shaderPtr.get());

    ENGINE_PRINT("[HDREffect] Initialized successfully\n");
    return true;
}

void HDREffect::Shutdown()
{
    ENGINE_PRINT("[HDREffect] Shutdown complete\n");
}

void HDREffect::Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
{
    if (!enabled || !shader) {
        return;
    }

    // Bind output framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, width, height);

    // Disable depth testing for post-processing
    glDisable(GL_DEPTH_TEST);

    // Clear the output
    glClear(GL_COLOR_BUFFER_BIT);

    // Activate shader and set uniforms
    shader->Activate();
    shader->setFloat("exposure", exposure);
    shader->setFloat("gamma", gamma);
    shader->setInt("hdrBuffer", 0);
    shader->setInt("toneMappingMode", static_cast<int>(toneMappingMode));

    // Bind input HDR texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);

    // Render fullscreen quad (provided by PostProcessingManager)
    PostProcessingManager::GetInstance().RenderScreenQuad();

    // Cleanup
    glBindTexture(GL_TEXTURE_2D, 0);

    // Re-enable depth testing
    glEnable(GL_DEPTH_TEST);
}