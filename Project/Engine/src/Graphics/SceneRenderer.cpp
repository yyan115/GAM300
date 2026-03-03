#include "pch.h"
#include "Graphics/SceneRenderer.hpp"
#include "Graphics/Camera/Camera.h"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Engine.h"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Scene/SceneManager.hpp"
#include "Scene/SceneInstance.hpp"
#include "WindowManager.hpp"
#include <iostream>
#include "Logging.hpp"
#include <Graphics/PostProcessing/PostProcessingManager.hpp>
#include "Physics/PhysicsSystem.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Model/Model.h"

// Static member definitions for SCENE panel
unsigned int SceneRenderer::sceneFrameBuffer = 0;
unsigned int SceneRenderer::sceneColorTexture = 0;
unsigned int SceneRenderer::sceneDepthTexture = 0;
int SceneRenderer::sceneWidth = 1280;
int SceneRenderer::sceneHeight = 720;

// Static member definitions for GAME panel (separate)
unsigned int SceneRenderer::gameFrameBuffer = 0;
unsigned int SceneRenderer::gameColorTexture = 0;
unsigned int SceneRenderer::gameDepthTexture = 0;
int SceneRenderer::gameWidth = 1920;
int SceneRenderer::gameHeight = 1080;

Camera* SceneRenderer::editorCamera = nullptr;
std::shared_ptr<Shader> SceneRenderer::outlineShader = nullptr;
bool SceneRenderer::outlineShaderInitialized = false;

unsigned int SceneRenderer::CreateSceneFramebuffer(int width, int height)
{
    // Delete existing framebuffer if it exists
    if (sceneFrameBuffer != 0) {
        DeleteSceneFramebuffer();
    }

    sceneWidth = width;
    sceneHeight = height;

    // Generate framebuffer
    glGenFramebuffers(1, &sceneFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);

    // Create color texture
    glGenTextures(1, &sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);

    // Create depth-stencil texture (stencil needed for selection outline)
    glGenTextures(1, &sceneDepthTexture);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "SceneRenderer: Framebuffer not complete!\n");
    }

    // Check for OpenGL errors immediately
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SceneRenderer] OpenGL error: ", err, "\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ENGINE_PRINT("[SceneRenderer] Scene framebuffer created (", width, "x", height, ")\n");
    ENGINE_PRINT("[SceneRenderer] Scene FBO ID: ", sceneFrameBuffer, ", Scene Texture ID: ", sceneColorTexture, "\n");
    return sceneFrameBuffer;
}

void SceneRenderer::DeleteSceneFramebuffer()
{
    if (sceneColorTexture != 0) {
        glDeleteTextures(1, &sceneColorTexture);
        sceneColorTexture = 0;
    }
    if (sceneDepthTexture != 0) {
        glDeleteTextures(1, &sceneDepthTexture);
        sceneDepthTexture = 0;
    }
    if (sceneFrameBuffer != 0) {
        glDeleteFramebuffers(1, &sceneFrameBuffer);
        sceneFrameBuffer = 0;
    }

    // Clean up editor camera
    if (editorCamera) {
        delete editorCamera;
        editorCamera = nullptr;
        ENGINE_PRINT("[SceneRenderer] Editor camera deleted\n");
    }

    // Clean up outline shader
    outlineShader = nullptr;
    outlineShaderInitialized = false;
}

unsigned int SceneRenderer::GetSceneTexture()
{
    return sceneColorTexture;
}

unsigned int SceneRenderer::GetSceneFramebuffer()
{
    return sceneFrameBuffer;
}

