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

// NEW: Add FMOD includes for Android-specific functions and types
#include <fmod/fmod_android.h>
#include <fmod/fmod_errors.h>
#include <fmod/fmod.h>

// NEW: Add include for AudioManager (relative path from Engine/include)
#include "Sound/AudioManager.hpp"

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
Java_com_gam300_game_MainActivity_initEngine(JNIEnv* env, jobject thiz, jobject assetManager, jint width, jint height) {
    LOGI("Initializing GAM300 Engine: %dx%d", width, height);

    if (!engineInitialized) {
        // Get native AssetManager and set it in the platform FIRST
        AAssetManager* nativeAssetManager = AAssetManager_fromJava(env, assetManager);

        // NEW: Initialize FMOD for Android JNI (required before any FMOD calls)
        JavaVM* jvm;
        env->GetJavaVM(&jvm);
        FMOD_RESULT fmodResult = FMOD_Android_JNI_Init(jvm, thiz);  // Pass 'thiz' (the Java activity) as the second argument
        if (fmodResult != FMOD_OK) {
            LOGE("FMOD_Android_JNI_Init failed: %s", FMOD_ErrorString(fmodResult));
            return;  // Abort if FMOD init fails
        }
        LOGI("FMOD JNI initialized successfully");

        // Initialize Engine (but NOT input config yet - need AssetManager first)
        // This creates the platform but doesn't load assets
        Engine::Initialize();

        // Set the AssetManager in the Android platform BEFORE loading input config
        IPlatform* platform = WindowManager::GetPlatform();
        if (platform) {
            AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
            androidPlatform->SetAssetManager(nativeAssetManager);
            LOGI("AssetManager set in Android platform");

            // Initialize assets first (preserve original order)
            Engine::InitializeAssets();
            LOGI("Engine assets initialized");

            // Load input config after assets are initialized
            Engine::LoadInputConfig();
            LOGI("Input config loaded");
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
                    // Release the OpenGL context now that graphics resources are loaded
                    androidPlatform->ReleaseContext();
                    LOGI("OpenGL context released after graphics resource initialization");
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
    if (engineInitialized) {
        //LOGI("Starting render frame");

        // Update engine and game manager (same as game's main.cpp)
        //LOGI("Calling Engine::Update()");
        Engine::Update();

        //LOGI("Calling GameManager::Update()");
        GameManager::Update();

        // Draw frame (same as game's main.cpp)
        //LOGI("Calling Engine::StartDraw()");
        Engine::StartDraw();

        //LOGI("Calling Engine::Draw()");
        Engine::Draw();

        //LOGI("Calling Engine::EndDraw()");
        Engine::EndDraw();

        //LOGI("Render frame completed");
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

extern "C" JNIEXPORT void JNICALL
Java_com_gam300_game_MainActivity_onTouchEvent(JNIEnv* env, jobject /* this */, jint action, jfloat x, jfloat y) {
    if (engineInitialized) {
        IPlatform* platform = WindowManager::GetPlatform();
        if (platform) {
            AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
            androidPlatform->HandleTouchEvent(static_cast<int>(action), static_cast<float>(x), static_cast<float>(y));
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_gam300_game_MainActivity_onKeyEvent(JNIEnv* env, jobject /* this */, jint keyCode, jint action) {
    if (engineInitialized) {
        IPlatform* platform = WindowManager::GetPlatform();
        if (platform) {
            AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
            androidPlatform->HandleKeyEvent(static_cast<int>(keyCode), static_cast<int>(action));
        }
    }
}