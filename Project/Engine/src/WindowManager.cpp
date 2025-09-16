#include "pch.h"

#include <glad/glad.h>

#include "WindowManager.hpp"
#include "Engine.h"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"

#define UNREFERENCED_PARAMETER(P) (P)

GLFWwindow* WindowManager::ptrWindow = nullptr;
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
    ptrWindow = glfwCreateWindow(width, height, title, NULL, NULL);
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
    if (isFullscreen) {
        // Restore to windowed mode
        glfwSetWindowMonitor(ptrWindow, nullptr, windowedPosX, windowedPosY, windowedWidth, windowedHeight, 0);
    }
    else {
        // Save current window position and size
        glfwGetWindowPos(ptrWindow, &windowedPosX, &windowedPosY);
        glfwGetWindowSize(ptrWindow, &windowedWidth, &windowedHeight);

        // Get the primary monitor and its video mode
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        // Switch to fullscreen
        glfwSetWindowMonitor(ptrWindow, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    isFullscreen = !isFullscreen; // Toggle fullscreen state
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
    return isFocused;
}

void WindowManager::updateDeltaTime() {
    const double targetDeltaTime = 1.0 / 60.0; // cap at 60fps

    double currentTime = glfwGetTime();
    double frameTime = currentTime - lastFrameTime;

    double remainingTime = targetDeltaTime - frameTime;

    //Limit to 60 FPS?
    //// Sleep only if we have at least 5 ms remaining
    //if (remainingTime > 0.005) {
    //    std::this_thread::sleep_for(std::chrono::milliseconds((int)((remainingTime - 0.001) * 1000)));
    //}
    //// Busy-wait the last few milliseconds
    //while ((glfwGetTime() - lastFrameTime) < targetDeltaTime) {}

    // Update deltaTime
    currentTime = glfwGetTime();
    deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    glfwSwapInterval(1);
}

double WindowManager::getDeltaTime() {
    return deltaTime;
}
double WindowManager::getFps() {
    return deltaTime > 0.0 ? 1.0 / deltaTime : 0.0;
}
