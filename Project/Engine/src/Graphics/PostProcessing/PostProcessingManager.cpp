#include "pch.h"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Logging.hpp"
#include <glad/glad.h>
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

    // Future effects will be initialized here
    // bloomEffect = std::make_unique<BloomEffect>();
    // if (!bloomEffect->Initialize()) { return false; }
    CreateHDRFramebuffer(RunTimeVar::window.width, RunTimeVar::window.height);
    initialized = true;
    ENGINE_PRINT("[PostProcessingManager] Initialized successfully\n");
    return true;
}

void PostProcessingManager::Shutdown()
{
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

    static int count = 0;
    if (count++ % 60 == 0) 
    {
        ENGINE_PRINT("[Process] Input texture: ", inputTexture,
            " Output FBO: ", outputFBO,
            " HDR texture: ", hdrColorTexture, "\n");
    }

    // Current pipeline: HDR tone mapping only
    // Future pipeline: Bloom -> HDR -> Color Grading -> Output

    unsigned int currentInput = inputTexture;
    unsigned int currentOutput = outputFBO;

    // Future: Apply bloom before tone mapping (extracts and blurs bright areas)
    // if (bloomEffect && bloomEffect->IsEnabled()) {
    //     // Bloom needs to output to an intermediate buffer
    //     // Then that becomes input to HDR
    //     bloomEffect->Apply(currentInput, bloomIntermediateBuffer, width, height);
    //     currentInput = bloomIntermediateBuffer;
    // }

    // Apply HDR effect (shader will bypass tonemapping if disabled)
    if (hdrEffect)
    {
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

    // Create depth renderbuffer
    glGenRenderbuffers(1, &hdrDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hdrDepthRenderbuffer);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PostProcessingManager] HDR Framebuffer not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ENGINE_PRINT("[PostProcessingManager] HDR framebuffer created (", width, "x", height, ")\n");
    ENGINE_PRINT("[PostProcessingManager] HDR FBO ID: ", hdrFramebuffer, ", HDR Texture ID: ", hdrColorTexture, "\n");
    return hdrFramebuffer;
}

void PostProcessingManager::DeleteHDRFramebuffer()
{
    if (hdrColorTexture != 0) 
    {
        glDeleteTextures(1, &hdrColorTexture);
        hdrColorTexture = 0;
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
    // Create or resize HDR framebuffer if needed
    if (hdrFramebuffer == 0 || width != hdrWidth || height != hdrHeight) 
    {
        CreateHDRFramebuffer(width, height);
    }

    // Bind HDR framebuffer for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer);
    glViewport(0, 0, width, height);

    // Clear HDR buffer
    // Clear HDR buffer with BLACK (very important!)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessingManager::EndHDRRender(unsigned int outputFBO, int width, int height)
{
    // Unbind HDR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Debug: Check if tone mapping will run
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        ENGINE_PRINT("[EndHDRRender] Processing HDR texture: ", hdrColorTexture,
            " to output FBO: ", outputFBO, "\n");
        if (hdrEffect) {
            ENGINE_PRINT("[EndHDRRender] HDR Effect - Enabled: ", hdrEffect->IsEnabled(),
                " Exposure: ", hdrEffect->GetExposure(),
                " Gamma: ", hdrEffect->GetGamma(), "\n");
        }
    }

    // Apply post-processing effects (HDR tone mapping, etc.)
    Process(hdrColorTexture, outputFBO, width, height);
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

