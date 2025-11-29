#pragma once

#include "Platform/Platform.h"
#include <string>

#include "RunTimeVar.hpp"

#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
// Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

class ENGINE_API WindowManager {
public:
    static bool Initialize(GLint width, GLint height, const char* title);

    static void UpdateViewportDimensions();

    static void Exit();

    static PlatformWindow getWindow();

    static void SetWindowShouldClose();

    static bool ShouldClose();
    
    static void SwapBuffers();
    static void PollEvents();

    static void error_cb(int error, char const* description);
    static void fbsize_cb(PlatformWindow ptr_win, int width, int height);

    static GLint GetWindowWidth();
    static GLint GetWindowHeight();

    /// <summary>
    /// 
    /// </summary>
    /// <returns></returns>
    static GLint GetViewportWidth();
    static GLint GetViewportHeight();
    static void SetViewportDimensions(GLint width, GLint height);

    static void SetWindowTitle(const char* title);

    static void ToggleFullscreen();
    static void MinimizeWindow();

    static bool IsWindowMinimized();
    static bool IsWindowFocused();
    static void window_focus_callback(PlatformWindow window, int focused);

    // Cursor management - robust system that works with ImGui
    static void SetCursorLocked(bool locked);  // Request cursor lock state
    static bool IsCursorLocked();              // Check if cursor is currently locked
    static bool IsCursorLockRequested();       // Check if game code requested cursor lock
    static void UpdateCursorState();           // Called each frame to enforce cursor state (after ImGui)
    static void ForceUnlockCursor();           // Force unlock (called when game stops, etc.)

    // Platform access
    static class IPlatform* GetPlatform();


private:

    //static bool isFocused;
    //static bool isFullscreen;      // Tracks whether the window is fullscreen
    //static GLint windowedWidth;    // Saved width for windowed mode
    //static GLint windowedHeight;   // Saved height for windowed mode
    //static GLint windowedPosX;     // Saved X position for windowed mode
    //static GLint windowedPosY;     // Saved Y position for windowed mode

    static class IPlatform* platform;
    static PlatformWindow ptrWindow;

    //static GLint width;
    //static GLint height;

    //static GLint viewportWidth;
    //static GLint viewportHeight;

    //static const char* title;

    //static double deltaTime;
    //static double lastFrameTime;
};
