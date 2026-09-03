#include "pch.h"
#include "Graphics/PostProcessing/HDR/HDREffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Graphics/PostProcessing/PostProcessingManager.hpp>

HDREffect::HDREffect()
    : PostProcessEffect("HDR Tone Mapping"),
    shader(nullptr),
    exposure(1.3f),
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
    if (!shader) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[HDREffect] Apply called but shader not ready! shader=", (shader != nullptr), "\n");
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

#ifdef ANDROID
    // This pass overwrites every pixel. Declaring the old contents dead lets
    // tile-based GPUs skip loading the previous window surface into tiles.
    const GLenum discardedAttachment =
        outputFBO == 0 ? GL_COLOR : GL_COLOR_ATTACHMENT0;
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &discardedAttachment);
#endif

    // The fullscreen pass fully overwrites the output. Explicitly disable
    // blending so prior framebuffer contents cannot affect the result.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Activate shader and set uniforms
    shader->Activate();
    shader->setFloat("exposure", exposure);
#ifdef ANDROID
    // Gamma runs at native output resolution. Supply the reciprocal once from
    // the CPU so mobile fragment lanes only perform the required pow.
    shader->setFloat("inverseGamma", 1.0f / gamma);
    shader->setInt("bloomTexture", 1);
    shader->setFloat("bloomIntensity", bloomIntensity);
#else
    shader->setFloat("gamma", gamma);
#endif
    shader->setInt("hdrBuffer", 0);
    shader->setInt("toneMappingMode", static_cast<int>(toneMappingMode));
    shader->setBool("enableTonemapping", enabled);  // Pass enabled state to shader

    // Android uses a deliberately compact tone-mapping shader. These desktop
    // effects are absent there, so avoid uniform lookups and texture-unit work.
#ifndef ANDROID
    // Vignette uniforms
    shader->setBool("vignetteEnabled", vignetteEnabled);
    shader->setFloat("vignetteIntensity", vignetteIntensity);
    shader->setFloat("vignetteSmoothness", vignetteSmoothness);
    shader->setVec3("vignetteColor", vignetteColor);

    // Color Grading uniforms
    shader->setBool("colorGradingEnabled", colorGradingEnabled);
    shader->setFloat("cgBrightness", cgBrightness);
    shader->setFloat("cgContrast", cgContrast);
    shader->setFloat("cgSaturation", cgSaturation);
    shader->setVec3("cgTint", cgTint);

    // Chromatic Aberration uniforms
    shader->setBool("caEnabled", caEnabled);
    shader->setFloat("caIntensity", caIntensity);
    shader->setFloat("caPadding", caPadding);

    // SSAO uniforms
    shader->setBool("ssaoEnabled", ssaoEnabled);
    shader->setInt("ssaoTexture", 1);
#endif

    // Bind input HDR texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);

#ifdef ANDROID
    if (bloomIntensity > 0.0f) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomTexture);
        glActiveTexture(GL_TEXTURE0);
    }
#else
    // Bind SSAO texture
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, ssaoEnabled ? ssaoTexture : 0);
    glActiveTexture(GL_TEXTURE0);
#endif

    // Render fullscreen quad (provided by PostProcessingManager)
    PostProcessingManager::GetInstance().RenderScreenQuad();

    // Cleanup
#ifdef ANDROID
    if (bloomIntensity > 0.0f) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }
#endif
    glBindTexture(GL_TEXTURE_2D, 0);

    // Re-enable depth testing
    glEnable(GL_DEPTH_TEST);
}
