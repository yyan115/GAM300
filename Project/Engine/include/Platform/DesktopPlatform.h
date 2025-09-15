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
    
    double GetTime() override;
    
    bool InitializeGraphics() override;
    void MakeContextCurrent() override;
    
    void* GetNativeWindow() override;
    
    // Desktop-specific getters
    GLFWwindow* GetGLFWWindow() { return window; }
};

#endif // !ANDROID