#pragma once

#include "PostProcessEffect.hpp"
#include "HDR/HDREffect.hpp"
#include "Blur/BlurEffect.hpp"
#include <memory>
#include <vector>
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

    unsigned int GetHDRDepthTexture() const { return hdrDepthTexture; }

    void BeginHDRRender(int width, int height);

    void EndHDRRender(unsigned int outputFBO, int width, int height);

    HDREffect* GetHDREffect() { return hdrEffect.get(); }
    BlurEffect* GetBlurEffect() { return blurEffect.get(); }

    void RenderScreenQuad();

private:
    PostProcessingManager() = default;
    ~PostProcessingManager() = default;

    PostProcessingManager(const PostProcessingManager&) = delete;
    PostProcessingManager& operator=(const PostProcessingManager&) = delete;

    void CreateScreenQuad();
    void DeleteScreenQuad();

    // Effects (in order of application)
    std::unique_ptr<BlurEffect> blurEffect;
    std::unique_ptr<HDREffect> hdrEffect;
    // std::unique_ptr<BloomEffect> bloomEffect; // We'll add this next
    // Add more effects here as needed

    // HDR framebuffer resources
    unsigned int hdrFramebuffer{};
    unsigned int hdrColorTexture{};
    unsigned int hdrDepthTexture{};
    int hdrWidth{};
    int hdrHeight{};

    // Screen quad for rendering post-process effects
    unsigned int screenQuadVAO{};
    unsigned int screenQuadVBO{};

    bool initialized{};
};
