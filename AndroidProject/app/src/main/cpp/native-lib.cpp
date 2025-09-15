#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

// Include engine headers
#include "Engine.h"
#include "GameManager.h"
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"

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
Java_com_gam300_game_MainActivity_initEngine(JNIEnv* env, jobject /* this */, jint width, jint height) {
    LOGI("Initializing GAM300 Engine: %dx%d", width, height);
    
    if (!engineInitialized) {
        // Initialize Engine and GameManager (same as game's main.cpp)
        Engine::Initialize();
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
        
        // TODO: Set native window in AndroidPlatform
        // This will require adding a method to get the platform instance
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
    if (engineInitialized) {
        // Update engine and game manager (same as game's main.cpp)
        Engine::Update();
        GameManager::Update();
        
        // Draw frame (same as game's main.cpp)
        Engine::StartDraw();
        Engine::Draw();
        Engine::EndDraw();
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