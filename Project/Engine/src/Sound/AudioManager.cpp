#include "pch.h"
#include "Sound/AudioManager.hpp"
#include "Sound/Audio.hpp"
#include <fmod.h>
#include <fmod_errors.h>
#include <iostream>
#include <filesystem>
#include <atomic>
#include "Logging.hpp"
#include "Performance/PerformanceProfiler.hpp"

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#include <fmod/fmod.h>
#include <fmod/fmod_errors.h>
#include <fmod/fmod_android.h>
#endif

AudioManager& AudioManager::GetInstance() {
    static AudioManager inst;
    return inst;
}

AudioManager::AudioManager() {}

bool AudioManager::Initialise() {
    std::lock_guard<std::mutex> lock(Mutex);
    if (System) return true;

    ShuttingDown.store(false);

    FMOD_RESULT result = FMOD_System_Create(&System, FMOD_VERSION);
    if (result != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: FMOD_System_Create failed: ", FMOD_ErrorString(result), "\n");
        System = nullptr;
        return false;
    }

    #ifdef ANDROID
    result = FMOD_System_SetOutput(System, FMOD_OUTPUTTYPE_AAUDIO);
    if (result != FMOD_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "FMOD_System_SetOutput failed: %s", FMOD_ErrorString(result));
        // result = FMOD_System_SetOutput(System, FMOD_OUTPUTTYPE_AAUDIO);
    }

    // Set DSP buffer size to reduce crackling (1024 samples, 4 buffers - adjust based on device)
    result = FMOD_System_SetDSPBufferSize(System, 1024, 4);
    if (result != FMOD_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "FMOD_System_SetDSPBufferSize failed: %s", FMOD_ErrorString(result));
    }
    #endif

    result = FMOD_System_Init(System, 512, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: FMOD_System_Init failed: ", FMOD_ErrorString(result), "\n");
        FMOD_System_Release(System);
        System = nullptr;
        return false;
    }

    ENGINE_PRINT("[AudioManager] FMOD initialized successfully.\n");
    return true;
}

void AudioManager::Shutdown() {
    // Use atomic exchange to ensure shutdown runs only once
    if (ShuttingDown.exchange(true)) {
        return; // Already shutting down or shut down
    }

    // Stop all channels and collect FMOD objects
    FMOD_SYSTEM* sys = nullptr;
    std::vector<FMOD_CHANNEL*> channels;
    std::vector<FMOD_CHANNELGROUP*> groups;

    {
        std::lock_guard<std::mutex> lock(Mutex);

        // Stop and collect all channels
        for (auto& kv : ChannelMap) {
            if (kv.second.Channel) {
                channels.push_back(kv.second.Channel);
                kv.second.State = AudioSourceState::Stopped;
            }
        }
        ChannelMap.clear();

        // Collect channel groups
        for (auto& kv : BusMap) {
            if (kv.second) groups.push_back(kv.second);
        }
        BusMap.clear();

        // Take ownership of system
        sys = System;
        System = nullptr;
    }

    // Release FMOD resources outside of mutex
    for (auto ch : channels) {
        if (ch) FMOD_Channel_Stop(ch);
    }
    
    for (auto g : groups) {
        if (g) FMOD_ChannelGroup_Release(g);
    }

    if (sys) {
        FMOD_System_Close(sys);
        FMOD_System_Release(sys);
    }

    ENGINE_PRINT("[AudioManager] Shutdown complete.\n");
}

void AudioManager::Update() {
    PROFILE_FUNCTION();
    
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System) return;

        FMOD_System_Update(System);
        CleanupStoppedChannels();
}

