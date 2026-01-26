#pragma once

#include <string>
#include <mutex>

// GameSettingsData - contains all persistent game settings
struct GameSettingsData {
    // Audio settings (0.0 - 1.0)
    float masterVolume = 1.0f;
    float bgmVolume = 1.0f;
    float sfxVolume = 1.0f;

    // Graphics settings
    float gamma = 2.2f;      // 1.0 - 3.0 range (2.2 = standard gamma)
    float exposure = 1.0f;   // 0.1 - 5.0 range
};

// GameSettingsManager - singleton manager for persistent game settings
// Handles JSON serialization/deserialization and applies settings to engine systems
//
// Performance optimization: Settings are kept in memory and marked as "dirty" when modified.
// Call Save() explicitly at appropriate times (scene transitions, menu close) rather than
// spamming disk I/O on every slider drag.
class GameSettingsManager {
public:
    static GameSettingsManager& GetInstance();

    // Initialization - call once at game startup
    void Initialize();
    void Shutdown();

    // Load/Save settings from/to JSON file
    // Load is called automatically during Initialize()
    bool LoadSettings();

    // Save settings to disk - call this at appropriate times:
    // - When closing settings menu
    // - On scene transitions
    // - On game shutdown
    // DO NOT call this on every slider drag!
    bool SaveSettings();

    // Save if dirty (optimization - only writes if settings changed)
    bool SaveIfDirty();

    // Reset all settings to defaults
    void ResetToDefaults();

    // Apply current settings to engine systems (Audio, Graphics)
    void ApplySettings();

    // Individual setters (marks as dirty, does NOT auto-save)
    // Call SaveSettings() or SaveIfDirty() when appropriate
    void SetMasterVolume(float volume);
    void SetBGMVolume(float volume);
    void SetSFXVolume(float volume);
    void SetGamma(float gamma);
    void SetExposure(float exposure);

    // Getters (thread-safe)
    float GetMasterVolume() const;
    float GetBGMVolume() const;
    float GetSFXVolume() const;
    float GetGamma() const;
    float GetExposure() const;

    // Get default values (static for Lua bindings)
    static float GetDefaultMasterVolume() { return 1.0f; }
    static float GetDefaultBGMVolume() { return 1.0f; }
    static float GetDefaultSFXVolume() { return 1.0f; }
    static float GetDefaultGamma() { return 2.2f; }
    static float GetDefaultExposure() { return 1.0f; }

    // Check if settings have unsaved changes
    bool IsDirty() const { return m_dirty; }

    // Get the current settings data (read-only)
    const GameSettingsData& GetSettings() const { return m_settings; }

private:
    GameSettingsManager() = default;
    ~GameSettingsManager() = default;

    // Non-copyable
    GameSettingsManager(const GameSettingsManager&) = delete;
    GameSettingsManager& operator=(const GameSettingsManager&) = delete;

    // Settings file path
    std::string GetSettingsFilePath() const;

    // Mark settings as modified
    void MarkDirty();

    // Current settings
    GameSettingsData m_settings;
    GameSettingsData m_defaults;

    // Thread safety
    mutable std::mutex m_mutex;

    // Dirty flag - true if settings changed since last save
    bool m_dirty = false;

    // Initialization flag
    bool m_initialized = false;

    // Settings file name
    static constexpr const char* SETTINGS_FILENAME = "GameSettings.json";
};
