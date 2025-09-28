#include "pch.h"
#include "Sound/Audio.hpp"
#include "Sound/AudioSystem.hpp"
#include "Logging.hpp"

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif

std::string Audio::CompileToResource(const std::string& assetPath) {
    // For audio files, no compilation needed - return original path
    // FMOD handles various formats directly
    return assetPath;
}

bool Audio::LoadResource(const std::string& assetPath) {
    this->assetPath = assetPath;

    AudioSystem& audioSys = AudioSystem::GetInstance();

#ifdef ANDROID
    // On Android the AudioSystem must be initialized after the Android asset manager is set.
    // Assume platform code has initialized AudioSystem appropriately.
#endif

#ifdef ANDROID
    // Try to load asset bytes from APK via AndroidPlatform asset manager and create sound from memory
    auto* platform = WindowManager::GetPlatform();
    if (platform) {
        AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
        AAssetManager* assetMgr = androidPlatform->GetAssetManager();
        if (assetMgr) {
            AAsset* asset = AAssetManager_open(assetMgr, assetPath.c_str(), AASSET_MODE_BUFFER);
            if (asset) {
                off_t length = AAsset_getLength(asset);
                const void* buffer = AAsset_getBuffer(asset);
                if (buffer && length > 0) {
                    sound = audioSys.CreateSoundFromMemory(buffer, static_cast<unsigned int>(length), assetPath);
                }
                AAsset_close(asset);

                if (sound) {
                    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Audio] Successfully loaded audio from APK memory: %s", assetPath.c_str());
                    ENGINE_PRINT("[Audio] Successfully loaded (memory): ", assetPath, "\n");
                    return true;
                }
            } else {
                __android_log_print(ANDROID_LOG_WARN, "GAM300", "[Audio] AAssetManager_open failed for: %s", assetPath.c_str());
            }
        }
    }
#endif

    // Fallback: Use AudioSystem to create FMOD sound with platform-specific loading
    sound = audioSys.CreateSound(assetPath);
    if (!sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Failed to create FMOD sound for: ", assetPath, "\n");
        return false;
    }

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Audio] Successfully loaded audio: %s", assetPath.c_str());
#endif

    ENGINE_PRINT("[Audio] Successfully loaded: ", assetPath, "\n");
    return true;
}

bool Audio::ReloadResource(const std::string& assetPath) {
    // Release existing sound if any
    if (sound && !this->assetPath.empty()) {
        AudioSystem::GetInstance().ReleaseSound(sound, this->assetPath);
        sound = nullptr;
    }

    // Load new resource
    return LoadResource(assetPath);
}

std::shared_ptr<AssetMeta> Audio::ExtendMetaFile(const std::string& /*assetPath*/, std::shared_ptr<AssetMeta> currentMetaData) {
    // Audio files don't need extended metadata beyond the base AssetMeta
    // Could potentially add audio-specific metadata like:
    // - Duration
    // - Sample rate  
    // - Channel count
    // - Compression info
    // But for now, base metadata is sufficient
    return currentMetaData;
}