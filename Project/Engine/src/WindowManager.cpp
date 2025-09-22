#include "pch.h"

#include "Graphics/OpenGL.h"
#include "Platform/IPlatform.h"

#include "WindowManager.hpp"
#include "Engine.h"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"

#define UNREFERENCED_PARAMETER(P) (P)

IPlatform* WindowManager::platform = nullptr;
PlatformWindow WindowManager::ptrWindow = nullptr;
GLint WindowManager::width;
GLint WindowManager::height;
GLint WindowManager::viewportWidth;
GLint WindowManager::viewportHeight;
const char* WindowManager::title;

bool WindowManager::isFocused = true;
bool WindowManager::isFullscreen = false;
GLint WindowManager::windowedWidth = 1600;   // Default windowed size
GLint WindowManager::windowedHeight = 900;  // Default windowed size
GLint WindowManager::windowedPosX = 0;      // Default window position
GLint WindowManager::windowedPosY = 0;      // Default window position

double WindowManager::deltaTime = 0.0;
double WindowManager::lastFrameTime = 0.0;


bool WindowManager::Initialize(GLint _width, GLint _height, const char* _title) {
    WindowManager::width = _width;
    WindowManager::height = _height;
    WindowManager::viewportWidth = _width;
    WindowManager::viewportHeight = _height;
    title = _title;

    windowedWidth = _width;
    windowedHeight = _height;

    // Create platform instance
    platform = CreatePlatform();
    if (!platform) {
        std::cout << "Platform creation failed - abort program!!!" << std::endl;
        return false;
    }

    // Initialize platform window
    if (!platform->InitializeWindow(_width, _height, _title)) {
        std::cout << "Platform window initialization failed - abort program!!!" << std::endl;
        return false;
    }

    // Initialize graphics context
    if (!platform->InitializeGraphics()) {
        std::cout << "Platform graphics initialization failed - abort program!!!" << std::endl;
        return false;
    }

#ifndef ANDROID
    // Make context current (Android will do this later when surface is set)
    platform->MakeContextCurrent();
#endif

    // Get platform window handle for compatibility
    ptrWindow = static_cast<PlatformWindow>(platform->GetNativeWindow());

#ifndef ANDROID
    // Desktop: Initialize GLAD (Android uses OpenGL ES directly)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }
#endif
    
    // Enable depth testing and set viewport
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, _width, _height);

    return true;
}

void WindowManager::ToggleFullscreen() {
    if (platform) {
        platform->ToggleFullscreen();
        isFullscreen = !isFullscreen; // Toggle fullscreen state
    }
}

void WindowManager::MinimizeWindow() {
    if (platform) {
        platform->MinimizeWindow();
    }
}

void WindowManager::UpdateViewportDimensions() {
    //if (DuckEngine::isEditor) {
    //    Vec2 newViewportSize = DuckEngine::editorContentRegion;
    //    viewportWidth = static_cast<GLint>(newViewportSize.x);
    //    viewportHeight = static_cast<GLint>(newViewportSize.y);
    //}
    //else {
    //    // Fallback to window dimensions if ImGui is not initialized
    //    viewportWidth = width;
    //    viewportHeight = height;
    //}

    // std::cout << "viewport w h: " << viewportWidth << ", " << viewportHeight << "\n";
}

PlatformWindow WindowManager::getWindow() {
    return ptrWindow;
}

void WindowManager::SetWindowShouldClose()
{
    if (platform) {
        platform->SetShouldClose(true);
    }
}

bool WindowManager::ShouldClose() {
    if (platform) {
        return platform->ShouldClose();
    }
    return false;
}

void WindowManager::Exit() {
    if (platform) {
        platform->DestroyWindow();
        delete platform;
        platform = nullptr;
    }
}

void WindowManager::error_cb(int error, char const* description) {
#ifdef _DEBUG
    std::cerr << "GLFW error: " << description << ", " << error << std::endl;
#else
    (void)error;        // Avoid unused parameter warning
    (void)description;  // Avoid unused parameter warning
#endif
}

void WindowManager::fbsize_cb(PlatformWindow ptr_win, int _width, int _height) {
    UNREFERENCED_PARAMETER(ptr_win);

#ifdef _DEBUG
    std::cout << "fbsize_cb getting called!!!" << std::endl;
#endif
    WindowManager::width = _width;
    WindowManager::height = _height;

    glViewport(0, 0, _width, _height);

    // Call GraphicsManager to update UI positions based on new window size
    //GraphicsManager::OnWindowResize(_width, _height);
}

GLint WindowManager::GetWindowWidth()
{
    return width;
}

GLint WindowManager::GetWindowHeight()
{
    return height;
}

