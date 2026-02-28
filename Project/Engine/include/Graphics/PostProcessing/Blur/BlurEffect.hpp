#pragma once
#include "Graphics/PostProcessing/PostProcessEffect.hpp"
#include <memory>

class Shader;

class BlurEffect : public PostProcessEffect {
public:
    BlurEffect();
    ~BlurEffect() override;

    bool Initialize() override;
    void Shutdown() override;
    void Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height) override;

    void SetIntensity(float i) { intensity = i; }
    float GetIntensity() const { return intensity; }

    void SetRadius(float r) { radius = r; }
    float GetRadius() const { return radius; }

    void SetPasses(int p) { passes = p; }
    int GetPasses() const { return passes; }

private:
    std::shared_ptr<Shader> shader;
    float intensity = 0.0f;
    float radius = 2.0f;
    int passes = 2;

    // Ping-pong FBOs for two-pass separable blur
    unsigned int pingFBO = 0, pongFBO = 0;
    unsigned int pingTexture = 0, pongTexture = 0;
    int fboWidth = 0, fboHeight = 0;

    void CreatePingPongFBOs(int w, int h);
    void DeletePingPongFBOs();
};
