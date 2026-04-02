#include "pch.h"
#include "Settings/GameSettings.hpp"
#include "Logging.hpp"
#include "Sound/AudioManager.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "WindowManager.hpp"

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
    m_defaults.masterVolume = 1.0f;
    m_defaults.bgmVolume = 1.0f;
    m_defaults.sfxVolume = 1.0f;
    m_defaults.gamma = 2.2f;
    m_defaults.exposure = 1.0f;
    m_defaults.toneMappingMode = 2;
    m_defaults.vsync = true;
    m_defaults.fullscreen = false;
    m_defaults.bloomEnabled = true;
    m_defaults.bloomThreshold = 1.0f;
    m_defaults.bloomIntensity = 1.0f;
    m_defaults.bloomScatter = 0.5f;
    m_defaults.ssaoEnabled = true;

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
    fs::path exePath = fs::current_path();
    fs::path settingsPath = exePath / "Resources" / SETTINGS_FILENAME;
    return settingsPath.string();
}

bool GameSettingsManager::LoadSettings() {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = GetSettingsFilePath();
    if (!fs::exists(filePath)) return false;

    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) return false;

    std::string jsonContent((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());
    if (doc.HasParseError()) return false;

    // Audio
    if (doc.HasMember("masterVolume") && doc["masterVolume"].IsNumber()) m_settings.masterVolume = std::clamp(doc["masterVolume"].GetFloat(), 0.0f, 1.0f);
    if (doc.HasMember("bgmVolume") && doc["bgmVolume"].IsNumber()) m_settings.bgmVolume = std::clamp(doc["bgmVolume"].GetFloat(), 0.0f, 1.0f);
    if (doc.HasMember("sfxVolume") && doc["sfxVolume"].IsNumber()) m_settings.sfxVolume = std::clamp(doc["sfxVolume"].GetFloat(), 0.0f, 1.0f);

    // Graphics - Basic
    if (doc.HasMember("gamma") && doc["gamma"].IsNumber()) m_settings.gamma = std::clamp(doc["gamma"].GetFloat(), 1.0f, 3.0f);
    if (doc.HasMember("exposure") && doc["exposure"].IsNumber()) m_settings.exposure = std::clamp(doc["exposure"].GetFloat(), 0.1f, 5.0f);
    if (doc.HasMember("toneMappingMode") && doc["toneMappingMode"].IsInt()) m_settings.toneMappingMode = std::clamp(doc["toneMappingMode"].GetInt(), 0, 2);
    if (doc.HasMember("vsync") && doc["vsync"].IsBool()) m_settings.vsync = doc["vsync"].GetBool();
    if (doc.HasMember("fullscreen") && doc["fullscreen"].IsBool()) m_settings.fullscreen = doc["fullscreen"].GetBool();

    // Bloom
    if (doc.HasMember("bloomEnabled") && doc["bloomEnabled"].IsBool()) m_settings.bloomEnabled = doc["bloomEnabled"].GetBool();
    if (doc.HasMember("bloomThreshold") && doc["bloomThreshold"].IsNumber()) m_settings.bloomThreshold = doc["bloomThreshold"].GetFloat();
    if (doc.HasMember("bloomIntensity") && doc["bloomIntensity"].IsNumber()) m_settings.bloomIntensity = doc["bloomIntensity"].GetFloat();
    if (doc.HasMember("bloomScatter") && doc["bloomScatter"].IsNumber()) m_settings.bloomScatter = doc["bloomScatter"].GetFloat();

    // Vignette
    if (doc.HasMember("vignetteEnabled") && doc["vignetteEnabled"].IsBool()) m_settings.vignetteEnabled = doc["vignetteEnabled"].GetBool();
    if (doc.HasMember("vignetteIntensity") && doc["vignetteIntensity"].IsNumber()) m_settings.vignetteIntensity = doc["vignetteIntensity"].GetFloat();
    if (doc.HasMember("vignetteSmoothness") && doc["vignetteSmoothness"].IsNumber()) m_settings.vignetteSmoothness = doc["vignetteSmoothness"].GetFloat();
    if (doc.HasMember("vignetteColor") && doc["vignetteColor"].IsArray() && doc["vignetteColor"].Size() == 3) {
        for (int i = 0; i < 3; ++i) m_settings.vignetteColor[i] = doc["vignetteColor"][i].GetFloat();
    }

    // Color Grading
    if (doc.HasMember("colorGradingEnabled") && doc["colorGradingEnabled"].IsBool()) m_settings.colorGradingEnabled = doc["colorGradingEnabled"].GetBool();
    if (doc.HasMember("cgBrightness") && doc["cgBrightness"].IsNumber()) m_settings.cgBrightness = doc["cgBrightness"].GetFloat();
    if (doc.HasMember("cgContrast") && doc["cgContrast"].IsNumber()) m_settings.cgContrast = doc["cgContrast"].GetFloat();
    if (doc.HasMember("cgSaturation") && doc["cgSaturation"].IsNumber()) m_settings.cgSaturation = doc["cgSaturation"].GetFloat();
    if (doc.HasMember("cgTint") && doc["cgTint"].IsArray() && doc["cgTint"].Size() == 3) {
        for (int i = 0; i < 3; ++i) m_settings.cgTint[i] = doc["cgTint"][i].GetFloat();
    }

    // Chromatic Aberration
    if (doc.HasMember("caEnabled") && doc["caEnabled"].IsBool()) m_settings.caEnabled = doc["caEnabled"].GetBool();
    if (doc.HasMember("caIntensity") && doc["caIntensity"].IsNumber()) m_settings.caIntensity = doc["caIntensity"].GetFloat();
    if (doc.HasMember("caPadding") && doc["caPadding"].IsNumber()) m_settings.caPadding = doc["caPadding"].GetFloat();

    // SSAO
    if (doc.HasMember("ssaoEnabled") && doc["ssaoEnabled"].IsBool()) m_settings.ssaoEnabled = doc["ssaoEnabled"].GetBool();
    if (doc.HasMember("ssaoRadius") && doc["ssaoRadius"].IsNumber()) m_settings.ssaoRadius = doc["ssaoRadius"].GetFloat();
    if (doc.HasMember("ssaoBias") && doc["ssaoBias"].IsNumber()) m_settings.ssaoBias = doc["ssaoBias"].GetFloat();
    if (doc.HasMember("ssaoIntensity") && doc["ssaoIntensity"].IsNumber()) m_settings.ssaoIntensity = doc["ssaoIntensity"].GetFloat();

    return true;
}