GLint WindowManager::GetViewportWidth()
{
    //std::cout << "viewportW: " << viewportWidth << ", normalW: " << width << "\n";
    return viewportWidth;
}

GLint WindowManager::GetViewportHeight()
{
    return viewportHeight;
}

void WindowManager::SetWindowTitle(const char* _title) {
    if (platform) {
        platform->SetWindowTitle(_title);
    }
}

void WindowManager::window_focus_callback(PlatformWindow window, int focused) {

    //if (!focused && !isFullscreen && !DuckEngine::isEditor)  glfwIconifyWindow(ptrWindow);  // Minimizes the window

    //UNREFERENCED_PARAMETER(window);
    //isFocused = focused != 0;
}

bool WindowManager::IsWindowMinimized() {
    if (platform) {
        return platform->IsWindowMinimized();
    }
    return false;
}

bool WindowManager::IsWindowFocused() {
    return isFocused;
}

void WindowManager::updateDeltaTime() {
    const double targetDeltaTime = 1.0 / 60.0; // cap at 60fps

    double currentTime = platform ? platform->GetTime() : 0.0;
    double frameTime = currentTime - lastFrameTime;

    double remainingTime = targetDeltaTime - frameTime;

    //Limit to 60 FPS?
    //// Sleep only if we have at least 5 ms remaining
    //if (remainingTime > 0.005) {
    //    std::this_thread::sleep_for(std::chrono::milliseconds((int)((remainingTime - 0.001) * 1000)));
    //}
    //// Busy-wait the last few milliseconds - now handled by platform
    //while ((platform->GetTime() - lastFrameTime) < targetDeltaTime) {}

    // Update deltaTime
    currentTime = platform ? platform->GetTime() : 0.0;
    deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    // Swap interval handled by platform internally
}

double WindowManager::getDeltaTime() {
    return deltaTime;
}
double WindowManager::getFps() {
    return deltaTime > 0.0 ? 1.0 / deltaTime : 0.0;
}

// Scene framebuffer functions
unsigned int WindowManager::CreateSceneFramebuffer(int width, int height) 
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
        std::cerr << "WindowManager: Framebuffer not complete!" << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return sceneFrameBuffer;
}

void WindowManager::DeleteSceneFramebuffer() 
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
        std::cout << "[WindowManager] Editor camera deleted" << std::endl;
    }
}

unsigned int WindowManager::GetSceneTexture() 
{
    return sceneColorTexture;
}

void WindowManager::BeginSceneRender(int width, int height) 
{
    // Create or resize framebuffer if needed
    if (sceneFrameBuffer == 0 || width != ::sceneWidth || height != ::sceneHeight) {
        CreateSceneFramebuffer(width, height);
    }
    
    // Bind framebuffer and set viewport
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFrameBuffer);
    glViewport(0, 0, width, height);
    
    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
    
    
}

void WindowManager::EndSceneRender() 
{
    // Unbind framebuffer (render to screen again)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void WindowManager::RenderScene() 
{
    try {
        Engine::Draw();
    } catch (const std::exception& e) {
        std::cerr << "Exception in RenderScene: " << e.what() << std::endl;
    }
}

void WindowManager::RenderSceneForEditor() 
{
    // Use default camera parameters for the original function
    RenderSceneForEditor(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f);
}

void WindowManager::RenderSceneForEditor(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::vec3& cameraUp, float cameraZoom)
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
        ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager("TestScene");
        GraphicsManager& gfxManager = GraphicsManager::GetInstance();
        
        // Set the static editor camera (this won't be updated by input)
        gfxManager.SetCamera(editorCamera);
        
        // Begin frame and clear (without input processing)
        gfxManager.BeginFrame();
        gfxManager.Clear();
        
        // Update model system for rendering (without input-based updates)
        if (mainECS.modelSystem) {
            mainECS.modelSystem->Update();
        }
        
        // Render the scene
        gfxManager.Render();
        
        // Draw light cubes using the static editor camera (not the game camera)
        SceneInstance* currentScene = static_cast<SceneInstance*>(SceneManager::GetInstance().GetCurrentScene());
        if (currentScene) {
            currentScene->DrawLightCubes(*editorCamera);
        }
        
        // End frame
        gfxManager.EndFrame();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in RenderSceneForEditor: " << e.what() << std::endl;
    }
}

// Platform abstraction methods
void WindowManager::SwapBuffers() {
    if (platform) {
        platform->SwapBuffers();
    }
}

void WindowManager::PollEvents() {
    if (platform) {
        platform->PollEvents();
    }
}

IPlatform* WindowManager::GetPlatform() {
    return platform;
}