#include "pch.h"

#ifdef ANDROID
#include "Platform/AndroidPlatform.h"
#include <chrono>
#include <functional>
#include <android/log.h>
#include "WindowManager.hpp"

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
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "AndroidPlatform::InitializeWindow(%dx%d)", width, height);
    return true;  // Always succeed - graphics will be initialized later when surface is set
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
    // __android_log_print(ANDROID_LOG_INFO, "GAM300", "SWAPPED BUFFED\n");
    }
    // __android_log_print(ANDROID_LOG_INFO, "GAM300", "End buffer\n");
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
    if (!window) {
        __android_log_print(ANDROID_LOG_WARN, "GAM300", "AndroidPlatform::InitializeGraphics() - No native window set yet, will initialize later");
        return true; // Don't fail - just defer initialization
    }

    // Check if already initialized
    if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL already initialized");
        return true;
    }

    // Get the default display
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to get EGL display");
        return false;
    }

    // Initialize EGL
    if (!eglInitialize(display, nullptr, nullptr)) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to initialize EGL");
        return false;
    }

    // Choose EGL config
    const EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) || num_configs != 1) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to choose EGL config");
        return false;
    }

    // Create EGL context
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to create EGL context");
        return false;
    }

    // Create EGL surface
    surface = eglCreateWindowSurface(display, config, window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to create EGL surface");
        return false;
    }

    // Make context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to make EGL context current");
        return false;
    }

    // Get surface dimensions
    eglQuerySurface(display, surface, EGL_WIDTH, &windowWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT, &windowHeight);
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Updated window dimensions to actual surface size: %dx%d", windowWidth, windowHeight);

    // Set the OpenGL viewport to match the surface size
    glViewport(0, 0, windowWidth, windowHeight);
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "OpenGL viewport set to: %dx%d", windowWidth, windowHeight);

    // Update WindowManager with the correct dimensions so projection matrix calculations use real surface size
    WindowManager::fbsize_cb(static_cast<PlatformWindow>(window), windowWidth, windowHeight);
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Updated WindowManager dimensions to: %dx%d", windowWidth, windowHeight);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "OpenGL depth testing enabled");

    // Keep the context current for now - it will be released after graphics resources are loaded
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL initialized successfully: %dx%d", windowWidth, windowHeight);
    return true;
}

void AndroidPlatform::ReleaseContext() {
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL context released from main thread");
    }
}

bool AndroidPlatform::MakeContextCurrent() {
    // __android_log_print(ANDROID_LOG_INFO, "GAM300", "MakeContextCurrent() - Display: %p, Context: %p, Surface: %p",
    //                    display, context, surface);

    if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT) {
        EGLBoolean result = eglMakeCurrent(display, surface, surface, context);
        if (result == EGL_TRUE) {
            // __android_log_print(ANDROID_LOG_INFO, "GAM300", "eglMakeCurrent() SUCCESS");
            return true;
        } else {
            EGLint error = eglGetError();
            __android_log_print(ANDROID_LOG_ERROR, "GAM300", "eglMakeCurrent() FAILED - Error: 0x%x", error);
            return false;
        }
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "MakeContextCurrent() - Invalid EGL handles!");
        return false;
    }
}

void* AndroidPlatform::GetNativeWindow() {
    return static_cast<void*>(window);
}

void AndroidPlatform::SetNativeWindow(ANativeWindow* nativeWindow) {
    window = nativeWindow;
}

void AndroidPlatform::SetAssetManager(AAssetManager* manager) {
    assetManager = manager;
}

// Asset management for Android - uses AssetManager API
std::vector<std::string> AndroidPlatform::ListAssets(const std::string& folder, bool recursive) {
    if (!assetPaths.empty()) return assetPaths;
    if (!assetManager) return assetPaths;

    //// For Android, we need to recursively explore directories using AssetManager
    //std::function<void(const std::string&)> listAssetsRecursive = [&](const std::string& currentFolder) {
    //    AAssetDir* assetDir = AAssetManager_openDir(assetManager, currentFolder.c_str());
    //    if (!assetDir) return;

    //    const char* filename;
    //    while ((filename = AAssetDir_getNextFileName(assetDir)) != nullptr) {
    //        std::string fullPath = currentFolder.empty() ? filename : currentFolder + "/" + filename;

    //        // Check if this is a file by trying to open it
    //        AAsset* asset = AAssetManager_open(assetManager, fullPath.c_str(), AASSET_MODE_UNKNOWN);
    //        if (asset) {
    //            // It's a file
    //            assetPaths.push_back(fullPath);
    //            AAsset_close(asset);
    //        } else if (recursive) {
    //            // Try as directory for recursive search
    //            listAssetsRecursive(fullPath);
    //        }
    //    }

    //    AAssetDir_close(assetDir);
    //};

    //listAssetsRecursive(folder);

    AAsset* asset = AAssetManager_open(assetManager, "asset_manifest.txt", AASSET_MODE_BUFFER);
    if (!asset) return assetPaths;

    off_t size = AAsset_getLength(asset);
    std::string buffer(size, '\0');
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    std::istringstream stream(buffer);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            assetPaths.push_back(line);
        }
    }

    return assetPaths;
}

std::vector<uint8_t> AndroidPlatform::ReadAsset(const std::string& path) {
    std::vector<uint8_t> data;
    if (!assetManager) return data;

    AAsset* asset = AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) return data;

    off_t length = AAsset_getLength(asset);
    data.resize(length);

    int64_t readBytes = AAsset_read(asset, data.data(), length);
    if (readBytes < 0) {
        data.clear(); // error
    }

    AAsset_close(asset);
    return data;
}

bool AndroidPlatform::FileExists(const std::string& path) {
    if (!assetManager) return false;
    AAsset* asset = AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) {
        AAsset_close(asset);
        return true;
    }
    return false;
}

// Cursor management (no-op implementations for Android)
void AndroidPlatform::SetCursorMode(void* window, bool locked) {
    // No cursor control needed on Android
    (void)window;
    (void)locked;
}

void AndroidPlatform::SetCursorPosition(void* window, double x, double y) {
    // No cursor positioning needed on Android
    (void)window;
    (void)x;
    (void)y;
}

#endif // ANDROID