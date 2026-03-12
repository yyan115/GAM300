#pragma once
#include "Graphics/PostProcessing/PostProcessEffect.hpp"
#include <memory>

class Shader;

class DirectionalBlurEffect : public PostProcessEffect {
public:
    DirectionalBlurEffect();
    ~DirectionalBlurEffect() override;

    bool Initialize() override;
    void Shutdown() override;
    void Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height) override;

    void SetIntensity(float i) { intensity = i; }
    float GetIntensity() const { return intensity; }

    void SetStrength(float s) { strength = s; }
    float GetStrength() const { return strength; }

    void SetAngle(float a) { angle = a; }
    float GetAngle() const { return angle; }

    void SetSamples(int s) { samples = s; }
    int GetSamples() const { return samples; }

private:
    std::shared_ptr<Shader> shader;
    float intensity = 0.0f;
    float strength = 5.0f;
    float angle = 0.0f;
    int samples = 8;

    // Ping-pong FBOs (mirrors BlurEffect exactly)
    unsigned int pingFBO = 0, pongFBO = 0;
    unsigned int pingTexture = 0, pongTexture = 0;
    int fboWidth = 0, fboHeight = 0;

    void CreatePingPongFBOs(int w, int h);
    void DeletePingPongFBOs();
};
