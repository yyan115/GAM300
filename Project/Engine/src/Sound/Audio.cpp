#include "pch.h"
#include "Sound/Audio.hpp"
#include <Asset Manager/AssetManager.hpp>
#include "Sound/AudioManager.hpp"
#include "Logging.hpp"
#include <filesystem>
#include <fstream>
#include "Utilities/FileUtilities.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif

std::string Audio::CompileToResource(const std::string& assetPathParam, bool forAndroid) {
    if (!std::filesystem::exists(assetPathParam)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] File does not exist: ", assetPathParam, "\n");
        return std::string{};
    }

    if (std::filesystem::is_directory(assetPathParam)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Path is a directory, not a file: ", assetPathParam, "\n");
        return std::string{};
    }

    // For audio files, we copy them directly since FMOD handles various formats natively
    // Unlike fonts which compile to .font files, audio keeps original extension
    std::filesystem::path p(assetPathParam);
    std::string outPath{};
    
    if (!forAndroid) {
        // For desktop, copy to Resources folder maintaining structure and original extension
        outPath = p.generic_string(); // Keep the original path - no need to change it
    }
    else {
        // For Android, copy to android resources path
        std::string assetPathAndroid = assetPathParam.substr(assetPathParam.find("Resources"));
        outPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string();
    }

    // For desktop builds, we don't need to copy the file since it's already in Resources
    // For Android, we need to copy it to the Android assets folder
    if (forAndroid) {
        // Ensure parent directories exist
        std::filesystem::path outputPath(outPath);
        std::filesystem::create_directories(outputPath.parent_path());

        try {
            // Copy the audio file to the Android assets location
            std::filesystem::copy_file(assetPathParam, outPath, std::filesystem::copy_options::overwrite_existing);
        }
        catch (const std::filesystem::filesystem_error& e) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Failed to copy audio file for Android: ", e.what(), "\n");
            return std::string{};
        }
    }

    ENGINE_PRINT("[Audio] Successfully compiled audio resource: ", assetPathParam, " to ", outPath, "\n");
    return outPath;
}

bool Audio::LoadResource(const std::string& resourcePath, const std::string& assetPathParam) {
    // Store paths
    this->assetPath = assetPathParam.empty() ? resourcePath : assetPathParam;

    // Clean up existing sound if any
    if (sound) {
        AudioManager::GetInstance().ReleaseSound(sound, this->assetPath);
        sound = nullptr;
    }

    AudioManager& audioSys = AudioManager::GetInstance();

    // NEW: Use platform abstraction to read asset data (works on Android via AndroidPlatform::ReadAsset)
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] ERROR: Platform not available for asset loading.\n");
        return false;
    }

    std::vector<uint8_t> audioData = platform->ReadAsset(resourcePath);
    if (audioData.empty()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] ERROR: Failed to read asset data for: ", resourcePath, "\n");
        return false;
    }

    // Create sound from memory (FMOD handles it)
    sound = audioSys.CreateSoundFromMemory(audioData.data(), static_cast<unsigned int>(audioData.size()), this->assetPath);
    if (!sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Audio] Failed to create FMOD sound for: ", resourcePath, "\n");
        return false;
    }

    ENGINE_PRINT("[Audio] Successfully loaded: ", resourcePath, "\n");
    return true;
}

bool Audio::ReloadResource(const std::string& resourcePath, const std::string& assetPathParam) {
    // Release existing sound if any
    if (sound && !this->assetPath.empty()) {
        AudioManager::GetInstance().ReleaseSound(sound, this->assetPath);
        sound = nullptr;
    }

    // Load new resource
    return LoadResource(resourcePath, assetPathParam);
}

std::shared_ptr<AssetMeta> Audio::ExtendMetaFile(const std::string& assetPathParam, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid) {
    (void)assetPathParam; // Suppress unused parameter warning
    (void)forAndroid; // Suppress unused parameter warning
    // Audio files don't need extended metadata beyond the base AssetMeta
    // Could potentially add audio-specific metadata like:
    // - Duration
    // - Sample rate  
    // - Channel count
    // - Compression info
    // But for now, base metadata is sufficient
    return currentMetaData;
}
