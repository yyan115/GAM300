#include "pch.h"
#include "Graphics/PostProcessing/Bloom/BloomEffect.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Graphics/PostProcessing/PostProcessingManager.hpp>

BloomEffect::BloomEffect()
    : PostProcessEffect("Bloom"),
    shader(nullptr),
    threshold(1.0f),
    intensity(1.0f),
    blurPasses(3)
{
}

BloomEffect::~BloomEffect()
{
    Shutdown();
}

bool BloomEffect::Initialize()
{
    ENGINE_PRINT("[BloomEffect] Initializing...\n");

    std::string shaderPath = ResourceManager::GetPlatformShaderPath("bloom");
    GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);
    shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(shaderGUID, shaderPath);

    if (!shader)
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Failed to load bloom shader from path: ", shaderPath, "\n");
        return false;
    }

    ENGINE_PRINT("[BloomEffect] Initialized successfully\n");
    return true;
}

void BloomEffect::Shutdown()
{
    DeleteFBOs();
    ENGINE_PRINT("[BloomEffect] Shutdown complete\n");
}

void BloomEffect::CreateFBOs(int w, int h)
{
    if (extractFBO != 0 || activeMipLevels != 0)
        DeleteFBOs();

    fboWidth = w;
    fboHeight = h;

    // The normal renderer supplies an MRT emission texture, so only allocate
    // this full-resolution fallback when a caller actually needs extraction.
    if (bloomEmissionTexture == 0)
    {
        glGenFramebuffers(1, &extractFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, extractFBO);
        glGenTextures(1, &extractTexture);
        glBindTexture(GL_TEXTURE_2D, extractTexture);
#ifdef __ANDROID__
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
#endif
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, extractTexture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Extract FBO not complete!\n");
    }

    // Create mip chain at progressively halved resolutions.
    // Android starts at w/4 (quarter res) instead of w/2 to save 75% bandwidth on
    // the bloom pipeline. Bloom is already blurred so 1 extra level of downsample
    // is imperceptible.
    activeMipLevels = 0;
#ifdef __ANDROID__
    int mipW = w / 4;
    int mipH = h / 4;
#else
    int mipW = w / 2;
    int mipH = h / 2;
#endif

    for (int i = 0; i < MAX_MIP_LEVELS; ++i)
    {
        if (mipW < 2 || mipH < 2)
            break;

        MipLevel& mip = mipChain[i];
        mip.width = mipW;
        mip.height = mipH;

        glGenFramebuffers(1, &mip.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
        glGenTextures(1, &mip.texture);
        glBindTexture(GL_TEXTURE_2D, mip.texture);
#ifdef __ANDROID__
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, mipW, mipH, 0, GL_RGB, GL_FLOAT, nullptr);
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mipW, mipH, 0, GL_RGBA, GL_FLOAT, nullptr);
#endif
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[BloomEffect] Mip level ", i, " FBO not complete!\n");

        activeMipLevels++;
        mipW /= 2;
        mipH /= 2;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //ENGINE_PRINT("[BloomEffect] Created mip chain with ", activeMipLevels, " levels\n");
}

void BloomEffect::DeleteFBOs()
{
    if (extractTexture != 0) { glDeleteTextures(1, &extractTexture); extractTexture = 0; }
    if (extractFBO != 0) { glDeleteFramebuffers(1, &extractFBO); extractFBO = 0; }

    for (int i = 0; i < MAX_MIP_LEVELS; ++i)
    {
        MipLevel& mip = mipChain[i];
        if (mip.texture != 0) { glDeleteTextures(1, &mip.texture); mip.texture = 0; }
        if (mip.fbo != 0) { glDeleteFramebuffers(1, &mip.fbo); mip.fbo = 0; }
        mip.width = 0;
        mip.height = 0;
    }
    activeMipLevels = 0;
    resolvedBloomTexture = 0;
    fboWidth = 0;
    fboHeight = 0;
}