ChannelHandle AudioManager::PlayAudio(std::shared_ptr<Audio> audioAsset, bool loop, float volume) {
    if (ShuttingDown.load() || GlobalPaused.load()) return 0;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System || !audioAsset || !audioAsset->sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: PlayAudio called with invalid parameters.\n");
        return 0;
    }

    FMOD_CHANNEL* channel = nullptr;

    // Play paused so we can configure the channel before it starts
    FMOD_RESULT res = FMOD_System_PlaySound(System, audioAsset->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] FMOD_System_PlaySound failed: %s\n", FMOD_ErrorString(res));
        return 0;
    }

    // Configure per-channel looping explicitly
    FMOD_MODE channelMode = FMOD_DEFAULT;
    channelMode |= (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    FMOD_Channel_SetMode(channel, channelMode);
    
    // Set loop count: -1 == infinite loop, 0 == play once
    int loopCount = loop ? -1 : 0;
    FMOD_Channel_SetLoopCount(channel, loopCount);

    // Apply volume with master volume
    float finalVolume = volume * MasterVolume.load();
    FMOD_Channel_SetVolume(channel, finalVolume);

    // Unpause to start playback
    res = FMOD_Channel_SetPaused(channel, false);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] Failed to unpause channel: %s\n", FMOD_ErrorString(res));
    }

    ChannelHandle chId = NextChannelHandle++;
    ChannelData chd;
    chd.Channel = channel;
    chd.Id = chId;
    chd.State = AudioSourceState::Playing;
    chd.AssetPath = audioAsset->assetPath;
    ChannelMap[chId] = chd;

    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

ChannelHandle AudioManager::PlayAudioAtPosition(std::shared_ptr<Audio> audioAsset, const Vector3D& position, bool loop, float volume, float attenuation) {
    if (ShuttingDown.load() || GlobalPaused.load()) return 0;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System || !audioAsset || !audioAsset->sound) return 0;

    FMOD_CHANNEL* channel = nullptr;

    // Play paused so we can configure the channel before it starts
    FMOD_RESULT res = FMOD_System_PlaySound(System, audioAsset->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: PlayAtPosition failed: ", FMOD_ErrorString(res), "\n");
        return 0;
    }

    // Per-channel looping
    FMOD_MODE channelMode = FMOD_3D;
    channelMode |= (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    FMOD_Channel_SetMode(channel, channelMode);
    FMOD_Channel_SetLoopCount(channel, loop ? -1 : 0);

    // Set 3D position
    FMOD_VECTOR pos = { position.x, position.y, position.z };
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };
    FMOD_Channel_Set3DAttributes(channel, &pos, &vel);

    float finalVolume = volume * attenuation * MasterVolume.load();
    FMOD_Channel_SetVolume(channel, finalVolume);

    // Unpause to start playback
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = NextChannelHandle++;
    ChannelData chd;
    chd.Channel = channel;
    chd.Id = chId;
    chd.State = AudioSourceState::Playing;
    chd.AssetPath = audioAsset->assetPath;
    ChannelMap[chId] = chd;

    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

ChannelHandle AudioManager::PlayAudioOnBus(std::shared_ptr<Audio> audioAsset, const std::string& busName, bool loop, float volume) {
    if (ShuttingDown.load() || GlobalPaused.load()) return 0;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System || !audioAsset || !audioAsset->sound) return 0;

    FMOD_CHANNELGROUP* group = GetOrCreateBus(busName);
    if (!group) return 0;

    FMOD_CHANNEL* channel = nullptr;

    // Play paused so we can configure the channel
    FMOD_RESULT res = FMOD_System_PlaySound(System, audioAsset->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: PlayOnBus failed: ", FMOD_ErrorString(res), "\n");
        return 0;
    }

    // Attach to bus
    FMOD_Channel_SetChannelGroup(channel, group);

    // Per-channel looping
    FMOD_MODE channelMode = FMOD_DEFAULT;
    channelMode |= (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    FMOD_Channel_SetMode(channel, channelMode);
    FMOD_Channel_SetLoopCount(channel, loop ? -1 : 0);

    float finalVolume = volume * MasterVolume.load();
    FMOD_Channel_SetVolume(channel, finalVolume);

    // Unpause to start playback
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = NextChannelHandle++;
    ChannelData chd;
    chd.Channel = channel;
    chd.Id = chId;
    chd.State = AudioSourceState::Playing;
    chd.AssetPath = audioAsset->assetPath;
    ChannelMap[chId] = chd;

    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

void AudioManager::Stop(ChannelHandle channel) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end()) return;

    if (it->second.Channel) {
        FMOD_Channel_Stop(it->second.Channel);
        it->second.State = AudioSourceState::Stopped;
    }
    ChannelMap.erase(it);
}