void SceneRenderer::BeginSceneRender(int width, int height)
{
    // Create or resize framebuffer if needed
    if (sceneFrameBuffer == 0 || width != sceneWidth || height != sceneHeight) {
        ENGINE_PRINT("[SceneRenderer] Calling CreateSceneFramebuffer from BeginSceneRender\n");
        CreateSceneFramebuffer(width, height);
    }

    // Update WindowManager viewport dimensions to match scene rendering area
    WindowManager::SetViewportDimensions(width, height);

    // Update GraphicsManager viewport for correct frustum culling
    GraphicsManager::GetInstance().SetViewportSize(width, height);

    // Bind framebuffer and set viewport
    //glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);
    //glViewport(0, 0, width, height);
    //PostProcessingManager::GetInstance().BeginHDRRender(width, height);

    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::EndSceneRender()
{
    // Unbind framebuffer (render to screen again)
    PostProcessingManager::GetInstance().EndHDRRender(sceneFrameBuffer, sceneWidth, sceneHeight);

    // Render deferred items (excluded from post-processing) on top of blurred output
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();
    if (gfxManager.HasDeferredItems())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);
        glViewport(0, 0, sceneWidth, sceneHeight);
        gfxManager.RenderDeferred();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::RenderScene()
{
    try {
        Engine::Draw();
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Exception in SceneRenderer::RenderScene: ", e.what(), "\n");
    }
}

void SceneRenderer::RenderSceneForEditor()
{
    // Use default camera parameters for the original function
    RenderSceneForEditor(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f);
}

void SceneRenderer::RenderSceneForEditor(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::vec3& cameraUp, float cameraZoom, float orthoZoomLevel)
{
    PostProcessingManager::GetInstance().BeginHDRRender(sceneWidth, sceneHeight);

    try {
        // Initialize static editor camera if not already done
        if (!editorCamera) {
            editorCamera = new Camera(glm::vec3(0.0f, 0.0f, 3.0f));
        }

        // Get the ECS manager and graphics manager
        ECSManager& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();
        GraphicsManager& gfxManager = GraphicsManager::GetInstance();

        // Update the static camera with the provided parameters
        editorCamera->Position = cameraPos;
        editorCamera->Front = cameraFront;
        editorCamera->Up = cameraUp;
        editorCamera->Zoom = cameraZoom;
        editorCamera->OrthoZoomLevel = orthoZoomLevel;

        // Mark that we're rendering for the editor (for view mode filtering)
        gfxManager.SetRenderingForEditor(true);

        // Update UI anchors before transform (sets local positions based on viewport)
        if (mainECS.uiAnchorSystem)
        {
            mainECS.uiAnchorSystem->Update();
        }
        mainECS.transformSystem->Update();

		// ONLY CALLED VIA EDITOR - Update physics in editor mode
        mainECS.physicsSystem->EditorUpdate(mainECS);

        // Set the static editor camera (this won't be updated by input)
        gfxManager.SetCamera(editorCamera);

        // Update frustum with editor camera for correct culling
        gfxManager.UpdateFrustum();

        // Begin frame and clear (without input processing)
        gfxManager.BeginFrame();

        Entity activeCam = mainECS.cameraSystem ? mainECS.cameraSystem->GetActiveCameraEntity() : UINT32_MAX;
        if (activeCam != UINT32_MAX && mainECS.HasComponent<CameraComponent>(activeCam)) {
            auto& camComp = mainECS.GetComponent<CameraComponent>(activeCam);
            gfxManager.Clear(camComp.backgroundColor.r, camComp.backgroundColor.g, camComp.backgroundColor.b, 1.0f);

            // Apply camera post-processing settings in editor mode
            // (CameraSystem::Update only runs during play mode)
            auto& ppManager = PostProcessingManager::GetInstance();
            BlurEffect* blur = ppManager.GetBlurEffect();
            if (blur) {
                if (camComp.blurEnabled) {
                    blur->SetIntensity(camComp.blurIntensity);
                    blur->SetRadius(camComp.blurRadius);
                    blur->SetPasses(camComp.blurPasses);
                } else {
                    blur->SetIntensity(0.0f);
                }
            }
            if (camComp.blurEnabled)
                ppManager.SetExcludedLayerMask(~camComp.blurLayerMask);
            else
                ppManager.SetExcludedLayerMask(0);
            BloomEffect* bloom = ppManager.GetBloomEffect();
            if (bloom) {
                if (camComp.bloomEnabled) {
                    bloom->SetEnabled(true);
                    bloom->SetThreshold(camComp.bloomThreshold);
                    bloom->SetIntensity(camComp.bloomIntensity);
                } else {
                    bloom->SetEnabled(false);
                }
            }
            ppManager.SetVignetteEnabled(camComp.vignetteEnabled);
            ppManager.SetVignetteIntensity(camComp.vignetteIntensity);
            ppManager.SetVignetteSmoothness(camComp.vignetteSmoothness);
            ppManager.SetVignetteColor(camComp.vignetteColor);
            ppManager.SetColorGradingEnabled(camComp.colorGradingEnabled);
            ppManager.SetCGBrightness(camComp.cgBrightness);
            ppManager.SetCGContrast(camComp.cgContrast);
            ppManager.SetCGSaturation(camComp.cgSaturation);
            ppManager.SetCGTint(camComp.cgTint);
            ppManager.SetChromaticAberrationEnabled(camComp.chromaticAberrationEnabled);
            ppManager.SetChromaticAberrationIntensity(camComp.chromaticAberrationIntensity);
            ppManager.SetChromaticAberrationPadding(camComp.chromaticAberrationPadding);
        } else {
            gfxManager.Clear(0.192f, 0.301f, 0.475f, 1.0f);
        }

        // Update model system for rendering (without input-based updates)
        if (mainECS.modelSystem) 
        {
            mainECS.modelSystem->Update();
        }
		if (mainECS.textSystem)
		{
			mainECS.textSystem->Update();
		}
        if (mainECS.spriteSystem)
        {
            mainECS.spriteSystem->Update();
        }
        if (mainECS.lightingSystem)
        {
            mainECS.lightingSystem->Update();
        }
        if (mainECS.particleSystem)
        {
            mainECS.particleSystem->Update();
        }
        if (mainECS.fogSystem)
        {
            mainECS.fogSystem->Update();
        }

        // Render the scene
        gfxManager.Render();

        // End frame
        gfxManager.EndFrame();

        // Reset editor rendering flag
        gfxManager.SetRenderingForEditor(false);

		// Set all isDirty flags to false after rendering
        mainECS.transformSystem->PostUpdate();

    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Exception in SceneRenderer::RenderSceneForEditor: ", e.what(), "\n");
    }
}

// ============================================================================
// GAME PANEL SEPARATE FRAMEBUFFER FUNCTIONS
// ============================================================================

void SceneRenderer::BeginGameRender(int width, int height)
{
    GraphicsManager::GetInstance().SetGamePanelActive(true);

    // Create or resize game framebuffer if needed
    if (gameFrameBuffer == 0 || width != gameWidth || height != gameHeight) {
        // Delete existing game framebuffer if it exists
        if (gameFrameBuffer != 0) {
            glDeleteFramebuffers(1, &gameFrameBuffer);
            glDeleteTextures(1, &gameColorTexture);
            glDeleteTextures(1, &gameDepthTexture);
        }

        gameWidth = width;
        gameHeight = height;

        // Create framebuffer
        glGenFramebuffers(1, &gameFrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, gameFrameBuffer);

        // Create color texture
        glGenTextures(1, &gameColorTexture);
        glBindTexture(GL_TEXTURE_2D, gameColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameColorTexture, 0);

        // Create depth texture
        glGenTextures(1, &gameDepthTexture);
        glBindTexture(GL_TEXTURE_2D, gameDepthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gameDepthTexture, 0);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SceneRenderer] Game framebuffer is not complete!\n");
        }
    }

    // Update WindowManager viewport dimensions to match game rendering area
    WindowManager::SetViewportDimensions(width, height);

    // Update GraphicsManager viewport for correct frustum culling
    GraphicsManager::GetInstance().SetViewportSize(width, height);

    // Bind game framebuffer and set viewport
    /*glBindFramebuffer(GL_FRAMEBUFFER, gameFrameBuffer);
    glViewport(0, 0, width, height);*/
    //PostProcessingManager::GetInstance().BeginHDRRender(width, height);

    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::EndGameRender()
{
    // Prevent double blur: SceneInstance::Draw already applied blur in its own
    // EndHDRRender pass. Running blur again here with different dimensions would
    // partially overwrite hdrColorTexture, causing a double image artifact.
    BlurEffect* blur = PostProcessingManager::GetInstance().GetBlurEffect();
    if (blur) blur->SetIntensity(0.0f);

    // Prevent double bloom: same issue as blur — SceneInstance::Draw already applied
    // bloom in its first EndHDRRender pass. Save/restore since bloom intensity is persistent.
    BloomEffect* bloom = PostProcessingManager::GetInstance().GetBloomEffect();
    float savedBloomIntensity = bloom ? bloom->GetIntensity() : 0.0f;
    if (bloom) bloom->SetIntensity(0.0f);

    // NOTE: Chromatic aberration, vignette, and color grading are NOT zeroed here.
    // They are tonemapping shader effects that sample hdrColorTexture read-only,
    // so they don't cause double-application. They must stay active because this
    // second EndHDRRender pass is what writes to the game framebuffer.

    PostProcessingManager::GetInstance().EndHDRRender(gameFrameBuffer, gameWidth, gameHeight);

    // Restore bloom intensity for next frame
    if (bloom) bloom->SetIntensity(savedBloomIntensity);

    // Render deferred items (excluded from post-processing) on top of blurred output
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();
    if (gfxManager.HasDeferredItems())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gameFrameBuffer);
        glViewport(0, 0, gameWidth, gameHeight);
        gfxManager.RenderDeferred();
    }

    // Unbind framebuffer (render to screen again)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gfxManager.SetGamePanelActive(false);
}