bool GameSettingsManager::SaveSettings() {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = GetSettingsFilePath();
    fs::create_directories(fs::path(filePath).parent_path());

    rapidjson::Document doc;
    doc.SetObject();
    auto& a = doc.GetAllocator();

    doc.AddMember("masterVolume", m_settings.masterVolume, a);
    doc.AddMember("bgmVolume", m_settings.bgmVolume, a);
    doc.AddMember("sfxVolume", m_settings.sfxVolume, a);
    doc.AddMember("gamma", m_settings.gamma, a);
    doc.AddMember("exposure", m_settings.exposure, a);
    doc.AddMember("toneMappingMode", m_settings.toneMappingMode, a);
    doc.AddMember("vsync", m_settings.vsync, a);
    doc.AddMember("fullscreen", m_settings.fullscreen, a);
    doc.AddMember("bloomEnabled", m_settings.bloomEnabled, a);
    doc.AddMember("bloomThreshold", m_settings.bloomThreshold, a);
    doc.AddMember("bloomIntensity", m_settings.bloomIntensity, a);
    doc.AddMember("bloomScatter", m_settings.bloomScatter, a);
    doc.AddMember("vignetteEnabled", m_settings.vignetteEnabled, a);
    doc.AddMember("vignetteIntensity", m_settings.vignetteIntensity, a);
    doc.AddMember("vignetteSmoothness", m_settings.vignetteSmoothness, a);
    rapidjson::Value vColor(rapidjson::kArrayType);
    for (int i = 0; i < 3; ++i) vColor.PushBack(m_settings.vignetteColor[i], a);
    doc.AddMember("vignetteColor", vColor, a);
    doc.AddMember("colorGradingEnabled", m_settings.colorGradingEnabled, a);
    doc.AddMember("cgBrightness", m_settings.cgBrightness, a);
    doc.AddMember("cgContrast", m_settings.cgContrast, a);
    doc.AddMember("cgSaturation", m_settings.cgSaturation, a);
    rapidjson::Value cTint(rapidjson::kArrayType);
    for (int i = 0; i < 3; ++i) cTint.PushBack(m_settings.cgTint[i], a);
    doc.AddMember("cgTint", cTint, a);
    doc.AddMember("caEnabled", m_settings.caEnabled, a);
    doc.AddMember("caIntensity", m_settings.caIntensity, a);
    doc.AddMember("caPadding", m_settings.caPadding, a);
    doc.AddMember("ssaoEnabled", m_settings.ssaoEnabled, a);
    doc.AddMember("ssaoRadius", m_settings.ssaoRadius, a);
    doc.AddMember("ssaoBias", m_settings.ssaoBias, a);
    doc.AddMember("ssaoIntensity", m_settings.ssaoIntensity, a);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) return false;
    outFile << buffer.GetString();
    outFile.close();

    m_dirty = false;
    return true;
}

