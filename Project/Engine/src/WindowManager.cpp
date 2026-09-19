#include "pch.h"

#include "Graphics/OpenGL.h"
#include "Platform/IPlatform.h"

#include "WindowManager.hpp"
#include "Engine.h"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"

#include "RunTimeVar.hpp"
#include "TimeManager.hpp"
#include "UI/QuitConfirmation.hpp"
#include <Logging.hpp>

#define UNREFERENCED_PARAMETER(P) (P)

IPlatform* WindowManager::platform = nullptr;
PlatformWindow WindowManager::ptrWindow = nullptr;
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


bool WindowManager::Initialize(GLint _width, GLint _height, const char* _title) {

    //WindowManager::width = _width;
    //WindowManager::height = _height;
    RunTimeVar::window.width = _width; 
    RunTimeVar::window.height = _height;
    RunTimeVar::window.viewportWidth = _width;
    RunTimeVar::window.viewportHeight = _height;


    // Create platform instance
    platform = CreatePlatform();
    if (!platform) {
        ENGINE_PRINT("Platform creation failed - abort program!!!\n");
        return false;
    }

    // Initialize platform window
    if (!platform->InitializeWindow(_width, _height, _title)) {
        ENGINE_PRINT("Platform window initialization failed - abort program!!!\n");
        return false;
    }

    // Initialize graphics context
    if (!platform->InitializeGraphics()) {
        ENGINE_PRINT("Platform graphics initialization failed - abort program!!!\n");
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
        ENGINE_PRINT("Failed to initialize GLAD\n");
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
        RunTimeVar::window.isFullscreen = platform->IsFullscreen(); // Toggle fullscreen state
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

// Set when the engine itself decides to close. A close flag the platform raised
// without it came from the window, and goes to QuitConfirmation instead.
static bool s_closeApproved = false;

void WindowManager::SetWindowShouldClose()
{
    s_closeApproved = true;
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
    // A window made after this one starts with no close decided.
    s_closeApproved = false;
    if (platform) {
        platform->DestroyWindow();
        delete platform;
        platform = nullptr;
    }
}

void WindowManager::error_cb(int error, char const* description) {
#ifdef _DEBUG
    ENGINE_PRINT(EngineLogging::LogLevel::Error, "GLFW error: ", description, ", ", error, "\n");
#else
    (void)error;        // Avoid unused parameter warning
    (void)description;  // Avoid unused parameter warning
#endif
}

void WindowManager::fbsize_cb(PlatformWindow ptr_win, int _width, int _height) {
    UNREFERENCED_PARAMETER(ptr_win);

#ifdef _DEBUG
    ENGINE_PRINT("fbsize_cb getting called!!!\n");
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
#ifndef EDITOR
    return RunTimeVar::window.width;
#else
    return RunTimeVar::window.gameResolutionWidth;
#endif;
}

GLint WindowManager::GetWindowHeight()
{
#ifndef EDITOR
    return RunTimeVar::window.height;
#else
    return RunTimeVar::window.gameResolutionHeight;
#endif
}

GLint WindowManager::GetViewportWidth()
{
#ifndef EDITOR
    return RunTimeVar::window.viewportWidth;
#else
    return RunTimeVar::window.gameViewportWidth;
#endif
}

GLint WindowManager::GetViewportHeight()
{
#ifndef EDITOR
    return RunTimeVar::window.viewportHeight;
#else
    return RunTimeVar::window.gameViewportHeight;
#endif
}

void WindowManager::SetViewportDimensions(GLint width, GLint height)
{
    RunTimeVar::window.viewportWidth = width;
    RunTimeVar::window.viewportHeight = height;
}

void WindowManager::SetWindowTitle(const char* _title) {
    if (platform) {
        platform->SetWindowTitle(_title);
    }
}

void WindowManager::SetVSync(bool enabled) {
    if (platform) {
        platform->SetVSync(enabled);
        RunTimeVar::window.vsync = enabled;
    }
}

void WindowManager::SetFullscreen(bool enabled) {
    if (platform) {
        platform->SetFullscreen(enabled);
        RunTimeVar::window.isFullscreen = enabled;
    }
}

bool WindowManager::IsFullscreen() {
    if (platform) {
        return platform->IsFullscreen();
    }
    return RunTimeVar::window.isFullscreen;
}

void WindowManager::window_focus_callback(PlatformWindow window, int focused) {

    //if (!focused && !isFullscreen && !DuckEngine::isEditor)  glfwIconifyWindow(ptrWindow);  // Minimizes the window
	(void)focused,window;
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
    return platform && platform->IsWindowFocused();
}

// ============================================================================
// CURSOR MANAGEMENT - Robust system that works with ImGui
// ============================================================================

using CursorMode = IPlatform::CursorMode;

// Static state tracking
static bool s_cursorLockRequested = false;   // What game code wants
static CursorMode s_appliedCursorMode = CursorMode::Free;  // Current actual state
static bool s_cursorPausedByUser = false;    // User pressed ESC to temporarily unlock

// The cursor mode the current state calls for
static CursorMode DesiredCursorMode() {
    // The quit prompt needs the cursor. The game's own request is left as it
    // is, so the lock comes back by itself when the prompt goes.
    bool lock = s_cursorLockRequested && !s_cursorPausedByUser
        && !QuitConfirmation::IsPrompting();
#ifdef EDITOR
    // In Editor, only actually lock cursor if game is playing. ImGui sets the
    // cursor itself every frame unless it is locked, so it is never confined.
    if (!Engine::ShouldRunGameLogic()) lock = false;
    return lock ? CursorMode::Locked : CursorMode::Free;
#else
    if (lock) return CursorMode::Locked;
    // A fullscreen game keeps the pointer on its own screen. A window leaves
    // it free to go to other programs.
    return WindowManager::IsFullscreen() ? CursorMode::Confined : CursorMode::Free;
#endif
}

// Hands the platform the mode the current state calls for, when it changes
static void ApplyCursorMode(IPlatform* platform) {
    const CursorMode mode = DesiredCursorMode();
    if (!platform || mode == s_appliedCursorMode) return;
    platform->SetCursorMode(mode);
    s_appliedCursorMode = mode;
}

void WindowManager::SetCursorLocked(bool locked) {
    s_cursorLockRequested = locked;
    // When game code explicitly requests locking (e.g. unpausing), clear the
    // "paused by user" flag so UpdateCursorState() will actually lock the cursor.
    if (locked) {
        s_cursorPausedByUser = false;
    }
    // Actual locking happens in UpdateCursorState() each frame
}

bool WindowManager::IsCursorLocked() {
    return s_appliedCursorMode == CursorMode::Locked;
}

bool WindowManager::IsCursorLockRequested() {
    return s_cursorLockRequested;
}

void WindowManager::ForceUnlockCursor() {
    s_cursorLockRequested = false;
    s_cursorPausedByUser = false;
    ApplyCursorMode(platform);
}

void WindowManager::PauseCursorLock() {
    // User temporarily paused cursor lock (e.g., pressed ESC in Editor)
    s_cursorPausedByUser = true;
    ApplyCursorMode(platform);
}

void WindowManager::ResumeCursorLock() {
    // User clicked back in game panel to resume cursor lock
    if (s_cursorLockRequested) {
        s_cursorPausedByUser = false;
        ApplyCursorMode(platform);
    }
}

bool WindowManager::IsCursorPausedByUser() {
    return s_cursorPausedByUser;
}

void WindowManager::UpdateCursorState() {
    if (!platform) return;

#ifdef EDITOR
    if (!Engine::ShouldRunGameLogic()) {
        s_cursorPausedByUser = false; // Reset pause when game stops
    }
#endif

    // Also picks up a switch between fullscreen and windowed, within a frame
    ApplyCursorMode(platform);
}

// ============================================================================

// Platform abstraction methods
void WindowManager::SwapBuffers() {
    if (platform) {
        platform->SwapBuffers();
    }
}

// A close the window raised on its own, such as Alt+F4, is withdrawn and
// handed to QuitConfirmation, which asks first. The editor's window and the
// Android activity close the way they always have.
static void RouteWindowCloseRequest(IPlatform* platform) {
#if !defined(EDITOR) && !defined(ANDROID)
    if (!s_closeApproved && platform->ShouldClose()) {
        platform->SetShouldClose(false);
        QuitConfirmation::OnWindowCloseRequest();
    }
#else
    (void)platform;
#endif
}

void WindowManager::PollEvents() {
    if (platform) {
        platform->PollEvents();
        RouteWindowCloseRequest(platform);
    }
}

IPlatform* WindowManager::GetPlatform() {
    return platform;
}

bool WindowManager::InitPlatformOnly() {
    if (!platform) {
        platform = CreatePlatform();
    }
    return platform != nullptr;
}
