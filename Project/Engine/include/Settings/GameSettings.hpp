#pragma once

#include <string>
#include <mutex>

// GameSettingsData - contains all persistent game settings
struct GameSettingsData {
    // Audio settings (0.0 - 1.0)
    float masterVolume = 1.0f;
    float bgmVolume = 1.0f;
    float sfxVolume = 1.0f;

    // Graphics settings - Basic
    float gamma = 2.2f;      // 1.0 - 3.0 range (2.2 = standard gamma)
    float exposure = 1.0f;   // 0.1 - 5.0 range
    int toneMappingMode = 2; // 0: Reinhard, 1: Exposure, 2: ACES
    bool vsync = true;
    bool fullscreen = false;

    // Bloom
    bool bloomEnabled = true;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 1.0f;
    float bloomScatter = 0.5f;

    // Vignette
    bool vignetteEnabled = false;
    float vignetteIntensity = 0.5f;
    float vignetteSmoothness = 0.5f;
    float vignetteColor[3] = { 0.0f, 0.0f, 0.0f };

    // Color Grading
    bool colorGradingEnabled = false;
    float cgBrightness = 0.0f;
    float cgContrast = 1.0f;
    float cgSaturation = 1.0f;
    float cgTint[3] = { 1.0f, 1.0f, 1.0f };

    // Chromatic Aberration
    bool caEnabled = false;
    float caIntensity = 0.5f;
    float caPadding = 0.5f;

    // SSAO
    bool ssaoEnabled = true;
    float ssaoRadius = 0.5f;
    float ssaoBias = 0.025f;
    float ssaoIntensity = 1.0f;
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
    void SetToneMappingMode(int mode);
    void SetVSync(bool enabled);
    void SetFullscreen(bool enabled);

    // Bloom
    void SetBloomEnabled(bool enabled);
    void SetBloomThreshold(float threshold);
    void SetBloomIntensity(float intensity);
    void SetBloomScatter(float scatter);

    // Vignette
    void SetVignetteEnabled(bool enabled);
    void SetVignetteIntensity(float intensity);
    void SetVignetteSmoothness(float smoothness);
    void SetVignetteColor(float r, float g, float b);

    // Color Grading
    void SetColorGradingEnabled(bool enabled);
    void SetCGBrightness(float brightness);
    void SetCGContrast(float contrast);
    void SetCGSaturation(float saturation);
    void SetCGTint(float r, float g, float b);

    // Chromatic Aberration
    void SetCAEnabled(bool enabled);
    void SetCAIntensity(float intensity);
    void SetCAPadding(float padding);

    // SSAO
    void SetSSAOEnabled(bool enabled);
    void SetSSAORadius(float radius);
    void SetSSAOBias(float bias);
    void SetSSAOIntensity(float intensity);

    // Getters (thread-safe)
    float GetMasterVolume() const;
    float GetBGMVolume() const;
    float GetSFXVolume() const;
    float GetGamma() const;
    float GetExposure() const;
    int GetToneMappingMode() const;
    bool GetVSync() const;
    bool GetFullscreen() const;

    // Get default values
    float GetDefaultMasterVolume() { return 1.0f; }
    float GetDefaultBGMVolume() { return 1.0f; }
    float GetDefaultSFXVolume() { return 1.0f; }
    float GetDefaultGamma() { return 2.2f; }
    float GetDefaultExposure() { return 1.0f; }

    // Get the current settings data (read-only)
    const GameSettingsData& GetSettings() const { return m_settings; }

    // Get mutable settings (use with caution, call MarkDirty() after changes)
    GameSettingsData& GetSettingsMutable() { return m_settings; }
    void MarkDirty();

    // Check if settings have unsaved changes
    bool IsDirty() const { return m_dirty; }

private:
    GameSettingsManager() = default;
    ~GameSettingsManager() = default;

    // Non-copyable
    GameSettingsManager(const GameSettingsManager&) = delete;
    GameSettingsManager& operator=(const GameSettingsManager&) = delete;

    // Settings file path
    std::string GetSettingsFilePath() const;

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
