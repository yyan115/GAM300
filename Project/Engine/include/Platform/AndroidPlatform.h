/* Start Header ************************************************************************/
/*!
\file       AndroidPlatform.h
\author     Yan Yu
\date       Oct 8, 2025
\brief      Platform abstraction layer for Android, implementing the IPlatform interface
            for Android-specific window, graphics, input, and asset management

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "IPlatform.h"
#include "../Engine.h"

#ifdef ANDROID
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <EGL/egl.h>
#include "Input/VirtualControls.hpp"

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

    void SetCursorLocked(bool locked) override;
    bool IsCursorLocked() override;
    
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
    
    // Input handling methods
    ENGINE_API void HandleTouchEvent(int action, float x, float y);
    ENGINE_API void HandleKeyEvent(int keyCode, int action);
};

#endif // ANDROID