bool GameSettingsManager::SaveIfDirty() {
    if (!m_dirty) return true;
    return SaveSettings();
}

void GameSettingsManager::ResetToDefaults() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_settings = m_defaults;
        m_dirty = true;
    }
    ApplySettings();
    SaveSettings();
}

void GameSettingsManager::ApplySettings() {
    // Audio
    AudioManager::GetInstance().SetMasterVolume(m_settings.masterVolume);
    AudioManager::GetInstance().SetBusVolume("BGM", m_settings.bgmVolume);
    AudioManager::GetInstance().SetBusVolume("SFX", m_settings.sfxVolume);
    AudioManager::GetInstance().SetBusVolume("UI", m_settings.sfxVolume);

    // Window
    WindowManager::SetVSync(m_settings.vsync);
    WindowManager::SetFullscreen(m_settings.fullscreen);

    /*
    // Graphics - Commented out to allow CameraComponent/Editor to have full control
    auto& pm = PostProcessingManager::GetInstance();
    auto* hdr = pm.GetHDREffect();
    if (hdr) {
        hdr->SetGamma(m_settings.gamma);
        hdr->SetExposure(m_settings.exposure);
        hdr->SetToneMappingMode(static_cast<HDREffect::ToneMappingMode>(m_settings.toneMappingMode));
        
        hdr->SetVignetteEnabled(m_settings.vignetteEnabled);
        hdr->SetVignetteIntensity(m_settings.vignetteIntensity);
        hdr->SetVignetteSmoothness(m_settings.vignetteSmoothness);
        hdr->SetVignetteColor({m_settings.vignetteColor[0], m_settings.vignetteColor[1], m_settings.vignetteColor[2]});
        
        hdr->SetColorGradingEnabled(m_settings.colorGradingEnabled);
        hdr->SetCGBrightness(m_settings.cgBrightness);
        hdr->SetCGContrast(m_settings.cgContrast);
        hdr->SetCGSaturation(m_settings.cgSaturation);
        hdr->SetCGTint({m_settings.cgTint[0], m_settings.cgTint[1], m_settings.cgTint[2]});
        
        hdr->SetChromaticAberrationEnabled(m_settings.caEnabled);
        hdr->SetChromaticAberrationIntensity(m_settings.caIntensity);
        hdr->SetChromaticAberrationPadding(m_settings.caPadding);
        
        hdr->SetSSAOEnabled(m_settings.ssaoEnabled);
    }
    
    auto* bloom = pm.GetBloomEffect();
    if (bloom) {
        bloom->SetThreshold(m_settings.bloomThreshold);
        bloom->SetIntensity(m_settings.bloomIntensity);
        bloom->SetScatter(m_settings.bloomScatter);
    }
    
    auto* ssao = pm.GetSSAOEffect();
    if (ssao) {
        ssao->SetRadius(m_settings.ssaoRadius);
        ssao->SetBias(m_settings.ssaoBias);
        ssao->SetIntensity(m_settings.ssaoIntensity);
    }
    */
}

void GameSettingsManager::MarkDirty() {
    m_dirty = true;
}

// Setters
void GameSettingsManager::SetMasterVolume(float volume) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.masterVolume = std::clamp(volume, 0.0f, 1.0f); MarkDirty(); } AudioManager::GetInstance().SetMasterVolume(m_settings.masterVolume); }
void GameSettingsManager::SetBGMVolume(float volume) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.bgmVolume = std::clamp(volume, 0.0f, 1.0f); MarkDirty(); } AudioManager::GetInstance().SetBusVolume("BGM", m_settings.bgmVolume); }
void GameSettingsManager::SetSFXVolume(float volume) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.sfxVolume = std::clamp(volume, 0.0f, 1.0f); MarkDirty(); } AudioManager::GetInstance().SetBusVolume("SFX", m_settings.sfxVolume); AudioManager::GetInstance().SetBusVolume("UI", m_settings.sfxVolume); }
void GameSettingsManager::SetGamma(float gamma) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.gamma = std::clamp(gamma, 1.0f, 3.0f); MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetGamma(m_settings.gamma); }
void GameSettingsManager::SetExposure(float exposure) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.exposure = std::clamp(exposure, 0.1f, 5.0f); MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetExposure(m_settings.exposure); }
void GameSettingsManager::SetToneMappingMode(int mode) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.toneMappingMode = std::clamp(mode, 0, 2); MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetToneMappingMode(static_cast<HDREffect::ToneMappingMode>(m_settings.toneMappingMode)); }
void GameSettingsManager::SetVSync(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.vsync = enabled; MarkDirty(); } WindowManager::SetVSync(enabled); }
void GameSettingsManager::SetFullscreen(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.fullscreen = enabled; MarkDirty(); } WindowManager::SetFullscreen(enabled); }

