#include "pch.h"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/VAO.h"
#include "Logging.hpp"

namespace {
void CheckGLError(const char* location) {
#if defined(GAM300_GL_VALIDATION) || defined(_WIN32) || (!defined(NDEBUG) && !defined(ANDROID))
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[OpenGL Error] at ", location, ": ", err, "\n");
    }
#else
    // glGetError can synchronize with the driver. Release rendering relies on
    // explicit framebuffer/shader validation performed during initialization.
    (void)location;
#endif
}
}

PostProcessingManager& PostProcessingManager::GetInstance() 
{
    static PostProcessingManager instance;
    return instance;
}

bool PostProcessingManager::Initialize()
{
    if (initialized) 
    {
        ENGINE_PRINT("[PostProcessingManager] Already initialized\n");
        return true;
    }

    ENGINE_PRINT("[PostProcessingManager] Initializing...\n");

    // Initialize member variables
    hdrFramebuffer = 0;
    hdrColorTexture = 0;
    hdrBloomEmissionTexture = 0;
    hdrDepthTexture = 0;
    hdrDepthRenderbuffer = 0;
    hdrWidth = 0;
    hdrHeight = 0;
    screenQuadVAO = 0;
    screenQuadVBO = 0;
    initialized = false;

    // Create screen quad for rendering
    CreateScreenQuad();

    // Initialize HDR effect
    hdrEffect = std::make_unique<HDREffect>();
    if (!hdrEffect->Initialize()) 
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize HDR effect!\n");
        return false;
    }

    // Initialize Blur effect (applied before HDR tonemapping)
    blurEffect = std::make_unique<BlurEffect>();
    if (!blurEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize Blur effect!\n");
        return false;
    }

    // Android's lean tone-mapping shader does not consume SSAO. Do not create
    // or execute an invisible two-pass effect on bandwidth-limited mobile GPUs.
#ifndef ANDROID
    ssaoEffect = std::make_unique<SSAOEffect>();
    if (!ssaoEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize SSAO effect!\n");
        // Non-fatal: SSAO is optional
    }
    ssaoEffect->SetEnabled(false);  // Default to off — CameraSystem enables when needed
#endif

    // Initialize Directional Blur effect (applied after Gaussian blur).
    // There is no GLES directional-blur shader; keeping a dead effect object on
    // Android only causes failed resource work and pointless per-frame state.
#ifndef ANDROID
    directionalBlurEffect = std::make_unique<DirectionalBlurEffect>();
    if (!directionalBlurEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize Directional Blur effect!\n");
        // Non-fatal: directional blur is optional
    }
    directionalBlurEffect->SetEnabled(false);  // Default to off — CameraSystem enables when needed
#endif

    // Initialize Bloom effect (applied after blur, before HDR tonemapping)
    bloomEffect = std::make_unique<BloomEffect>();
    if (!bloomEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize Bloom effect!\n");
        // Non-fatal: bloom is optional
    }
    bloomEffect->SetEnabled(false);  // Default to off — CameraSystem enables when needed

    // Allocate lazily in BeginHDRRender. Android renders below native
    // resolution, so allocating a native-size target here would immediately be
    // deleted and recreated on the first frame.
    initialized = true;
    ENGINE_PRINT("[PostProcessingManager] Initialized successfully\n");
    return true;
}

void PostProcessingManager::Shutdown()
{
    if (ssaoEffect)
    {
        ssaoEffect->Shutdown();
        ssaoEffect.reset();
    }

    if (blurEffect)
    {
        blurEffect->Shutdown();
        blurEffect.reset();
    }

    if (directionalBlurEffect)
    {
        directionalBlurEffect->Shutdown();
        directionalBlurEffect.reset();
    }

    if (bloomEffect)
    {
        bloomEffect->Shutdown();
        bloomEffect.reset();
    }

    if (hdrEffect)
    {
        hdrEffect->Shutdown();
        hdrEffect.reset();
    }

    // Delete HDR framebuffer
    DeleteHDRFramebuffer();

    // Delete screen quad
    DeleteScreenQuad();

    initialized = false;
    ENGINE_PRINT("[PostProcessingManager] Shutdown complete\n");
}

