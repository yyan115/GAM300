#pragma once

#include "../Graphics/OpenGL.h"
#include "../Input/Keys.h"
#include <string>
#include <vector>

// Abstract platform interface
class IPlatform {
public:
    virtual ~IPlatform() = default;
    
    // Window management
    virtual bool InitializeWindow(int width, int height, const char* title) = 0;
    virtual void DestroyWindow() = 0;
    virtual bool ShouldClose() = 0;
    virtual void SetShouldClose(bool shouldClose) = 0;
    virtual void SwapBuffers() = 0;
    virtual void PollEvents() = 0;
    
    // Window properties
    virtual int GetWindowWidth() = 0;
    virtual int GetWindowHeight() = 0;
    virtual void SetWindowTitle(const char* title) = 0;
    virtual void ToggleFullscreen() = 0;
    virtual void MinimizeWindow() = 0;
    virtual bool IsWindowMinimized() = 0;
    virtual bool IsWindowFocused() = 0;
    
    // Input - uses engine key constants
    virtual bool IsKeyPressed(Input::Key key) = 0;
    virtual bool IsMouseButtonPressed(Input::MouseButton button) = 0;
    virtual void GetMousePosition(double* x, double* y) = 0;

    // Cursor control
    virtual void SetCursorLocked(bool locked) = 0;
    virtual bool IsCursorLocked() = 0;
    
    // Time
    virtual double GetTime() = 0;
    
    // OpenGL context
    virtual bool InitializeGraphics() = 0;
    virtual bool MakeContextCurrent() = 0;
        
    // Asset management
    virtual std::vector<std::string> ListAssets(const std::string& folder, bool recursive = true) = 0;
    virtual std::vector<uint8_t> ReadAsset(const std::string& path) = 0;
    virtual bool FileExists(const std::string& path) = 0;

    // Platform-specific getters (if needed by other systems)
    virtual void* GetNativeWindow() = 0;
};

// Factory function to create platform instance
IPlatform* CreatePlatform();