void GameSettingsManager::SetBloomEnabled(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.bloomEnabled = enabled; MarkDirty(); } }
void GameSettingsManager::SetBloomThreshold(float threshold) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.bloomThreshold = threshold; MarkDirty(); } if (auto* b = PostProcessingManager::GetInstance().GetBloomEffect()) b->SetThreshold(threshold); }
void GameSettingsManager::SetBloomIntensity(float intensity) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.bloomIntensity = intensity; MarkDirty(); } if (auto* b = PostProcessingManager::GetInstance().GetBloomEffect()) b->SetIntensity(intensity); }
void GameSettingsManager::SetBloomScatter(float scatter) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.bloomScatter = scatter; MarkDirty(); } if (auto* b = PostProcessingManager::GetInstance().GetBloomEffect()) b->SetScatter(scatter); }

void GameSettingsManager::SetVignetteEnabled(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.vignetteEnabled = enabled; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetVignetteEnabled(enabled); }
void GameSettingsManager::SetVignetteIntensity(float intensity) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.vignetteIntensity = intensity; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetVignetteIntensity(intensity); }
void GameSettingsManager::SetVignetteSmoothness(float smoothness) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.vignetteSmoothness = smoothness; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetVignetteSmoothness(smoothness); }
void GameSettingsManager::SetVignetteColor(float r, float g, float b) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.vignetteColor[0] = r; m_settings.vignetteColor[1] = g; m_settings.vignetteColor[2] = b; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetVignetteColor({r, g, b}); }

void GameSettingsManager::SetColorGradingEnabled(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.colorGradingEnabled = enabled; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetColorGradingEnabled(enabled); }
void GameSettingsManager::SetCGBrightness(float brightness) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.cgBrightness = brightness; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetCGBrightness(brightness); }
void GameSettingsManager::SetCGContrast(float contrast) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.cgContrast = contrast; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetCGContrast(contrast); }
void GameSettingsManager::SetCGSaturation(float saturation) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.cgSaturation = saturation; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetCGSaturation(saturation); }
void GameSettingsManager::SetCGTint(float r, float g, float b) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.cgTint[0] = r; m_settings.cgTint[1] = g; m_settings.cgTint[2] = b; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetCGTint({r, g, b}); }

void GameSettingsManager::SetCAEnabled(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.caEnabled = enabled; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetChromaticAberrationEnabled(enabled); }
void GameSettingsManager::SetCAIntensity(float intensity) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.caIntensity = intensity; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetChromaticAberrationIntensity(intensity); }
void GameSettingsManager::SetCAPadding(float padding) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.caPadding = padding; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetChromaticAberrationPadding(padding); }

void GameSettingsManager::SetSSAOEnabled(bool enabled) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.ssaoEnabled = enabled; MarkDirty(); } if (auto* hdr = PostProcessingManager::GetInstance().GetHDREffect()) hdr->SetSSAOEnabled(enabled); }
void GameSettingsManager::SetSSAORadius(float radius) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.ssaoRadius = radius; MarkDirty(); } if (auto* s = PostProcessingManager::GetInstance().GetSSAOEffect()) s->SetRadius(radius); }
void GameSettingsManager::SetSSAOBias(float bias) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.ssaoBias = bias; MarkDirty(); } if (auto* s = PostProcessingManager::GetInstance().GetSSAOEffect()) s->SetBias(bias); }
void GameSettingsManager::SetSSAOIntensity(float intensity) { { std::lock_guard<std::mutex> lock(m_mutex); m_settings.ssaoIntensity = intensity; MarkDirty(); } if (auto* s = PostProcessingManager::GetInstance().GetSSAOEffect()) s->SetIntensity(intensity); }

// Getters
float GameSettingsManager::GetMasterVolume() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.masterVolume; }
float GameSettingsManager::GetBGMVolume() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.bgmVolume; }
float GameSettingsManager::GetSFXVolume() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.sfxVolume; }
float GameSettingsManager::GetGamma() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.gamma; }
float GameSettingsManager::GetExposure() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.exposure; }
int GameSettingsManager::GetToneMappingMode() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.toneMappingMode; }
bool GameSettingsManager::GetVSync() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.vsync; }
bool GameSettingsManager::GetFullscreen() const { std::lock_guard<std::mutex> lock(m_mutex); return m_settings.fullscreen; }
