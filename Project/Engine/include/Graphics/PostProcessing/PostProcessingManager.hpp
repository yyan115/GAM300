#pragma once

#include "PostProcessEffect.hpp"
#include "HDR/HDREffect.hpp"
#include <memory>
#include <vector>
#include "Engine.h"

class PostProcessingManager {
public:
    static PostProcessingManager& GetInstance();

    bool Initialize();

    void Shutdown();

    void Process(unsigned int inputTexture, unsigned int outputFBO, int width, int height);

    unsigned int CreateHDRFramebuffer(int width, int height);

    void DeleteHDRFramebuffer();

    unsigned int GetHDRFramebuffer() const { return hdrFramebuffer; }

    unsigned int GetHDRTexture() const { return hdrColorTexture; }

    void BeginHDRRender(int width, int height);

    void EndHDRRender(unsigned int outputFBO, int width, int height);

    HDREffect* GetHDREffect() { return hdrEffect.get(); }

    void RenderScreenQuad();

private:
    PostProcessingManager() = default;
    ~PostProcessingManager() = default;

    PostProcessingManager(const PostProcessingManager&) = delete;
    PostProcessingManager& operator=(const PostProcessingManager&) = delete;

    void CreateScreenQuad();
    void DeleteScreenQuad();

    // Effects (in order of application)
    std::unique_ptr<HDREffect> hdrEffect;
    // std::unique_ptr<BloomEffect> bloomEffect; // We'll add this next
    // Add more effects here as needed

    // HDR framebuffer resources
    unsigned int hdrFramebuffer;
    unsigned int hdrColorTexture;
    unsigned int hdrDepthRenderbuffer;
    int hdrWidth;
    int hdrHeight;

    // Screen quad for rendering post-process effects
    unsigned int screenQuadVAO;
    unsigned int screenQuadVBO;

    bool initialized;
};