void PostProcessingManager::Process(unsigned int inputTexture, unsigned int outputFBO,
    int renderWidth, int renderHeight, int outputWidth, int outputHeight)
{
    PROFILE_FUNCTION();

    if (!initialized)
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Not initialized!\n");
        return;
    }

    if (outputWidth <= 0) outputWidth = renderWidth;
    if (outputHeight <= 0) outputHeight = renderHeight;

    // Current pipeline: SSAO -> Blur -> Directional Blur -> Bloom -> HDR tone mapping -> Output

    unsigned int currentInput = inputTexture;
    unsigned int currentOutput = outputFBO;
#ifdef ANDROID
    unsigned int resolvedBloomTexture = 0;
    float resolvedBloomIntensity = 0.0f;
#endif

    // Apply SSAO (generates half-res AO texture, consumed by HDR pass).
#ifndef ANDROID
    if (ssaoEffect && ssaoEffect->IsEnabled())
    {
        PROFILE_SCOPED("PostProcess::SSAO");
        ssaoEffect->SetDepthTexture(hdrDepthTexture);
        ssaoEffect->SetProjectionMatrix(currentProjection);
        ssaoEffect->SetInvProjectionMatrix(currentInvProjection);
        ssaoEffect->Apply(currentInput, hdrFramebuffer, renderWidth, renderHeight);
    }
#endif

    // Apply blur before tonemapping (modifies HDR framebuffer in-place)
    if (blurEffect && blurEffect->IsEnabled() && blurEffect->GetIntensity() > 0.01f)
    {
        PROFILE_SCOPED("PostProcess::Blur");
        blurEffect->Apply(currentInput, hdrFramebuffer, renderWidth, renderHeight);
        // hdrColorTexture now contains blurred image, currentInput still points to it
    }

    // Apply directional blur after Gaussian blur
    if (directionalBlurEffect && directionalBlurEffect->IsEnabled() && directionalBlurEffect->GetIntensity() > 0.01f)
    {
        PROFILE_SCOPED("PostProcess::DirectionalBlur");
        directionalBlurEffect->Apply(currentInput, hdrFramebuffer, renderWidth, renderHeight);
    }

    // Apply bloom using per-entity emission buffer (no threshold extraction)
    // Early-out: skip the entire bloom pipeline (downsample mip chain + upsample +
    // composite) if no entity on screen has bloom > 0 this frame. Saves ~3-5ms on
    // mobile when nothing is glowing (which is most of the time).
    if (bloomEffect && bloomEffect->IsEnabled() && bloomEffect->GetIntensity() > 0.01f &&
        GraphicsManager::GetInstance().HasBloomEmissionThisFrame())
    {
        PROFILE_SCOPED("PostProcess::Bloom");
        bloomEffect->SetBloomEmissionTexture(hdrBloomEmissionTexture);
        bloomEffect->Apply(currentInput, hdrFramebuffer, renderWidth, renderHeight);
#ifdef ANDROID
        resolvedBloomTexture = bloomEffect->GetResolvedBloomTexture();
        if (resolvedBloomTexture != 0) {
            resolvedBloomIntensity = bloomEffect->GetIntensity();
        }
#endif
    }

    // Apply HDR effect (shader will bypass tonemapping if disabled)
    if (hdrEffect)
    {
        PROFILE_SCOPED("PostProcess::HDR");
        // Android's compact shader intentionally omits these effects.
#ifdef ANDROID
        hdrEffect->SetBloomInput(
            resolvedBloomTexture, resolvedBloomIntensity);
#else
        hdrEffect->SetVignetteEnabled(vignetteEnabled);
        hdrEffect->SetVignetteIntensity(vignetteIntensity_);
        hdrEffect->SetVignetteSmoothness(vignetteSmoothness_);
        hdrEffect->SetVignetteColor(vignetteColor_);
        hdrEffect->SetColorGradingEnabled(colorGradingEnabled);
        hdrEffect->SetCGBrightness(cgBrightness_);
        hdrEffect->SetCGContrast(cgContrast_);
        hdrEffect->SetCGSaturation(cgSaturation_);
        hdrEffect->SetCGTint(cgTint_);
        hdrEffect->SetChromaticAberrationEnabled(caEnabled_);
        hdrEffect->SetChromaticAberrationIntensity(caIntensity_);
        hdrEffect->SetChromaticAberrationPadding(caPadding_);

        // Pass SSAO texture to HDR effect
        if (ssaoEffect && ssaoEffect->IsEnabled())
        {
            hdrEffect->SetSSAOEnabled(true);
            hdrEffect->SetSSAOTexture(ssaoEffect->GetSSAOTexture());
        }
        else
        {
            hdrEffect->SetSSAOEnabled(false);
            hdrEffect->SetSSAOTexture(0);
        }
#endif

        // Apply the effect (it binds and fully overwrites the output).
        hdrEffect->Apply(currentInput, currentOutput, outputWidth, outputHeight);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CheckGLError("After Process");
}

