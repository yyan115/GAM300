#include "pch.h"
#include "Settings/GameSettings.hpp"
#include "Logging.hpp"
#include "Sound/AudioManager.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <fstream>
#include <filesystem>
#include <algorithm>

GameSettingsManager& GameSettingsManager::GetInstance() {
    static GameSettingsManager instance;
    return instance;
}

void GameSettingsManager::Initialize() {
    if (m_initialized) return;

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[GameSettings] Initializing...");

    // Set defaults
    m_defaults.masterVolume = GetDefaultMasterVolume();
    m_defaults.bgmVolume = GetDefaultBGMVolume();
    m_defaults.sfxVolume = GetDefaultSFXVolume();
    m_defaults.gamma = GetDefaultGamma();
    m_defaults.exposure = GetDefaultExposure();

    // Copy defaults to current settings
    m_settings = m_defaults;

    // Try to load saved settings (no disk I/O if file doesn't exist)
    LoadSettings();

    // Apply settings to engine systems
    ApplySettings();

    m_initialized = true;
    m_dirty = false; // Just loaded, not dirty

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[GameSettings] Initialized");
}

void GameSettingsManager::Shutdown() {
    if (!m_initialized) return;

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[GameSettings] Shutting down...");

    // Save settings if there are unsaved changes
    SaveIfDirty();

    m_initialized = false;
}

std::string GameSettingsManager::GetSettingsFilePath() const {
    namespace fs = std::filesystem;

    // Get Resources folder relative to executable
    // For Game builds: Build/[Config]/Resources/GameSettings.json
    // For Editor builds: Build/EditorRelease/Resources/GameSettings.json
    fs::path exePath = fs::current_path();
    fs::path settingsPath = exePath / "Resources" / SETTINGS_FILENAME;

    return settingsPath.string();
}

bool GameSettingsManager::LoadSettings() {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = GetSettingsFilePath();

    // Check if file exists (avoid exception overhead)
    if (!fs::exists(filePath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "[GameSettings] No saved settings found, using defaults");
        return false;
    }

    // Read file
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GameSettings] Failed to open file: ", filePath);
        return false;
    }

    // Read entire file into string
    std::string jsonContent((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    // Parse JSON
    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());

    if (doc.HasParseError()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GameSettings] JSON parse error in: ", filePath);
        return false;
    }

    // Load and clamp audio settings
    if (doc.HasMember("masterVolume") && doc["masterVolume"].IsNumber()) {
        m_settings.masterVolume = std::clamp(doc["masterVolume"].GetFloat(), 0.0f, 1.0f);
    }
    if (doc.HasMember("bgmVolume") && doc["bgmVolume"].IsNumber()) {
        m_settings.bgmVolume = std::clamp(doc["bgmVolume"].GetFloat(), 0.0f, 1.0f);
    }
    if (doc.HasMember("sfxVolume") && doc["sfxVolume"].IsNumber()) {
        m_settings.sfxVolume = std::clamp(doc["sfxVolume"].GetFloat(), 0.0f, 1.0f);
    }

    // Load and clamp graphics settings
    if (doc.HasMember("gamma") && doc["gamma"].IsNumber()) {
        m_settings.gamma = std::clamp(doc["gamma"].GetFloat(), 1.0f, 3.0f);
    }
    if (doc.HasMember("exposure") && doc["exposure"].IsNumber()) {
        m_settings.exposure = std::clamp(doc["exposure"].GetFloat(), 0.1f, 5.0f);
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[GameSettings] Loaded settings from: ", filePath);
    return true;
}

bool GameSettingsManager::SaveSettings() {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = GetSettingsFilePath();

    // Create parent directory if needed
    fs::path fullPath(filePath);
    fs::path parentDir = fullPath.parent_path();
    if (!fs::exists(parentDir)) {
        try {
            fs::create_directories(parentDir);
        } catch (const std::exception& e) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GameSettings] Failed to create directory: ", parentDir.string());
            return false;
        }
    }

    // Build JSON document
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();

    // Serialize audio settings
    doc.AddMember("masterVolume", m_settings.masterVolume, alloc);
    doc.AddMember("bgmVolume", m_settings.bgmVolume, alloc);
    doc.AddMember("sfxVolume", m_settings.sfxVolume, alloc);

    // Serialize graphics settings
    doc.AddMember("gamma", m_settings.gamma, alloc);
    doc.AddMember("exposure", m_settings.exposure, alloc);

    // Write to file
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GameSettings] Failed to open file for writing: ", filePath);
        return false;
    }

    outFile << buffer.GetString();
    outFile.close();

    // Clear dirty flag after successful save
    m_dirty = false;

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[GameSettings] Saved settings to: ", filePath);
    return true;
}

