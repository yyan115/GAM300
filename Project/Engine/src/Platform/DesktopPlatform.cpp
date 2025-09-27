#include "pch.h"

#ifndef ANDROID
#include "Platform/DesktopPlatform.h"
#include "Input/Keys.h"
#include <glad/glad.h>
#include <iostream>
#include <chrono>
#include "Logging.hpp"

// Static instance pointer for callbacks
static DesktopPlatform* s_instance = nullptr;

DesktopPlatform::DesktopPlatform() 
    : window(nullptr)
    , isFullscreen(false)
    , windowedWidth(1280)
    , windowedHeight(720)
    , windowedPosX(100)
    , windowedPosY(100)
{
    s_instance = this;
}

DesktopPlatform::~DesktopPlatform() {
    DestroyWindow();
    s_instance = nullptr;
}

bool DesktopPlatform::InitializeWindow(int width, int height, const char* title) {
    if (!glfwInit()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to initialize GLFW\n");
        //std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Set GLFW error callback
    glfwSetErrorCallback(ErrorCallback);

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to create GLFW window\n");
        //std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    // Store window dimensions
    windowedWidth = width;
    windowedHeight = height;

    // Set up callbacks
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetWindowFocusCallback(window, FocusCallback);

    return true;
}

void DesktopPlatform::DestroyWindow() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

bool DesktopPlatform::ShouldClose() {
    return window ? glfwWindowShouldClose(window) : true;
}

void DesktopPlatform::SetShouldClose(bool shouldClose) {
    if (window) {
        glfwSetWindowShouldClose(window, shouldClose ? GLFW_TRUE : GLFW_FALSE);
    }
}

void DesktopPlatform::SwapBuffers() {
    if (window) {
        glfwSwapBuffers(window);
    }
}

void DesktopPlatform::PollEvents() {
    glfwPollEvents();
}

int DesktopPlatform::GetWindowWidth() {
    if (window) {
        int width;
        glfwGetWindowSize(window, &width, nullptr);
        return width;
    }
    return 0;
}

int DesktopPlatform::GetWindowHeight() {
    if (window) {
        int height;
        glfwGetWindowSize(window, nullptr, &height);
        return height;
    }
    return 0;
}

void DesktopPlatform::SetWindowTitle(const char* title) {
    if (window) {
        glfwSetWindowTitle(window, title);
    }
}

void DesktopPlatform::ToggleFullscreen() {
    if (!window) return;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (isFullscreen) {
        // Switch to windowed
        glfwSetWindowMonitor(window, nullptr, windowedPosX, windowedPosY, 
                           windowedWidth, windowedHeight, GLFW_DONT_CARE);
        isFullscreen = false;
    } else {
        // Store current windowed position and size
        glfwGetWindowPos(window, &windowedPosX, &windowedPosY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
        
        // Switch to fullscreen
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        isFullscreen = true;
    }
}

void DesktopPlatform::MinimizeWindow() {
    if (window) {
        glfwIconifyWindow(window);
    }
}

bool DesktopPlatform::IsWindowMinimized() {
    if (window) {
        return glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
    }
    return false;
}

bool DesktopPlatform::IsWindowFocused() {
    if (window) {
        return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    }
    return false;
}

bool DesktopPlatform::IsKeyPressed(Input::Key key) {
    if (!window) return false;
    
    int glfwKey = static_cast<int>(key);  // Assuming direct mapping
    return glfwGetKey(window, glfwKey) == GLFW_PRESS;
}

bool DesktopPlatform::IsMouseButtonPressed(Input::MouseButton button) {
    if (!window) return false;
    
    int glfwButton = static_cast<int>(button);  // Assuming direct mapping
    return glfwGetMouseButton(window, glfwButton) == GLFW_PRESS;
}

void DesktopPlatform::GetMousePosition(double* x, double* y) {
    if (window) {
        glfwGetCursorPos(window, x, y);
    } else {
        *x = 0.0;
        *y = 0.0;
    }
}

double DesktopPlatform::GetTime() {
    return glfwGetTime();
}

bool DesktopPlatform::InitializeGraphics() {
    if (!window) return false;
    
    MakeContextCurrent();
    
    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to initialize GLAD\n");
        //std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    return true;
}

void DesktopPlatform::MakeContextCurrent() {
    if (window) {
        glfwMakeContextCurrent(window);
    }
}

void* DesktopPlatform::GetNativeWindow() {
    return window;
}

// Static callback implementations
void DesktopPlatform::ErrorCallback(int error, const char* description) {
    ENGINE_PRINT(EngineLogging::LogLevel::Error, "GLFW Error ", error, ": ", description, "\n");
    //std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void DesktopPlatform::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void DesktopPlatform::FocusCallback(GLFWwindow* window, int focused) {
    // Handle focus changes if needed
}

#endif // !ANDROID