void AudioManager::StopAll() {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    for (auto& kv : ChannelMap) {
        if (kv.second.Channel) {
            FMOD_Channel_Stop(kv.second.Channel);
            kv.second.State = AudioSourceState::Stopped;
        }
    }
    ChannelMap.clear();
}

void AudioManager::Pause(ChannelHandle channel) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    FMOD_Channel_SetPaused(it->second.Channel, true);
    it->second.State = AudioSourceState::Paused;
}

void AudioManager::Resume(ChannelHandle channel) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    FMOD_Channel_SetPaused(it->second.Channel, false);
    it->second.State = AudioSourceState::Playing;
}

bool AudioManager::IsPlaying(ChannelHandle channel) {
    if (ShuttingDown.load()) return false;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return false;

    FMOD_BOOL playing = 0;
    FMOD_Channel_IsPlaying(it->second.Channel, &playing);
    return playing != 0 && it->second.State == AudioSourceState::Playing;
}

bool AudioManager::IsPaused(ChannelHandle channel) {
    if (ShuttingDown.load()) return false;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end()) return false;

    return it->second.State == AudioSourceState::Paused;
}

AudioSourceState AudioManager::GetState(ChannelHandle channel) {
    if (ShuttingDown.load()) return AudioSourceState::Stopped;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end()) return AudioSourceState::Stopped;

    UpdateChannelState(it->first);
    return it->second.State;
}

void AudioManager::SetChannelVolume(ChannelHandle channel, float volume) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    float finalVolume = volume * MasterVolume.load();
    FMOD_Channel_SetVolume(it->second.Channel, finalVolume);
}

void AudioManager::SetChannelPitch(ChannelHandle channel, float pitch) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    FMOD_Channel_SetPitch(it->second.Channel, pitch);
}

void AudioManager::SetChannelLoop(ChannelHandle channel, bool loop) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    FMOD_MODE mode = loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
    FMOD_Channel_SetMode(it->second.Channel, mode);
}

void AudioManager::UpdateChannelPosition(ChannelHandle channel, const Vector3D& position) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    FMOD_VECTOR pos = { position.x, position.y, position.z };
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };
    FMOD_Channel_Set3DAttributes(it->second.Channel, &pos, &vel);
}

FMOD_CHANNELGROUP* AudioManager::GetOrCreateBus(const std::string& busName) {
    auto it = BusMap.find(busName);
    if (it != BusMap.end()) return it->second;

    if (!System) return nullptr;

    FMOD_CHANNELGROUP* group = nullptr;
    FMOD_RESULT res = FMOD_System_CreateChannelGroup(System, busName.c_str(), &group);
    if (res != FMOD_OK || !group) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: Failed to create bus ", busName, ": ", FMOD_ErrorString(res), "\n");
        return nullptr;
    }

    BusMap[busName] = group;
    return group;
}

void AudioManager::SetBusVolume(const std::string& busName, float volume) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = BusMap.find(busName);
    if (it == BusMap.end() || !it->second) return;

    FMOD_ChannelGroup_SetVolume(it->second, volume);
}

void AudioManager::SetBusPaused(const std::string& busName, bool paused) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    auto it = BusMap.find(busName);
    if (it == BusMap.end() || !it->second) return;

    FMOD_ChannelGroup_SetPaused(it->second, paused);
}

void AudioManager::SetMasterVolume(float volume) {
    MasterVolume.store(volume);
    
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System) return;

    // Update all existing channels
    for (auto& kv : ChannelMap) {
        if (kv.second.Channel) {
            float currentVolume;
            FMOD_Channel_GetVolume(kv.second.Channel, &currentVolume);
            FMOD_Channel_SetVolume(kv.second.Channel, currentVolume * volume);
        }
    }
}

