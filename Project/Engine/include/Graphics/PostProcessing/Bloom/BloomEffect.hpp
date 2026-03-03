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

    // Brightness extraction FBO
    unsigned int extractFBO = 0;
    unsigned int extractTexture = 0;

    // Ping-pong FBOs for blurring bright pixels
    unsigned int pingFBO = 0, pongFBO = 0;
    unsigned int pingTexture = 0, pongTexture = 0;
    int fboWidth = 0, fboHeight = 0;

    unsigned int bloomEmissionTexture = 0;  // From MRT, set by PostProcessingManager

    void CreateFBOs(int w, int h);
    void DeleteFBOs();
};
