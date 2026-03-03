#pragma once

#include "PostProcessEffect.hpp"
#include "HDR/HDREffect.hpp"
#include "Blur/BlurEffect.hpp"
#include "Bloom/BloomEffect.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Engine.h"

class PostProcessingManager {
public:
    static ENGINE_API PostProcessingManager& GetInstance();

    bool Initialize();

    void Shutdown();

    void Process(unsigned int inputTexture, unsigned int outputFBO, int width, int height);

    unsigned int CreateHDRFramebuffer(int width, int height);

    void DeleteHDRFramebuffer();

    unsigned int GetHDRFramebuffer() const { return hdrFramebuffer; }

    unsigned int GetHDRTexture() const { return hdrColorTexture; }

    unsigned int GetBloomEmissionTexture() const { return hdrBloomEmissionTexture; }

    void BeginHDRRender(int width, int height);

    void EndHDRRender(unsigned int outputFBO, int width, int height);

    HDREffect* GetHDREffect() { return hdrEffect.get(); }
    BlurEffect* GetBlurEffect() { return blurEffect.get(); }
    BloomEffect* GetBloomEffect() { return bloomEffect.get(); }

    void RenderScreenQuad();

    // Enable/disable MRT for bloom emission writing during scene rendering
    void EnableBloomMRT();
    void DisableBloomMRT();

    // Layer exclusion from post-processing (bitmask: bit N = layer N excluded)
    void SetExcludedLayerMask(uint32_t mask) { excludedLayerMask = mask; }
    uint32_t GetExcludedLayerMask() const { return excludedLayerMask; }
    void ExcludeLayer(int layerIndex) { if (layerIndex >= 0 && layerIndex < 32) excludedLayerMask |= (1u << layerIndex); }
    void IncludeLayer(int layerIndex) { if (layerIndex >= 0 && layerIndex < 32) excludedLayerMask &= ~(1u << layerIndex); }
    bool IsLayerExcluded(int layerIndex) const { return layerIndex >= 0 && layerIndex < 32 && (excludedLayerMask & (1u << layerIndex)) != 0; }

    // Vignette settings (applied in tonemapping shader)
    void SetVignetteEnabled(bool e) { vignetteEnabled = e; }
    void SetVignetteIntensity(float i) { vignetteIntensity_ = i; }
    void SetVignetteSmoothness(float s) { vignetteSmoothness_ = s; }
    void SetVignetteColor(const glm::vec3& c) { vignetteColor_ = c; }
    bool GetVignetteEnabled() const { return vignetteEnabled; }
    float GetVignetteIntensity() const { return vignetteIntensity_; }
    float GetVignetteSmoothness() const { return vignetteSmoothness_; }
    glm::vec3 GetVignetteColor() const { return vignetteColor_; }

    // Color Grading settings (applied in tonemapping shader)
    void SetColorGradingEnabled(bool e) { colorGradingEnabled = e; }
    void SetCGBrightness(float b) { cgBrightness_ = b; }
    void SetCGContrast(float c) { cgContrast_ = c; }
    void SetCGSaturation(float s) { cgSaturation_ = s; }
    void SetCGTint(const glm::vec3& t) { cgTint_ = t; }
    bool GetColorGradingEnabled() const { return colorGradingEnabled; }
    float GetCGBrightness() const { return cgBrightness_; }
    float GetCGContrast() const { return cgContrast_; }
    float GetCGSaturation() const { return cgSaturation_; }
    glm::vec3 GetCGTint() const { return cgTint_; }

    // Reset all runtime post-processing state to defaults
    // Call this when exiting play mode to prevent stale effects persisting
    ENGINE_API void ResetRuntimeState();

private:
    PostProcessingManager() = default;
    ~PostProcessingManager() = default;

    PostProcessingManager(const PostProcessingManager&) = delete;
    PostProcessingManager& operator=(const PostProcessingManager&) = delete;

    void CreateScreenQuad();
    void DeleteScreenQuad();

    // Effects (in order of application)
    std::unique_ptr<BlurEffect> blurEffect;
    std::unique_ptr<BloomEffect> bloomEffect;
    std::unique_ptr<HDREffect> hdrEffect;

    // HDR framebuffer resources
    unsigned int hdrFramebuffer{};
    unsigned int hdrColorTexture{};
    unsigned int hdrBloomEmissionTexture{};  // MRT attachment 1: per-entity bloom emission
    unsigned int hdrDepthRenderbuffer{};
    int hdrWidth{};
    int hdrHeight{};

    // Screen quad for rendering post-process effects
    unsigned int screenQuadVAO{};
    unsigned int screenQuadVBO{};

    bool initialized{};

    // Layer exclusion mask
    uint32_t excludedLayerMask = 0;

    // Vignette state
    bool vignetteEnabled = false;
    float vignetteIntensity_ = 0.5f;
    float vignetteSmoothness_ = 0.5f;
    glm::vec3 vignetteColor_ = glm::vec3(0.0f);

    // Color Grading state
    bool colorGradingEnabled = false;
    float cgBrightness_ = 0.0f;
    float cgContrast_ = 1.0f;
    float cgSaturation_ = 1.0f;
    glm::vec3 cgTint_ = glm::vec3(1.0f);
};
