/* Start Header ************************************************************************/
/*!
\file       DesktopPlatform.cpp
\author     Yan Yu
\date       Oct 8, 2025
\brief      Implementation of Desktop platform abstraction layer using GLFW for
            window management, input callbacks, and filesystem-based asset loading

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "pch.h"

#ifndef ANDROID
#include "Platform/DesktopPlatform.h"
#include "Input/Keys.h"
#include "Input/InputManager.hpp"
#include <glad/glad.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include "Logging.hpp"
#include "RunTimeVar.hpp"

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
        glfwTerminate();
        return false;
    }

    // Store window dimensions
    windowedWidth = width;
    windowedHeight = height;

    // Set up callbacks
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetWindowFocusCallback(window, FocusCallback);

    // Set up input callbacks for Engine's InputManager
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);

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
        return false;
    }
    
    return true;
}

bool DesktopPlatform::MakeContextCurrent() {
    if (window) {
        glfwMakeContextCurrent(window);
        return true;
    }
    return false;
}

void* DesktopPlatform::GetNativeWindow() {
    return window;
}

// Asset management for desktop - uses filesystem
std::vector<std::string> DesktopPlatform::ListAssets(const std::string& folder, bool recursive) {
    std::vector<std::string> assetPaths;

    if (!std::filesystem::exists(folder)) {
        return assetPaths;
    }

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                assetPaths.push_back(entry.path().generic_string());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                assetPaths.push_back(entry.path().generic_string());
            }
        }
    }

    return assetPaths;
}

std::vector<uint8_t> DesktopPlatform::ReadAsset(const std::string& path) {
    std::vector<uint8_t> data;
    if (!std::filesystem::exists(path)) {
        return data;
    }

    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        ifs.close();
        return data;
    }

    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    data.resize(size);
    if (!ifs.read(reinterpret_cast<char*>(data.data()), size)) {
        data.clear(); // read failed
    }

    ifs.close();
    return data;
}

bool DesktopPlatform::FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

// Static callback implementations
void DesktopPlatform::ErrorCallback(int error, const char* description) {
    ENGINE_PRINT(EngineLogging::LogLevel::Error, "GLFW Error ", error, ": ", description, "\n");
}

void DesktopPlatform::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

	// Update RunTimeVar so the game knows about the new window size
	RunTimeVar::window.width = width;
	RunTimeVar::window.height = height;

	(void)window;
}

void DesktopPlatform::FocusCallback(GLFWwindow* window, int focused) {
	(void)focused,window;
    // Handle focus changes if needed
}

// Input callback implementations
void DesktopPlatform::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	(void)mods, scancode, window;
    Input::Key engineKey = GLFWKeyToEngineKey(key);
    Input::KeyAction engineAction = GLFWActionToEngineAction(action);

    if (engineKey != Input::Key::UNKNOWN) {
        InputManager::OnKeyEvent(engineKey, engineAction);
    }
}

void DesktopPlatform::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods,window;
    Input::MouseButton engineButton = GLFWButtonToEngineButton(button);
    Input::KeyAction engineAction = GLFWActionToEngineAction(action);

    if (engineButton != Input::MouseButton::UNKNOWN) {
        InputManager::OnMouseButtonEvent(engineButton, engineAction);
    }
}

void DesktopPlatform::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    InputManager::OnMousePositionEvent(xpos, ypos);
	(void)window;
}

void DesktopPlatform::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    InputManager::OnScrollEvent(xoffset, yoffset);
	(void)window;
}

// Helper functions for input mapping
Input::Key DesktopPlatform::GLFWKeyToEngineKey(int glfwKey) {
    switch (glfwKey) {
        // Alphabet keys
        case GLFW_KEY_A: return Input::Key::A;
        case GLFW_KEY_B: return Input::Key::B;
        case GLFW_KEY_C: return Input::Key::C;
        case GLFW_KEY_D: return Input::Key::D;
        case GLFW_KEY_E: return Input::Key::E;
        case GLFW_KEY_F: return Input::Key::F;
        case GLFW_KEY_G: return Input::Key::G;
        case GLFW_KEY_H: return Input::Key::H;
        case GLFW_KEY_I: return Input::Key::I;
        case GLFW_KEY_J: return Input::Key::J;
        case GLFW_KEY_K: return Input::Key::K;
        case GLFW_KEY_L: return Input::Key::L;
        case GLFW_KEY_M: return Input::Key::M;
        case GLFW_KEY_N: return Input::Key::N;
        case GLFW_KEY_O: return Input::Key::O;
        case GLFW_KEY_P: return Input::Key::P;
        case GLFW_KEY_Q: return Input::Key::Q;
        case GLFW_KEY_R: return Input::Key::R;
        case GLFW_KEY_S: return Input::Key::S;
        case GLFW_KEY_T: return Input::Key::T;
        case GLFW_KEY_U: return Input::Key::U;
        case GLFW_KEY_V: return Input::Key::V;
        case GLFW_KEY_W: return Input::Key::W;
        case GLFW_KEY_X: return Input::Key::X;
        case GLFW_KEY_Y: return Input::Key::Y;
        case GLFW_KEY_Z: return Input::Key::Z;

        // Number keys
        case GLFW_KEY_0: return Input::Key::NUM_0;
        case GLFW_KEY_1: return Input::Key::NUM_1;
        case GLFW_KEY_2: return Input::Key::NUM_2;
        case GLFW_KEY_3: return Input::Key::NUM_3;
        case GLFW_KEY_4: return Input::Key::NUM_4;
        case GLFW_KEY_5: return Input::Key::NUM_5;
        case GLFW_KEY_6: return Input::Key::NUM_6;
        case GLFW_KEY_7: return Input::Key::NUM_7;
        case GLFW_KEY_8: return Input::Key::NUM_8;
        case GLFW_KEY_9: return Input::Key::NUM_9;

        // Special keys
        case GLFW_KEY_SPACE: return Input::Key::SPACE;
        case GLFW_KEY_ENTER: return Input::Key::ENTER;
        case GLFW_KEY_ESCAPE: return Input::Key::ESC;
        case GLFW_KEY_TAB: return Input::Key::TAB;
        case GLFW_KEY_BACKSPACE: return Input::Key::BACKSPACE;
        case GLFW_KEY_DELETE: return Input::Key::DELETE_;

        // Arrow keys
        case GLFW_KEY_UP: return Input::Key::UP;
        case GLFW_KEY_DOWN: return Input::Key::DOWN;
        case GLFW_KEY_LEFT: return Input::Key::LEFT;
        case GLFW_KEY_RIGHT: return Input::Key::RIGHT;

        // Function keys
        case GLFW_KEY_F1: return Input::Key::F1;
        case GLFW_KEY_F2: return Input::Key::F2;
        case GLFW_KEY_F3: return Input::Key::F3;
        case GLFW_KEY_F4: return Input::Key::F4;
        case GLFW_KEY_F5: return Input::Key::F5;
        case GLFW_KEY_F6: return Input::Key::F6;
        case GLFW_KEY_F7: return Input::Key::F7;
        case GLFW_KEY_F8: return Input::Key::F8;
        case GLFW_KEY_F9: return Input::Key::F9;
        case GLFW_KEY_F10: return Input::Key::F10;
        case GLFW_KEY_F11: return Input::Key::F11;
        case GLFW_KEY_F12: return Input::Key::F12;

        // Modifier keys
        case GLFW_KEY_LEFT_SHIFT:
        case GLFW_KEY_RIGHT_SHIFT: return Input::Key::SHIFT;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL: return Input::Key::CTRL;
        case GLFW_KEY_LEFT_ALT:
        case GLFW_KEY_RIGHT_ALT: return Input::Key::ALT;

        default: return Input::Key::UNKNOWN;
    }
}

Input::MouseButton DesktopPlatform::GLFWButtonToEngineButton(int glfwButton) {
    switch (glfwButton) {
        case GLFW_MOUSE_BUTTON_LEFT: return Input::MouseButton::LEFT;
        case GLFW_MOUSE_BUTTON_RIGHT: return Input::MouseButton::RIGHT;
        case GLFW_MOUSE_BUTTON_MIDDLE: return Input::MouseButton::MIDDLE;
        case GLFW_MOUSE_BUTTON_4: return Input::MouseButton::BUTTON_4;
        case GLFW_MOUSE_BUTTON_5: return Input::MouseButton::BUTTON_5;
        default: return Input::MouseButton::UNKNOWN;
    }
}

Input::KeyAction DesktopPlatform::GLFWActionToEngineAction(int glfwAction) {
    // GLFW: RELEASE=0, PRESS=1, REPEAT=2
    // Engine: PRESS=0, RELEASE=1, REPEAT=2
    switch (glfwAction) {
        case GLFW_RELEASE: return Input::KeyAction::RELEASE;  // 0 -> 1
        case GLFW_PRESS: return Input::KeyAction::PRESS;      // 1 -> 0
        case GLFW_REPEAT: return Input::KeyAction::REPEAT;    // 2 -> 2
        default: return Input::KeyAction::RELEASE;
    }
}

#endif // !ANDROID