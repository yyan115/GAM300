#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

// Include engine headers
#include "Engine.h"
#include "GameManager.h"
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Platform/AndroidPlatform.h"
#include "Platform/IPlatform.h"

#define LOG_TAG "GAM300"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global state
static bool engineInitialized = false;
static ANativeWindow* nativeWindow = nullptr;

extern "C" JNIEXPORT jstring JNICALL
Java_com_gam300_game_MainActivity_stringFromJNI(JNIEnv* env, jobject /* this */) {
    std::string hello = "GAM300 Engine Running!";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_gam300_game_MainActivity_initEngine(JNIEnv* env, jobject /* this */, jobject assetManager, jint width, jint height) {
    LOGI("Initializing GAM300 Engine: %dx%d", width, height);

    if (!engineInitialized) {
        // Get native AssetManager and set it in the platform
        AAssetManager* nativeAssetManager = AAssetManager_fromJava(env, assetManager);

        // Initialize Engine and GameManager (same as game's main.cpp)
        Engine::Initialize();

        // Set the AssetManager in the Android platform
        IPlatform* platform = WindowManager::GetPlatform();
        if (platform) {
            AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
            androidPlatform->SetAssetManager(nativeAssetManager);
            LOGI("AssetManager set in Android platform");
        }

        GameManager::Initialize();

        engineInitialized = true;
        LOGI("Engine and GameManager initialized successfully");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_gam300_game_MainActivity_setSurface(JNIEnv* env, jobject /* this */, jobject surface) {
    if (surface) {
        nativeWindow = ANativeWindow_fromSurface(env, surface);
        LOGI("Surface set: %p", nativeWindow);

        // Set native window in AndroidPlatform
        IPlatform* platform = WindowManager::GetPlatform();
        if (platform && engineInitialized) {
            AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
            androidPlatform->SetNativeWindow(nativeWindow);

            // Initialize graphics context now that we have a surface
            if (androidPlatform->InitializeGraphics()) {
                LOGI("Graphics initialized successfully");
                // Now initialize graphics resources (scenes, lighting, etc.)
                if (Engine::InitializeGraphicsResources()) {
                    LOGI("Graphics resources initialized successfully");
                } else {
                    LOGE("Failed to initialize graphics resources");
                }
            } else {
                LOGE("Failed to initialize graphics");
            }
        }
    } else {
        if (nativeWindow) {
            ANativeWindow_release(nativeWindow);
            nativeWindow = nullptr;
        }
        LOGI("Surface cleared");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_gam300_game_MainActivity_renderFrame(JNIEnv* env, jobject /* this */) {
    static int frameCount = 0;
    if (engineInitialized) {
        if (frameCount % 60 == 0) { // Log every 60 frames (~1 second)
            LOGI("renderFrame() called - frame %d", frameCount);
        }

        // Update engine and game manager (same as game's main.cpp)
        Engine::Update();
        GameManager::Update();

        // Draw frame (same as game's main.cpp)
        Engine::StartDraw();
        Engine::Draw();
        Engine::EndDraw();

        frameCount++;
    } else {
        LOGI("renderFrame() called but engine not initialized");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_gam300_game_MainActivity_destroyEngine(JNIEnv* env, jobject /* this */) {
    LOGI("Destroying GAM300 Engine");
    
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
    
    if (engineInitialized) {
        // Shutdown in reverse order (same as game's main.cpp)
        GameManager::Shutdown();
        Engine::Shutdown();
        engineInitialized = false;
        LOGI("Engine and GameManager destroyed");
    }
}