void BloomEffect::Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
{
    resolvedBloomTexture = 0;
    if (!shader || !enabled || intensity < 0.01f)
        return;

    // The extraction FBO is optional when the renderer supplies MRT emission.
    const bool needsExtractFBO = bloomEmissionTexture == 0;
    if (activeMipLevels == 0 || fboWidth != width || fboHeight != height ||
        (needsExtractFBO && extractFBO == 0))
        CreateFBOs(width, height);

    if (activeMipLevels == 0)
        return;

    glDisable(GL_DEPTH_TEST);
    shader->Activate();
    shader->setInt("inputTexture", 0);
    glActiveTexture(GL_TEXTURE0);

    // Determine bloom source texture
    unsigned int bloomSource;
    if (bloomEmissionTexture != 0)
    {
        // Use MRT emission buffer directly (per-entity bloom)
        bloomSource = bloomEmissionTexture;
    }
    else
    {
        // Fallback: threshold extraction from HDR buffer
        glBindFramebuffer(GL_FRAMEBUFFER, extractFBO);
        glViewport(0, 0, width, height);

        shader->setInt("passType", 0); // extract
        shader->setFloat("threshold", threshold);
        shader->setFloat("intensity", intensity);

        glBindTexture(GL_TEXTURE_2D, inputTexture);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        bloomSource = extractTexture;
    }

    // === PROGRESSIVE DOWNSAMPLE ===
    // Downsample from bloom source through each mip level
    unsigned int currentInput = bloomSource;

    // For the first downsample, texelSize is based on the source texture size
    float srcTexelW = 1.0f / static_cast<float>(width);
    float srcTexelH = 1.0f / static_cast<float>(height);

    for (int i = 0; i < activeMipLevels; ++i)
    {
        MipLevel& mip = mipChain[i];

        glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
        glViewport(0, 0, mip.width, mip.height);

        shader->setInt("passType", 1); // downsample
        shader->setVec2("texelSize", srcTexelW, srcTexelH);

        glBindTexture(GL_TEXTURE_2D, currentInput);
        PostProcessingManager::GetInstance().RenderScreenQuad();

        // Next iteration reads from this level
        currentInput = mip.texture;
        srcTexelW = 1.0f / static_cast<float>(mip.width);
        srcTexelH = 1.0f / static_cast<float>(mip.height);
    }

    // === PROGRESSIVE UPSAMPLE ===
    // Upsample from smallest mip back up, additively blending with each level.
    // scatter controls how much energy from lower (more blurred) mip levels
    // is added to each target level. 0.5 gives ~2x total energy (geometric series),
    // which is then controlled by the bloom intensity in the composite pass.
    const float effectiveScatter = scatter;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // Additive blending

    for (int i = activeMipLevels - 1; i > 0; --i)
    {
        MipLevel& targetMip = mipChain[i - 1];
        MipLevel& sourceMip = mipChain[i];

        glBindFramebuffer(GL_FRAMEBUFFER, targetMip.fbo);
        glViewport(0, 0, targetMip.width, targetMip.height);
        // Don't clear — we want to ADD to the existing downsampled content

        shader->setInt("passType", 2); // upsample
        shader->setFloat("scatter", effectiveScatter);
        shader->setVec2("texelSize",
            1.0f / static_cast<float>(sourceMip.width),
            1.0f / static_cast<float>(sourceMip.height));

        glBindTexture(GL_TEXTURE_2D, sourceMip.texture);
        PostProcessingManager::GetInstance().RenderScreenQuad();
    }

    glDisable(GL_BLEND);

#ifdef ANDROID
    // Tone mapping samples this accumulated mip directly and adds it before
    // exposure. This avoids a full render-resolution HDR framebuffer load/store
    // and one fullscreen draw.
    (void)outputFBO;
    resolvedBloomTexture = mipChain[0].texture;
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    return;
#else
    // === COMPOSITE ===
    // Additively blend the final mip level 0 result onto the HDR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
    glViewport(0, 0, width, height);

    // Only write to attachment 0 (main color) during composite.
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // Additive blending

    shader->setInt("passType", 3); // composite
    shader->setFloat("intensity", intensity);
    shader->setVec2("texelSize",
        1.0f / static_cast<float>(mipChain[0].width),
        1.0f / static_cast<float>(mipChain[0].height));

    glBindTexture(GL_TEXTURE_2D, mipChain[0].texture);
    PostProcessingManager::GetInstance().RenderScreenQuad();

    glDisable(GL_BLEND);

    // Cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
#endif
}
