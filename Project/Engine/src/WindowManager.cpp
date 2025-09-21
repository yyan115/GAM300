#include "pch.h"

#include <glad/glad.h>

#include "WindowManager.hpp"
#include "Engine.h"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/Camera.h"
#include "Scene/SceneManager.hpp"
#include "Scene/SceneInstance.hpp"

#include "RunTimeVar.hpp"
#include "TimeManager.hpp"

#define UNREFERENCED_PARAMETER(P) (P)

GLFWwindow* WindowManager::ptrWindow = nullptr;
//GLint WindowManager::width;
//GLint WindowManager::height;
//GLint WindowManager::viewportWidth;
//GLint WindowManager::viewportHeight;
//const char* WindowManager::title;

//bool WindowManager::isFocused = true;
//bool WindowManager::isFullscreen = false;
//GLint WindowManager::windowedWidth = 1600;   // Default windowed size
//GLint WindowManager::windowedHeight = 900;  // Default windowed size
//GLint WindowManager::windowedPosX = 0;      // Default window position
//GLint WindowManager::windowedPosY = 0;      // Default window position
//
//double WindowManager::deltaTime = 0.0;

//double WindowManager::lastFrameTime = 0.0;

// Scene framebuffer static members
static unsigned int sceneFrameBuffer = 0;
static unsigned int sceneColorTexture = 0;
static unsigned int sceneDepthTexture = 0;
static int sceneWidth = 1280;
static int sceneHeight = 720;

// Static editor camera that doesn't respond to input
static Camera* editorCamera = nullptr;

bool WindowManager::Initialize(GLint _width, GLint _height, const char* _title) {

    //WindowManager::width = _width;
    //WindowManager::height = _height;
    RunTimeVar::window.width = _width; 
    RunTimeVar::window.height = _height;

    RunTimeVar::window.viewportWidth = _width;
    RunTimeVar::window.viewportHeight = _height;
    RunTimeVar::window.title = _title;

    RunTimeVar::window.windowedWidth = _width;
    RunTimeVar::window.windowedHeight = _height;

    // Check if glfw init success
    if (!glfwInit()) {
        std::cout << "GLFW init has failed - abort program!!!" << std::endl;
        return false;
    }
    
    // If GLFW function fails, callback error
    glfwSetErrorCallback(error_cb);

    // Setup GLFW hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_RED_BITS, 8); glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8); glfwWindowHint(GLFW_ALPHA_BITS, 8);

    // Create window and check if success
    ptrWindow = glfwCreateWindow(RunTimeVar::window.width, RunTimeVar::window.height, RunTimeVar::window.title, NULL, NULL);
    if (!ptrWindow) {
        std::cerr << "GLFW unable to create OpenGL context - abort program\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(ptrWindow);

    // Set callback for FB size change
    glfwSetFramebufferSizeCallback(ptrWindow, fbsize_cb);
    glfwSetWindowFocusCallback(ptrWindow, window_focus_callback);

    // Initializes GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, _width, _height);

    return true;
}

void WindowManager::ToggleFullscreen() {
    if (RunTimeVar::window.isFullscreen) {
        // Restore to windowed mode
        glfwSetWindowMonitor(ptrWindow, nullptr, RunTimeVar::window.windowedPosX, RunTimeVar::window.windowedPosY, RunTimeVar::window.windowedWidth, RunTimeVar::window.windowedHeight, 0);
    }
    else {
        // Save current window position and size
        glfwGetWindowPos(ptrWindow, &RunTimeVar::window.windowedPosX, &RunTimeVar::window.windowedPosY);
        glfwGetWindowSize(ptrWindow, &RunTimeVar::window.windowedWidth, &RunTimeVar::window.windowedHeight);

        // Get the primary monitor and its video mode
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        // Switch to fullscreen
        glfwSetWindowMonitor(ptrWindow, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    RunTimeVar::window.isFullscreen = !RunTimeVar::window.isFullscreen; // Toggle fullscreen state
}

void WindowManager::MinimizeWindow() {
    glfwIconifyWindow(ptrWindow);  // Minimizes the window
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

GLFWwindow* WindowManager::getWindow() {
    return ptrWindow;
}

void WindowManager::SetWindowShouldClose()
{
    glfwSetWindowShouldClose(ptrWindow, 1);
}

bool WindowManager::ShouldClose() {
    return glfwWindowShouldClose(ptrWindow);
}

void WindowManager::Exit() {
    glfwDestroyWindow(ptrWindow);
    glfwTerminate();
}

void WindowManager::error_cb(int error, char const* description) {
#ifdef _DEBUG
    std::cerr << "GLFW error: " << description << ", " << error << std::endl;
#else
    (void)error;        // Avoid unused parameter warning
    (void)description;  // Avoid unused parameter warning
#endif
}

void WindowManager::fbsize_cb(GLFWwindow* ptr_win, int _width, int _height) {
    UNREFERENCED_PARAMETER(ptr_win);

#ifdef _DEBUG
    std::cout << "fbsize_cb getting called!!!" << std::endl;
#endif
    //WindowManager::width = _width;
    //WindowManager::height = _height;
    RunTimeVar::window.width = _width;
    RunTimeVar::window.height = _height;

    glViewport(0, 0, _width, _height);

    // Call GraphicsManager to update UI positions based on new window size
    //GraphicsManager::OnWindowResize(_width, _height);
}

GLint WindowManager::GetWindowWidth()
{
    return RunTimeVar::window.width;
}

GLint WindowManager::GetWindowHeight()
{
    return RunTimeVar::window.height;
}

GLint WindowManager::GetViewportWidth()
{
    //std::cout << "viewportW: " << viewportWidth << ", normalW: " << width << "\n";
    return RunTimeVar::window.viewportWidth;
}

GLint WindowManager::GetViewportHeight()
{
    return RunTimeVar::window.viewportHeight;
}

void WindowManager::SetWindowTitle(const char* _title) {
    glfwSetWindowTitle(ptrWindow, _title);
}

void WindowManager::window_focus_callback(GLFWwindow* window, int focused) {

    //if (!focused && !isFullscreen && !DuckEngine::isEditor)  glfwIconifyWindow(ptrWindow);  // Minimizes the window

    //UNREFERENCED_PARAMETER(window);
    //isFocused = focused != 0;
}

bool WindowManager::IsWindowMinimized() {
    return glfwGetWindowAttrib(ptrWindow, GLFW_ICONIFIED) != 0;
}

bool WindowManager::IsWindowFocused() {
    return RunTimeVar::window.isFocused;
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