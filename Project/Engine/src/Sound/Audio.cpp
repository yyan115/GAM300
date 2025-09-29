#include "pch.h"
#include "Sound/Audio.hpp"
#include <Asset Manager/AssetManager.hpp>
#include "Sound/AudioManager.hpp"
#include "Logging.hpp"
#include <filesystem>
#include <fstream>
#include "Utilities/FileUtilities.hpp"

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif

std::string Audio::CompileToResource(const std::string& assetPath, bool forAndroid) {
    if (!std::filesystem::exists(assetPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] File does not exist: ", assetPath, "\n");
        return std::string{};
    }

    if (std::filesystem::is_directory(assetPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Path is a directory, not a file: ", assetPath, "\n");
        return std::string{};
    }

    // For audio files, we copy them directly since FMOD handles various formats natively
    // Unlike fonts which compile to .font files, audio keeps original extension
    std::filesystem::path p(assetPath);
    std::string outPath{};
    
    if (!forAndroid) {
        // For desktop, copy to Resources folder maintaining structure and original extension
        outPath = p.generic_string(); // Keep the original path - no need to change it
    }
    else {
        // For Android, copy to android resources path
        outPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / p).generic_string();
    }

    // For desktop builds, we don't need to copy the file since it's already in Resources
    // For Android, we need to copy it to the Android assets folder
    if (forAndroid) {
        // Ensure parent directories exist
        std::filesystem::path outputPath(outPath);
        std::filesystem::create_directories(outputPath.parent_path());

        try {
            // Copy the audio file to the Android assets location
            std::filesystem::copy_file(assetPath, outPath, std::filesystem::copy_options::overwrite_existing);
        }
        catch (const std::filesystem::filesystem_error& e) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Failed to copy audio file for Android: ", e.what(), "\n");
            return std::string{};
        }
    }

    ENGINE_PRINT("[Audio] Successfully compiled audio resource: ", assetPath, " to ", outPath, "\n");
    return outPath;
}

bool Audio::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
    // Store paths
    this->assetPath = assetPath.empty() ? resourcePath : assetPath;

    // Clean up existing sound if any
    if (sound) {
        AudioManager::GetInstance().ReleaseSound(sound, this->assetPath);
        sound = nullptr;
    }

    AudioManager& audioSys = AudioManager::GetInstance();

#ifdef ANDROID
    // On Android, try to load from APK assets first using platform abstraction
    IPlatform* platform = WindowManager::GetPlatform();
    if (platform) {
        AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
        AAssetManager* assetMgr = androidPlatform->GetAssetManager();
        if (assetMgr) {
            AAsset* asset = AAssetManager_open(assetMgr, resourcePath.c_str(), AASSET_MODE_BUFFER);
            if (asset) {
                off_t length = AAsset_getLength(asset);
                const void* buffer = AAsset_getBuffer(asset);
                if (buffer && length > 0) {
                    sound = audioSys.CreateSoundFromMemory(buffer, static_cast<unsigned int>(length), this->assetPath);
                }
                AAsset_close(asset);

                if (sound) {
                    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Audio] Successfully loaded audio from APK memory: %s", resourcePath.c_str());
                    ENGINE_PRINT("[Audio] Successfully loaded (memory): ", resourcePath, "\n");
                    return true;
                }
            } else {
                __android_log_print(ANDROID_LOG_WARN, "GAM300", "[Audio] AAssetManager_open failed for: %s", resourcePath.c_str());
            }
        }
    }
#endif
    // Resolve path - check multiple locations to ensure the file is found
    std::string pathToUse = resourcePath;

    try {
        // If the path doesn't exist as-is, try solution root + resourcePath
        if (!std::filesystem::exists(pathToUse)) {
            std::filesystem::path candidate;

            // Try solution root path + resourcePath
            const auto& solRoot = FileUtilities::GetSolutionRootDir();
            if (!solRoot.empty()) {
                candidate = solRoot / resourcePath;
                if (std::filesystem::exists(candidate)) {
                    pathToUse = candidate.generic_string();
                    ENGINE_PRINT("[Audio] Resolved resource via solution root: ", pathToUse, "\n");
                }
            }

            // If still not found, try exe directory + resourcePath
            if (!std::filesystem::exists(pathToUse)) {
                candidate = std::filesystem::current_path() / resourcePath;
                if (std::filesystem::exists(candidate)) {
                    pathToUse = candidate.generic_string();
                    ENGINE_PRINT("[Audio] Resolved resource via current_path: ", pathToUse, "\n");
                }
            }

            if (!std::filesystem::exists(pathToUse)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Resource not found at any location: ", resourcePath, "\n");
            }
        }
    }
    catch (const std::exception& ex) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Path resolution error: ", ex.what(), "\n");
    }

    // Use the resolved path with FMOD
    sound = audioSys.CreateSound(pathToUse);
    if (!sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Failed to create FMOD sound for: ", pathToUse, "\n");
        return false;
    }

    ENGINE_PRINT("[Audio] Successfully loaded: ", pathToUse, "\n");
    return true;
}

bool Audio::ReloadResource(const std::string& resourcePath, const std::string& assetPath) {
    // Release existing sound if any
    if (sound && !this->assetPath.empty()) {
        AudioManager::GetInstance().ReleaseSound(sound, this->assetPath);
        sound = nullptr;
    }

    // Load new resource
    return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Audio::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid) {
    // Audio files don't need extended metadata beyond the base AssetMeta
    // Could potentially add audio-specific metadata like:
    // - Duration
    // - Sample rate  
    // - Channel count
    // - Compression info
    // But for now, base metadata is sufficient
    return currentMetaData;
}