unsigned int PostProcessingManager::CreateHDRFramebuffer(int width, int height)
{
    //std::cout << "[PostProcessingManager] Reallocating HDR Framebuffer: " << width << " x " << height << std::endl;
    // Delete existing framebuffer if it exists
    if (hdrFramebuffer != 0) 
    {
        DeleteHDRFramebuffer();
    }

    hdrWidth = width;
    hdrHeight = height;

    // Generate HDR framebuffer
    glGenFramebuffers(1, &hdrFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer);

    // Create HDR color texture. Android uses packed HDR because the final
    // scene target does not need alpha; this halves color-buffer bandwidth.
    glGenTextures(1, &hdrColorTexture);
    glBindTexture(GL_TEXTURE_2D, hdrColorTexture);
#ifdef __ANDROID__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTexture, 0);

    // Create bloom emission texture (MRT attachment 1)
    // Android uses R11F_G11F_B10F (4 bytes/pixel) instead of RGBA16F (8 bytes/pixel)
    // to halve memory bandwidth. Bloom doesn't need alpha and 10-11 bit precision
    // is plenty for glow/emission values.
    glGenTextures(1, &hdrBloomEmissionTexture);
    glBindTexture(GL_TEXTURE_2D, hdrBloomEmissionTexture);
#ifdef __ANDROID__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, hdrBloomEmissionTexture, 0);

    // Default: draw to attachment 0 only (bloom MRT enabled on demand)
    // glDrawBuffer is desktop OpenGL only; glDrawBuffers is the ES 3.0 equivalent
#ifdef ANDROID
    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);
#else
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif

    // Desktop fog/SSAO sample scene depth. Android disables both consumers, so
    // use an attachment-only renderbuffer; mobile tile drivers can keep it
    // transient and discard it without backing a full sampled texture.
#ifdef __ANDROID__
    glGenRenderbuffers(1, &hdrDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthRenderbuffer);
    glRenderbufferStorage(
        GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        hdrDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
#else
    glGenTextures(1, &hdrDepthTexture);
    glBindTexture(GL_TEXTURE_2D, hdrDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, hdrDepthTexture, 0);
#endif

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] HDR Framebuffer not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return hdrFramebuffer;
}

void PostProcessingManager::DeleteHDRFramebuffer()
{
    if (hdrColorTexture != 0)
    {
        glDeleteTextures(1, &hdrColorTexture);
        hdrColorTexture = 0;
    }
    if (hdrBloomEmissionTexture != 0)
    {
        glDeleteTextures(1, &hdrBloomEmissionTexture);
        hdrBloomEmissionTexture = 0;
    }
    if (hdrDepthTexture != 0)
    {
        glDeleteTextures(1, &hdrDepthTexture);
        hdrDepthTexture = 0;
    }
    if (hdrDepthRenderbuffer != 0)
    {
        glDeleteRenderbuffers(1, &hdrDepthRenderbuffer);
        hdrDepthRenderbuffer = 0;
    }
    if (hdrFramebuffer != 0)
    {
        glDeleteFramebuffers(1, &hdrFramebuffer);
        hdrFramebuffer = 0;
    }
}

void PostProcessingManager::BeginHDRRender(int width, int height)
{
    PROFILE_FUNCTION();

    // Create or resize HDR framebuffer if needed
    if (hdrFramebuffer == 0 || width != hdrWidth || height != hdrHeight)
    {
        CreateHDRFramebuffer(width, height);
    }

    // Bind HDR framebuffer for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer);
    glViewport(0, 0, width, height);

    // Start with the main color attachment only. The bloom attachment is
    // cleared and enabled later only if this frame actually emits bloom.
#ifdef ANDROID
    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);
#else
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif
}