bool GameSettingsManager::SaveIfDirty() {
    if (!m_dirty) {
        return true; // Nothing to save
    }
    return SaveSettings();
}

void GameSettingsManager::ResetToDefaults() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_settings = m_defaults;
        m_dirty = true;
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[GameSettings] Reset to defaults");

    // Apply immediately
    ApplySettings();

    // Save changes
    SaveSettings();
}

void GameSettingsManager::ApplySettings() {
    // Apply audio settings
    AudioManager::GetInstance().SetMasterVolume(m_settings.masterVolume);
    AudioManager::GetInstance().SetBusVolume("BGM", m_settings.bgmVolume);
    AudioManager::GetInstance().SetBusVolume("SFX", m_settings.sfxVolume);

    // Apply graphics settings
    auto* hdrEffect = PostProcessingManager::GetInstance().GetHDREffect();
    if (hdrEffect) {
        hdrEffect->SetGamma(m_settings.gamma);
        hdrEffect->SetExposure(m_settings.exposure);
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[GameSettings] Applied settings");
}

void GameSettingsManager::MarkDirty() {
    m_dirty = true;
}

// Individual setters - mark dirty but DO NOT auto-save
// This prevents disk I/O spam during slider dragging
void GameSettingsManager::SetMasterVolume(float volume) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        float newValue = std::clamp(volume, 0.0f, 1.0f);
        if (m_settings.masterVolume != newValue) {
            m_settings.masterVolume = newValue;
            MarkDirty();
        }
    }
    AudioManager::GetInstance().SetMasterVolume(m_settings.masterVolume);
}

void GameSettingsManager::SetBGMVolume(float volume) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        float newValue = std::clamp(volume, 0.0f, 1.0f);
        if (m_settings.bgmVolume != newValue) {
            m_settings.bgmVolume = newValue;
            MarkDirty();
        }
    }
    AudioManager::GetInstance().SetBusVolume("BGM", m_settings.bgmVolume);
}

void GameSettingsManager::SetSFXVolume(float volume) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        float newValue = std::clamp(volume, 0.0f, 1.0f);
        if (m_settings.sfxVolume != newValue) {
            m_settings.sfxVolume = newValue;
            MarkDirty();
        }
    }
    AudioManager::GetInstance().SetBusVolume("SFX", m_settings.sfxVolume);
}

void GameSettingsManager::SetGamma(float gamma) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        float newValue = std::clamp(gamma, 1.0f, 3.0f);
        if (m_settings.gamma != newValue) {
            m_settings.gamma = newValue;
            MarkDirty();
        }
    }
    auto* hdrEffect = PostProcessingManager::GetInstance().GetHDREffect();
    if (hdrEffect) {
        hdrEffect->SetGamma(m_settings.gamma);
    }
}

void GameSettingsManager::SetExposure(float exposure) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        float newValue = std::clamp(exposure, 0.1f, 5.0f);
        if (m_settings.exposure != newValue) {
            m_settings.exposure = newValue;
            MarkDirty();
        }
    }
    auto* hdrEffect = PostProcessingManager::GetInstance().GetHDREffect();
    if (hdrEffect) {
        hdrEffect->SetExposure(m_settings.exposure);
    }
}

// Getters - thread-safe
float GameSettingsManager::GetMasterVolume() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.masterVolume;
}

float GameSettingsManager::GetBGMVolume() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.bgmVolume;
}

float GameSettingsManager::GetSFXVolume() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.sfxVolume;
}

float GameSettingsManager::GetGamma() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.gamma;
}

float GameSettingsManager::GetExposure() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.exposure;
}
