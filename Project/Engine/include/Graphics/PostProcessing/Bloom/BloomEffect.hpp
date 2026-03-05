#pragma once
#include "Graphics/PostProcessing/PostProcessEffect.hpp"
#include <memory>

class Shader;

class BloomEffect : public PostProcessEffect {
public:
    BloomEffect();
    ~BloomEffect() override;

    bool Initialize() override;
    void Shutdown() override;
    void Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height) override;

    void SetThreshold(float t) { threshold = t; }
    float GetThreshold() const { return threshold; }

    void SetIntensity(float i) { intensity = i; }
    float GetIntensity() const { return intensity; }

    void SetBlurPasses(int p) { blurPasses = p; }
    int GetBlurPasses() const { return blurPasses; }

    // Set the bloom emission texture from the MRT pass (skips threshold extraction)
    void SetBloomEmissionTexture(unsigned int tex) { bloomEmissionTexture = tex; }

private:
    std::shared_ptr<Shader> shader;
    float threshold = 1.0f;
    float intensity = 1.0f;
    int blurPasses = 3;

    // Brightness extraction FBO (fallback when no MRT emission texture)
    unsigned int extractFBO = 0;
    unsigned int extractTexture = 0;

    // Mip chain for progressive downsample/upsample bloom
    static const int MAX_MIP_LEVELS = 6;

    struct MipLevel {
        unsigned int fbo = 0;
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
    };

    MipLevel mipChain[MAX_MIP_LEVELS];
    int activeMipLevels = 0;
    int fboWidth = 0, fboHeight = 0;

    unsigned int bloomEmissionTexture = 0;  // From MRT, set by PostProcessingManager

    void CreateFBOs(int w, int h);
    void DeleteFBOs();
};