void PostProcessingManager::EndHDRRender(unsigned int outputFBO, int renderWidth, int renderHeight,
    int outputWidth, int outputHeight)
{
    PROFILE_FUNCTION();

#ifdef ANDROID
    // No Android post-process consumes scene depth. Avoid storing the tile
    // depth attachment back to memory; the next frame clears it before use.
    const GLenum depthAttachment = GL_DEPTH_ATTACHMENT;
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &depthAttachment);
#endif

    // Unbind HDR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Apply post-processing effects (HDR tone mapping, etc.)
    Process(hdrColorTexture, outputFBO, renderWidth, renderHeight, outputWidth, outputHeight);
}

void PostProcessingManager::PrepareBloomMRT()
{
    // Clear emission only on frames that will consume it. Keep attachment 1
    // disabled afterwards so ordinary geometry does not pay a second
    // full-resolution color write just because one object glows.
    const unsigned int bloomAttachment = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(1, &bloomAttachment);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    DisableBloomMRT();
}

void PostProcessingManager::EnableBloomMRT()
{
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
}

void PostProcessingManager::DisableBloomMRT()
{
#ifdef ANDROID
    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);
#else
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif
}

void PostProcessingManager::RenderScreenQuad()
{
    if (screenQuadVAO == 0) 
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Screen quad not initialized!\n");
        return;
    }

    VAO::BindID(screenQuadVAO);
    PROFILE_COUNT("GL::DrawCalls", 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessingManager::ResetRuntimeState()
{
    // Reset blur
    if (blurEffect) {
        blurEffect->SetIntensity(0.0f);
        blurEffect->SetRadius(2.0f);
        blurEffect->SetPasses(2);
    }

    // Reset bloom
    if (bloomEffect) {
        bloomEffect->SetEnabled(false);
        bloomEffect->SetThreshold(1.0f);
        bloomEffect->SetIntensity(1.0f);
        bloomEffect->SetScatter(0.5f);
    }

    // Reset vignette
    vignetteEnabled = false;
    vignetteIntensity_ = 0.5f;
    vignetteSmoothness_ = 0.5f;
    vignetteColor_ = glm::vec3(0.0f);

    // Reset color grading
    colorGradingEnabled = false;
    cgBrightness_ = 0.0f;
    cgContrast_ = 1.0f;
    cgSaturation_ = 1.0f;
    cgTint_ = glm::vec3(1.0f);

    // Reset directional blur
    if (directionalBlurEffect) {
        directionalBlurEffect->SetEnabled(false);
        directionalBlurEffect->SetIntensity(0.0f);
        directionalBlurEffect->SetStrength(5.0f);
        directionalBlurEffect->SetAngle(0.0f);
        directionalBlurEffect->SetSamples(8);
    }

    // Reset chromatic aberration
    caEnabled_ = false;
    caIntensity_ = 0.5f;
    caPadding_ = 0.5f;

    // Reset layer exclusion
    excludedLayerMask = 0;
}

void PostProcessingManager::CreateScreenQuad()
{
    if (screenQuadVAO != 0) 
    {
        return; // Already created
    }

    // Fullscreen quad vertices (NDC coordinates)
    // Format: position (x, y), texCoords (u, v)
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,  // Top-left
        -1.0f, -1.0f,  0.0f, 0.0f,  // Bottom-left
         1.0f, -1.0f,  1.0f, 0.0f,  // Bottom-right

        -1.0f,  1.0f,  0.0f, 1.0f,  // Top-left
         1.0f, -1.0f,  1.0f, 0.0f,  // Bottom-right
         1.0f,  1.0f,  1.0f, 1.0f   // Top-right
    };

    glGenVertexArrays(1, &screenQuadVAO);
    glGenBuffers(1, &screenQuadVBO);

    VAO::BindID(screenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // TexCoord attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    VAO::BindID(0);

    ENGINE_PRINT("[PostProcessingManager] Screen quad created\n");
}

void PostProcessingManager::DeleteScreenQuad()
{
    if (screenQuadVAO != 0) 
    {
        glDeleteVertexArrays(1, &screenQuadVAO);
        screenQuadVAO = 0;
    }

    if (screenQuadVBO != 0)
    {
        glDeleteBuffers(1, &screenQuadVBO);
        screenQuadVBO = 0;
    }
}