unsigned int SceneRenderer::GetGameTexture()
{
    return gameColorTexture;
}

// ============================================================================
// SELECTION OUTLINE (Stencil-based)
// ============================================================================

void SceneRenderer::InitOutlineShader()
{
    if (outlineShaderInitialized) return;
    outlineShaderInitialized = true;

    outlineShader = std::make_shared<Shader>();
    std::string shaderPath = "Resources/Shaders/selection_outline.shader";
    if (!outlineShader->LoadResource(shaderPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SceneRenderer] Failed to load selection_outline shader\n");
        outlineShader = nullptr;
    }
}

void SceneRenderer::RenderSelectionOutline(Model* model,
                                            const glm::mat4& modelMatrix,
                                            const glm::mat4& view,
                                            const glm::mat4& proj,
                                            bool isAnimated,
                                            const std::vector<glm::mat4>& boneMatrices,
                                            float outlineThickness)
{
    try {
        if (!model) return;

        // Lazy-init the outline shader
        InitOutlineShader();
        if (!outlineShader) return;
        if (sceneFrameBuffer == 0) return;

        // Save GL state
        GLboolean depthTestEnabled, depthWriteEnabled, stencilTestEnabled, cullFaceEnabled, blendEnabled;
        GLint prevStencilFunc, prevStencilRef, prevStencilMask;
        GLint prevStencilFail, prevStencilZFail, prevStencilZPass;
        GLboolean colorMask[4];
        glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);
        glGetBooleanv(GL_STENCIL_TEST, &stencilTestEnabled);
        glGetBooleanv(GL_CULL_FACE, &cullFaceEnabled);
        glGetBooleanv(GL_BLEND, &blendEnabled);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        glGetIntegerv(GL_STENCIL_FUNC, &prevStencilFunc);
        glGetIntegerv(GL_STENCIL_REF, &prevStencilRef);
        glGetIntegerv(GL_STENCIL_VALUE_MASK, &prevStencilMask);
        glGetIntegerv(GL_STENCIL_FAIL, &prevStencilFail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &prevStencilZFail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &prevStencilZPass);

        // Bind the scene FBO
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);
        glViewport(0, 0, sceneWidth, sceneHeight);

        // --- Pass 1: Stencil fill - mark mesh pixels with 1 ---
        glEnable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);  // Depth buffer has stale values from tone mapping; disable so all fragments pass
        glClearStencil(0);
        glStencilMask(0xFF);
        glClear(GL_STENCIL_BUFFER_BIT);

        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);

        outlineShader->Activate();
        outlineShader->setMat4("model", modelMatrix);
        outlineShader->setMat4("view", view);
        outlineShader->setMat4("projection", proj);
        outlineShader->setFloat("outlineThickness", 0.0f);
        outlineShader->setBool("isAnimated", isAnimated);

        if (isAnimated) {
            for (size_t i = 0; i < boneMatrices.size(); ++i) {
                outlineShader->setMat4("finalBonesMatrices[" + std::to_string(i) + "]", boneMatrices[i]);
            }
        }

        model->DrawDepthOnly();

        // --- Pass 2: Outline ---
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0x00);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        outlineShader->Activate();
        outlineShader->setMat4("model", modelMatrix);
        outlineShader->setMat4("view", view);
        outlineShader->setMat4("projection", proj);
        outlineShader->setFloat("outlineThickness", outlineThickness);
        outlineShader->setVec4("outlineColor", glm::vec4(1.0f, 0.647f, 0.0f, 1.0f));
        outlineShader->setBool("isAnimated", isAnimated);

        if (isAnimated) {
            for (size_t i = 0; i < boneMatrices.size(); ++i) {
                outlineShader->setMat4("finalBonesMatrices[" + std::to_string(i) + "]", boneMatrices[i]);
            }
        }

        model->DrawDepthOnly();

        // --- Restore GL state ---
        glStencilMask(0xFF);
        glDisable(GL_STENCIL_TEST);

        if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(depthWriteEnabled);
        if (stencilTestEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        if (cullFaceEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        glStencilFunc(prevStencilFunc, prevStencilRef, prevStencilMask);
        glStencilOp(prevStencilFail, prevStencilZFail, prevStencilZPass);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SceneRenderer] Error in RenderSelectionOutline: ", e.what(), "\n");
    }
}