#include "pch.h"

#ifdef ANDROID
#include "Platform/AndroidPlatform.h"
#include <chrono>
#include <android/log.h>

AndroidPlatform::AndroidPlatform() 
    : window(nullptr)
    , display(EGL_NO_DISPLAY)
    , context(EGL_NO_CONTEXT)
    , surface(EGL_NO_SURFACE)
    , windowWidth(0)
    , windowHeight(0)
    , shouldClose(false)
    , isFocused(true)
    , mouseX(0.0)
    , mouseY(0.0)
{
    // Initialize key states
    memset(keyStates, 0, sizeof(keyStates));
    memset(mouseButtonStates, 0, sizeof(mouseButtonStates));
}

AndroidPlatform::~AndroidPlatform() {
    DestroyWindow();
}

bool AndroidPlatform::InitializeWindow(int width, int height, const char* title) {
    windowWidth = width;
    windowHeight = height;
    // Title is ignored on Android
    return true;  // Stub implementation
}

void AndroidPlatform::DestroyWindow() {
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    if (display != EGL_NO_DISPLAY) {
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }
}

bool AndroidPlatform::ShouldClose() {
    return shouldClose;
}

void AndroidPlatform::SetShouldClose(bool close) {
    shouldClose = close;
}

void AndroidPlatform::SwapBuffers() {
    if (surface != EGL_NO_SURFACE) {
        eglSwapBuffers(display, surface);
    }
}

void AndroidPlatform::PollEvents() {
    // Android events are handled through JNI callbacks
    // This is a no-op stub
}

int AndroidPlatform::GetWindowWidth() {
    return windowWidth;
}

int AndroidPlatform::GetWindowHeight() {
    return windowHeight;
}

void AndroidPlatform::SetWindowTitle(const char* title) {
    // No-op on Android - titles are handled by the Android system
}

void AndroidPlatform::ToggleFullscreen() {
    // Android apps are typically fullscreen by default
    // This could be implemented to toggle immersive mode
}

void AndroidPlatform::MinimizeWindow() {
    // On Android, this would minimize the entire app
    // Typically handled by the Android system
}

bool AndroidPlatform::IsWindowMinimized() {
    return !isFocused;
}

bool AndroidPlatform::IsWindowFocused() {
    return isFocused;
}

bool AndroidPlatform::IsKeyPressed(Input::Key key) {
    // Simple stub - would need proper key mapping
    return false;
}

bool AndroidPlatform::IsMouseButtonPressed(Input::MouseButton button) {
    // Android touch events would be mapped to mouse buttons
    return false;
}

void AndroidPlatform::GetMousePosition(double* x, double* y) {
    if (x) *x = mouseX;
    if (y) *y = mouseY;
}

double AndroidPlatform::GetTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}

bool AndroidPlatform::InitializeGraphics() {
    // Stub implementation - would initialize EGL context
    return true;
}

void AndroidPlatform::MakeContextCurrent() {
    if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT) {
        eglMakeCurrent(display, surface, surface, context);
    }
}

void* AndroidPlatform::GetNativeWindow() {
    return static_cast<void*>(window);
}

void AndroidPlatform::SetNativeWindow(ANativeWindow* nativeWindow) {
    window = nativeWindow;
}

#endif // ANDROID