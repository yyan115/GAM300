#include "pch.h"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Logging.hpp"
#include <WindowManager.hpp>

// Add to PostProcessingManager.cpp
void CheckGLError(const char* location) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[OpenGL Error] at ", location, ": ", err, "\n");
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

    // Initialize SSAO effect (applied before blur/bloom)
    ssaoEffect = std::make_unique<SSAOEffect>();
    if (!ssaoEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize SSAO effect!\n");
        // Non-fatal: SSAO is optional
    }
    ssaoEffect->SetEnabled(false);  // Default to off — CameraSystem enables when needed

    // Initialize Directional Blur effect (applied after Gaussian blur)
    directionalBlurEffect = std::make_unique<DirectionalBlurEffect>();
    if (!directionalBlurEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize Directional Blur effect!\n");
        // Non-fatal: directional blur is optional
    }
    directionalBlurEffect->SetEnabled(false);  // Default to off — CameraSystem enables when needed

    // Initialize Bloom effect (applied after blur, before HDR tonemapping)
    bloomEffect = std::make_unique<BloomEffect>();
    if (!bloomEffect->Initialize())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Failed to initialize Bloom effect!\n");
        // Non-fatal: bloom is optional
    }
    bloomEffect->SetEnabled(false);  // Default to off — CameraSystem enables when needed

    CreateHDRFramebuffer(RunTimeVar::window.width, RunTimeVar::window.height);
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

void PostProcessingManager::Process(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
{
    if (!initialized)
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] Not initialized!\n");
        return;
    }

    // Current pipeline: SSAO -> Blur -> Directional Blur -> Bloom -> HDR tone mapping -> Output

    unsigned int currentInput = inputTexture;
    unsigned int currentOutput = outputFBO;

    // Apply SSAO (generates half-res AO texture, consumed by HDR pass)
    if (ssaoEffect && ssaoEffect->IsEnabled())
    {
        ssaoEffect->SetDepthTexture(hdrDepthTexture);
        ssaoEffect->SetProjectionMatrix(currentProjection);
        ssaoEffect->SetInvProjectionMatrix(currentInvProjection);
        ssaoEffect->Apply(currentInput, hdrFramebuffer, width, height);
    }

    // Apply blur before tonemapping (modifies HDR framebuffer in-place)
    if (blurEffect && blurEffect->IsEnabled() && blurEffect->GetIntensity() > 0.01f)
    {
        blurEffect->Apply(currentInput, hdrFramebuffer, width, height);
        // hdrColorTexture now contains blurred image, currentInput still points to it
    }

    // Apply directional blur after Gaussian blur
    if (directionalBlurEffect && directionalBlurEffect->IsEnabled() && directionalBlurEffect->GetIntensity() > 0.01f)
    {
        directionalBlurEffect->Apply(currentInput, hdrFramebuffer, width, height);
    }

    // Apply bloom using per-entity emission buffer (no threshold extraction)
    if (bloomEffect && bloomEffect->IsEnabled() && bloomEffect->GetIntensity() > 0.01f)
    {
        bloomEffect->SetBloomEmissionTexture(hdrBloomEmissionTexture);
        bloomEffect->Apply(currentInput, hdrFramebuffer, width, height);
    }

    // Apply HDR effect (shader will bypass tonemapping if disabled)
    if (hdrEffect)
    {
        // Pass vignette/color grading settings to HDR effect
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

        // Bind output framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, currentOutput);
        glViewport(0, 0, width, height);

        // Disable depth testing
        glDisable(GL_DEPTH_TEST);

        // Clear output
        glClear(GL_COLOR_BUFFER_BIT);

        // Apply the effect (shader checks enableTonemapping uniform)
        hdrEffect->Apply(currentInput, currentOutput, width, height);

        // Re-enable depth testing
        glEnable(GL_DEPTH_TEST);
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

    // Create HDR color texture (16-bit floating point)
    glGenTextures(1, &hdrColorTexture);
    glBindTexture(GL_TEXTURE_2D, hdrColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTexture, 0);

    // Create bloom emission texture (MRT attachment 1)
    glGenTextures(1, &hdrBloomEmissionTexture);
    glBindTexture(GL_TEXTURE_2D, hdrBloomEmissionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
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

    // Create depth texture (sampleable, so fog can read scene depth for soft intersections)
    glGenTextures(1, &hdrDepthTexture);
    glBindTexture(GL_TEXTURE_2D, hdrDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, hdrDepthTexture, 0);

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

    // Clear bloom emission texture (attachment 1) to black
    // This is separate from the main color clear so the scene background color
    // doesn't leak into the bloom emission buffer
    GLenum bloomAttachment = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(1, &bloomAttachment);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Restore single draw buffer (attachment 0 only) for normal rendering
#ifdef ANDROID
    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);
#else
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif
}

void PostProcessingManager::EndHDRRender(unsigned int outputFBO, int width, int height)
{
    PROFILE_FUNCTION();

    // Unbind HDR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Apply post-processing effects (HDR tone mapping, etc.)
    Process(hdrColorTexture, outputFBO, width, height);
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

    glBindVertexArray(screenQuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6); 
    glBindVertexArray(0);
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

    glBindVertexArray(screenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // TexCoord attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

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

