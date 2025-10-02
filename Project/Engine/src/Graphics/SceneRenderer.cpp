#include "pch.h"
#include "Graphics/SceneRenderer.hpp"
#include "Graphics/Camera.h"
#include "Engine.h"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Scene/SceneManager.hpp"
#include "Scene/SceneInstance.hpp"
#include "WindowManager.hpp"
#include <iostream>
#include "Logging.hpp"

// Static member definitions
unsigned int SceneRenderer::sceneFrameBuffer = 0;
unsigned int SceneRenderer::sceneColorTexture = 0;
unsigned int SceneRenderer::sceneDepthTexture = 0;
int SceneRenderer::sceneWidth = 1280;
int SceneRenderer::sceneHeight = 720;
Camera* SceneRenderer::editorCamera = nullptr;

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

    // Create depth texture
    glGenTextures(1, &sceneDepthTexture);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTexture, 0);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "SceneRenderer: Framebuffer not complete!\n");
        //std::cerr << "SceneRenderer: Framebuffer not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
        //std::cout << "[SceneRenderer] Editor camera deleted" << std::endl;
    }
}

unsigned int SceneRenderer::GetSceneTexture()
{
    return sceneColorTexture;
}

void SceneRenderer::BeginSceneRender(int width, int height)
{
    // Create or resize framebuffer if needed
    if (sceneFrameBuffer == 0 || width != sceneWidth || height != sceneHeight) {
        CreateSceneFramebuffer(width, height);
    }

    // Update WindowManager viewport dimensions to match scene rendering area
    WindowManager::SetViewportDimensions(width, height);

    // Bind framebuffer and set viewport
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);
    glViewport(0, 0, width, height);

    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::EndSceneRender()
{
    // Unbind framebuffer (render to screen again)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::RenderScene()
{
    try {
        Engine::Draw();
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Exception in SceneRenderer::RenderScene: ", e.what(), "\n");
        //std::cerr << "Exception in SceneRenderer::RenderScene: " << e.what() << std::endl;
    }
}

void SceneRenderer::RenderSceneForEditor()
{
    // Use default camera parameters for the original function
    RenderSceneForEditor(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f);
}

void SceneRenderer::RenderSceneForEditor(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::vec3& cameraUp, float cameraZoom)
{
    try {
        // Initialize static editor camera if not already done
        if (!editorCamera) {
            editorCamera = new Camera(glm::vec3(0.0f, 0.0f, 3.0f));
        }

        // Update the static camera with the provided parameters
        editorCamera->Position = cameraPos;
        editorCamera->Front = cameraFront;
        editorCamera->Up = cameraUp;
        editorCamera->Zoom = cameraZoom;

        // Get the ECS manager and graphics manager
        ECSManager& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();
        GraphicsManager& gfxManager = GraphicsManager::GetInstance();

        mainECS.transformSystem->Update();

        // Set the static editor camera (this won't be updated by input)
        gfxManager.SetCamera(editorCamera);

        // Begin frame and clear (without input processing)
        gfxManager.BeginFrame();
        gfxManager.Clear();

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

        // Render the scene
        gfxManager.Render();

        // End frame
        gfxManager.EndFrame();

    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Exception in SceneRenderer::RenderSceneForEditor: ", e.what(), "\n");
        //std::cerr << "Exception in SceneRenderer::RenderSceneForEditor: " << e.what() << std::endl;
    }
}