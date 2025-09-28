#pragma once

#include "IPlatform.h"
#include "../Engine.h"

#ifdef ANDROID
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <EGL/egl.h>

class AndroidPlatform : public IPlatform {
private:
    ANativeWindow* window;
    AAssetManager* assetManager;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    int windowWidth, windowHeight;
    bool shouldClose;
    bool isFocused;
    
    // Input state tracking
    bool keyStates[512];  // Simple key state array
    bool mouseButtonStates[8];
    double mouseX, mouseY;

    std::vector<std::string> assetPaths;
    
public:
    AndroidPlatform();
    virtual ~AndroidPlatform();
    
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

    // Cursor management (no-op on Android)
    void SetCursorMode(void* window, bool locked) override;
    void SetCursorPosition(void* window, double x, double y) override;
    
    double GetTime() override;
    
    bool InitializeGraphics() override;
    bool MakeContextCurrent() override;
    ENGINE_API void ReleaseContext();

    // Asset management
    std::vector<std::string> ListAssets(const std::string& folder, bool recursive = true) override;
    std::vector<uint8_t> ReadAsset(const std::string& path) override;
    bool FileExists(const std::string& path) override;

    void* GetNativeWindow() override;
    
    // Android-specific methods
    ENGINE_API void SetNativeWindow(ANativeWindow* nativeWindow);
    ENGINE_API void SetAssetManager(AAssetManager* manager);
    ENGINE_API AAssetManager* GetAssetManager() const { return assetManager; }
    void HandleInputEvent(/* Android input event parameters */);
};

#endif // ANDROID