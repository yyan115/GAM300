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

void WindowManager::SetViewportDimensions(GLint width, GLint height)
{
    viewportWidth = width;
    viewportHeight = height;
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