float AudioManager::GetMasterVolume() const {
    return MasterVolume.load();
}

void AudioManager::SetGlobalPaused(bool paused) {
    GlobalPaused.store(paused);
    
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System) return;

    // Apply to all channels
    for (auto& kv : ChannelMap) {
        if (kv.second.Channel && kv.second.State == AudioSourceState::Playing) {
            FMOD_Channel_SetPaused(kv.second.Channel, paused);
        }
    }
}

FMOD_SOUND* AudioManager::CreateSound(const std::string& assetPath) {
    if (ShuttingDown.load()) return nullptr;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: CreateSound called but system not initialized.\n");
        return nullptr;
    }

    FMOD_SOUND* sound = nullptr;
    FMOD_RESULT res;

    // REMOVED: Android-specific block – now handled by Audio::LoadResource via platform abstraction

    // Fallback to file system loading (assumes path is resolvable by caller, e.g., ResourceManager)
    res = FMOD_System_CreateSound(System, assetPath.c_str(), FMOD_LOOP_OFF, nullptr, &sound);
    if (res != FMOD_OK || !sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: Failed to create sound for ", assetPath, ": ", FMOD_ErrorString(res), "\n");
        return nullptr;
    }

    return sound;
}

FMOD_SOUND* AudioManager::CreateSoundFromMemory(const void* data, unsigned int length, const std::string& assetPath) {
    if (ShuttingDown.load() || !data || length == 0) return nullptr;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!System) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: CreateSoundFromMemory called but system not initialized.\n");
        return nullptr;
    }

    FMOD_SOUND* sound = nullptr;
    FMOD_CREATESOUNDEXINFO exinfo = {};
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    exinfo.length = length;

    FMOD_RESULT res = FMOD_System_CreateSound(System, static_cast<const char*>(data), FMOD_OPENMEMORY | FMOD_LOOP_OFF, &exinfo, &sound);
    if (res != FMOD_OK || !sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: CreateSoundFromMemory failed for ", assetPath, ": ", FMOD_ErrorString(res), "\n");
        return nullptr;
    }

    return sound;
}

void AudioManager::ReleaseSound(FMOD_SOUND* sound, const std::string& assetPath) {
    if (ShuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(Mutex);
    if (!sound) return;

    FMOD_RESULT res = FMOD_Sound_Release(sound);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: Failed to release sound ", assetPath, ": ", FMOD_ErrorString(res), "\n");
    }
}

void AudioManager::CleanupStoppedChannels() {
    std::vector<ChannelHandle> toErase;
    
    for (auto& kv : ChannelMap) {
        if (kv.second.Channel) {
            FMOD_BOOL playing = 0;
            FMOD_Channel_IsPlaying(kv.second.Channel, &playing);
            
            if (!playing && kv.second.State != AudioSourceState::Paused) {
                kv.second.State = AudioSourceState::Stopped;
                toErase.push_back(kv.first);
            }
        }
    }
    
    for (auto id : toErase) {
        ChannelMap.erase(id);
    }
}

bool AudioManager::IsChannelValid(ChannelHandle channel) {
    auto it = ChannelMap.find(channel);
    return it != ChannelMap.end() && it->second.Channel != nullptr;
}

void AudioManager::UpdateChannelState(ChannelHandle channel) {
    auto it = ChannelMap.find(channel);
    if (it == ChannelMap.end() || !it->second.Channel) return;

    FMOD_BOOL playing = 0;
    FMOD_BOOL paused = 0;
    
    FMOD_Channel_IsPlaying(it->second.Channel, &playing);
    FMOD_Channel_GetPaused(it->second.Channel, &paused);

    if (!playing) {
        it->second.State = AudioSourceState::Stopped;
    } else if (paused) {
        it->second.State = AudioSourceState::Paused;
    } else {
        it->second.State = AudioSourceState::Playing;
    }
}