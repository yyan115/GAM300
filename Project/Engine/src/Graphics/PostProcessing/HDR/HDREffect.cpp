#include "pch.h"
#include "Graphics/PostProcessing/HDR/HDREffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Graphics/PostProcessing/PostProcessingManager.hpp>

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

    std::string shaderPath = ResourceManager::GetPlatformShaderPath("tonemapping");

    GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);

    shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(shaderGUID, shaderPath); 

    if (!shader) 
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[HDREffect] Failed to load tone mapping shader from path: ", shaderPath, "\n"); 
        return false;
    }

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
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[HDREffect] Apply called but shader not ready! enabled=", enabled, " shader=", (shader != nullptr), "\n");
        return;
    }

    static bool firstCall = true;
    if (firstCall) {
        ENGINE_PRINT("[HDREffect] First Apply call - inputTex: ", inputTexture, " outputFBO: ", outputFBO, " size: ", width, "x", height, "\n");
        ENGINE_PRINT("[HDREffect] Shader ID: ", shader->ID, "\n");
        firstCall = false;
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