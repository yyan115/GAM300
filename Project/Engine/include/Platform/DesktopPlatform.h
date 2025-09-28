#pragma once

#include "IPlatform.h"

#ifndef ANDROID
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class DesktopPlatform : public IPlatform {
private:
    GLFWwindow* window;
    bool isFullscreen;
    int windowedWidth, windowedHeight;
    int windowedPosX, windowedPosY;
    
    // Static callbacks for GLFW
    static void ErrorCallback(int error, const char* description);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void FocusCallback(GLFWwindow* window, int focused);

    // Input callbacks for GLFW
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Helper functions for input mapping
    static Input::Key GLFWKeyToEngineKey(int glfwKey);
    static Input::MouseButton GLFWButtonToEngineButton(int glfwButton);
    static Input::KeyAction GLFWActionToEngineAction(int glfwAction);
    
public:
    DesktopPlatform();
    virtual ~DesktopPlatform();
    
    // IPlatform interface
    bool InitializeWindow(int width, int height, const char* title) override;
    void DestroyWindow() override;
    bool ShouldClose() override;
    void SetShouldClose(bool shouldClose) override;
    void SwapBuffers() override;
    void PollEvents() override;
    
    int GetWindowWidth() override;
    int GetWindowHeight() override;
    void SetWindowTitle(const char* title) override;
    void ToggleFullscreen() override;
    void MinimizeWindow() override;
    bool IsWindowMinimized() override;
    bool IsWindowFocused() override;
    
    bool IsKeyPressed(Input::Key key) override;
    bool IsMouseButtonPressed(Input::MouseButton button) override;
    void GetMousePosition(double* x, double* y) override;

    // Cursor management
    void SetCursorMode(void* window, bool locked) override;
    void SetCursorPosition(void* window, double x, double y) override;
    
    double GetTime() override;
    
    bool InitializeGraphics() override;
    bool MakeContextCurrent() override;

    // Asset management
    std::vector<std::string> ListAssets(const std::string& folder, bool recursive = true) override;
    std::vector<uint8_t> ReadAsset(const std::string& path) override;
    bool FileExists(const std::string& path) override;

    void* GetNativeWindow() override;
    
    // Desktop-specific getters
    GLFWwindow* GetGLFWWindow() { return window; }
};

#endif // !ANDROID