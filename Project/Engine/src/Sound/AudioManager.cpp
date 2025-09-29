#include "pch.h"
#include "Sound/AudioManager.hpp"
#include "Sound/Audio.hpp"
#include <fmod.h>
#include <fmod_errors.h>
#include <iostream>
#include <filesystem>
#include <atomic>
#include "Logging.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Sound/AudioComponent.hpp"
#include "Transform/TransformComponent.hpp"

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#endif

AudioManager& AudioManager::GetInstance() {
    static AudioManager inst;
    return inst;
}

AudioManager::AudioManager() : system(nullptr) {}

bool AudioManager::Initialise() {
    std::lock_guard<std::mutex> lock(mtx);
    if (system) return true;

    shuttingDown.store(false);

    FMOD_RESULT result = FMOD_System_Create(&system, FMOD_VERSION);
    if (result != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: FMOD_System_Create failed: ", FMOD_ErrorString(result), "\n");
        system = nullptr;
        return false;
    }

    result = FMOD_System_Init(system, 512, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: FMOD_System_Init failed: ", FMOD_ErrorString(result), "\n");
        FMOD_System_Release(system);
        system = nullptr;
        return false;
    }

    ENGINE_PRINT("[AudioManager] FMOD initialized successfully.\n");
    return true;
}

void AudioManager::Shutdown() {
    // Use atomic exchange to ensure shutdown runs only once
    if (shuttingDown.exchange(true)) {
        return; // Already shutting down or shut down
    }

    // Stop all channels and collect FMOD objects
    FMOD_SYSTEM* sys = nullptr;
    std::vector<FMOD_CHANNEL*> channels;
    std::vector<FMOD_CHANNELGROUP*> groups;

    {
        std::lock_guard<std::mutex> lock(mtx);

        // Stop and collect all channels
        for (auto& kv : channelMap) {
            if (kv.second.channel) {
                channels.push_back(kv.second.channel);
                kv.second.state = AudioSourceState::Stopped;
            }
        }
        channelMap.clear();

        // Collect channel groups
        for (auto& kv : busMap) {
            if (kv.second) groups.push_back(kv.second);
        }
        busMap.clear();

        // Take ownership of system
        sys = system;
        system = nullptr;
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
    if (shuttingDown.load()) return;

    // Phase 1: Update FMOD system and internal state (requires lock)
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!system) return;

        // Update FMOD system
        FMOD_System_Update(system);

        // Update channel states and cleanup stopped channels
        CleanupStoppedChannels();
    }

    // Phase 2: Update audio components (no lock held)
    // Get all entities with AudioComponents outside of the lock
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    std::vector<Entity> allEntities = ecsManager.GetActiveEntities();

    for (const auto& entity : allEntities) {
        if (ecsManager.HasComponent<AudioComponent>(entity)) {
            AudioComponent& audioComp = ecsManager.GetComponent<AudioComponent>(entity);
            audioComp.UpdateComponent();

            // Update spatial audio position from Transform if applicable
            if (audioComp.Spatialize && ecsManager.HasComponent<Transform>(entity)) {
                const Transform& transform = ecsManager.GetComponent<Transform>(entity);
                /*Vector3D worldPos = transform.worldMatrix.GetTranslation();
                audioComp.OnTransformChanged(worldPos);*/
            }
        }
    }
}

ChannelHandle AudioManager::PlayAudio(std::shared_ptr<Audio> audioAsset, bool loop, float volume) {
    if (shuttingDown.load() || globalPaused.load()) return 0;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system || !audioAsset || !audioAsset->sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: PlayAudio called with invalid parameters.\n");
        return 0;
    }

    FMOD_CHANNEL* channel = nullptr;

    // Play paused so we can configure the channel before it starts
    FMOD_RESULT res = FMOD_System_PlaySound(system, audioAsset->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: PlaySound failed: ", FMOD_ErrorString(res), "\n");
        return 0;
    }

    // Configure per-channel looping explicitly
    FMOD_MODE channelMode = FMOD_DEFAULT;
    channelMode |= (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    FMOD_Channel_SetMode(channel, channelMode);
	std::cout << "loop mode set to: " << (loop ? "FMOD_LOOP_NORMAL" : "FMOD_LOOP_OFF") << std::endl;
    // Set loop count: -1 == infinite loop, 0 == play once
    int loopCount = loop ? -1 : 0;
    FMOD_Channel_SetLoopCount(channel, loopCount);

    // Apply volume with master volume
    float finalVolume = volume * masterVolume.load();
    FMOD_Channel_SetVolume(channel, finalVolume);

    // Unpause to start playback
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = nextChannelHandle++;
    ChannelData chd;
    chd.channel = channel;
    chd.id = chId;
    chd.state = AudioSourceState::Playing;
    chd.assetPath = audioAsset->assetPath;
    channelMap[chId] = chd;

    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

ChannelHandle AudioManager::PlayAudioAtPosition(std::shared_ptr<Audio> audioAsset, const Vector3D& position, bool loop, float volume, float attenuation) {
    if (shuttingDown.load() || globalPaused.load()) return 0;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system || !audioAsset || !audioAsset->sound) return 0;

    FMOD_CHANNEL* channel = nullptr;

    // Play paused so we can configure the channel before it starts
    FMOD_RESULT res = FMOD_System_PlaySound(system, audioAsset->sound, nullptr, true, &channel);
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

    float finalVolume = volume * attenuation * masterVolume.load();
    FMOD_Channel_SetVolume(channel, finalVolume);

    // Unpause to start playback
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = nextChannelHandle++;
    ChannelData chd;
    chd.channel = channel;
    chd.id = chId;
    chd.state = AudioSourceState::Playing;
    chd.assetPath = audioAsset->assetPath;
    channelMap[chId] = chd;

    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

ChannelHandle AudioManager::PlayAudioOnBus(std::shared_ptr<Audio> audioAsset, const std::string& busName, bool loop, float volume) {
    if (shuttingDown.load() || globalPaused.load()) return 0;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system || !audioAsset || !audioAsset->sound) return 0;

    FMOD_CHANNELGROUP* group = GetOrCreateBus(busName);
    if (!group) return 0;

    FMOD_CHANNEL* channel = nullptr;

    // Play paused so we can configure the channel
    FMOD_RESULT res = FMOD_System_PlaySound(system, audioAsset->sound, nullptr, true, &channel);
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

    float finalVolume = volume * masterVolume.load();
    FMOD_Channel_SetVolume(channel, finalVolume);

    // Unpause to start playback
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = nextChannelHandle++;
    ChannelData chd;
    chd.channel = channel;
    chd.id = chId;
    chd.state = AudioSourceState::Playing;
    chd.assetPath = audioAsset->assetPath;
    channelMap[chId] = chd;

    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

void AudioManager::Stop(ChannelHandle channel) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return;

    if (it->second.channel) {
        FMOD_Channel_Stop(it->second.channel);
        it->second.state = AudioSourceState::Stopped;
    }
    channelMap.erase(it);
}

void AudioManager::StopAll() {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    for (auto& kv : channelMap) {
        if (kv.second.channel) {
            FMOD_Channel_Stop(kv.second.channel);
            kv.second.state = AudioSourceState::Stopped;
        }
    }
    channelMap.clear();
}

void AudioManager::Pause(ChannelHandle channel) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    FMOD_Channel_SetPaused(it->second.channel, true);
    it->second.state = AudioSourceState::Paused;
}

void AudioManager::Resume(ChannelHandle channel) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    FMOD_Channel_SetPaused(it->second.channel, false);
    it->second.state = AudioSourceState::Playing;
}

bool AudioManager::IsPlaying(ChannelHandle channel) {
    if (shuttingDown.load()) return false;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return false;

    FMOD_BOOL playing = 0;
    FMOD_Channel_IsPlaying(it->second.channel, &playing);
    return playing != 0 && it->second.state == AudioSourceState::Playing;
}

bool AudioManager::IsPaused(ChannelHandle channel) {
    if (shuttingDown.load()) return false;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return false;

    return it->second.state == AudioSourceState::Paused;
}

AudioSourceState AudioManager::GetState(ChannelHandle channel) {
    if (shuttingDown.load()) return AudioSourceState::Stopped;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return AudioSourceState::Stopped;

    UpdateChannelState(it->first);
    return it->second.state;
}

void AudioManager::SetChannelVolume(ChannelHandle channel, float volume) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    float finalVolume = volume * masterVolume.load();
    FMOD_Channel_SetVolume(it->second.channel, finalVolume);
}

void AudioManager::SetChannelPitch(ChannelHandle channel, float pitch) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    FMOD_Channel_SetPitch(it->second.channel, pitch);
}

void AudioManager::SetChannelLoop(ChannelHandle channel, bool loop) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    FMOD_MODE mode = loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
    FMOD_Channel_SetMode(it->second.channel, mode);
}

void AudioManager::UpdateChannelPosition(ChannelHandle channel, const Vector3D& position) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    FMOD_VECTOR pos = { position.x, position.y, position.z };
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };
    FMOD_Channel_Set3DAttributes(it->second.channel, &pos, &vel);
}

FMOD_CHANNELGROUP* AudioManager::GetOrCreateBus(const std::string& busName) {
    auto it = busMap.find(busName);
    if (it != busMap.end()) return it->second;

    if (!system) return nullptr;

    FMOD_CHANNELGROUP* group = nullptr;
    FMOD_RESULT res = FMOD_System_CreateChannelGroup(system, busName.c_str(), &group);
    if (res != FMOD_OK || !group) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: Failed to create bus ", busName, ": ", FMOD_ErrorString(res), "\n");
        return nullptr;
    }

    busMap[busName] = group;
    return group;
}

void AudioManager::SetBusVolume(const std::string& busName, float volume) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = busMap.find(busName);
    if (it == busMap.end() || !it->second) return;

    FMOD_ChannelGroup_SetVolume(it->second, volume);
}

void AudioManager::SetBusPaused(const std::string& busName, bool paused) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = busMap.find(busName);
    if (it == busMap.end() || !it->second) return;

    FMOD_ChannelGroup_SetPaused(it->second, paused);
}

void AudioManager::SetMasterVolume(float volume) {
    masterVolume.store(volume);
    
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system) return;

    // Update all existing channels
    for (auto& kv : channelMap) {
        if (kv.second.channel) {
            float currentVolume;
            FMOD_Channel_GetVolume(kv.second.channel, &currentVolume);
            FMOD_Channel_SetVolume(kv.second.channel, currentVolume * volume);
        }
    }
}

float AudioManager::GetMasterVolume() const {
    return masterVolume.load();
}

void AudioManager::SetGlobalPaused(bool paused) {
    globalPaused.store(paused);
    
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system) return;

    // Apply to all channels
    for (auto& kv : channelMap) {
        if (kv.second.channel && kv.second.state == AudioSourceState::Playing) {
            FMOD_Channel_SetPaused(kv.second.channel, paused);
        }
    }
}

FMOD_SOUND* AudioManager::CreateSound(const std::string& assetPath) {
    if (shuttingDown.load()) return nullptr;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: CreateSound called but system not initialized.\n");
        return nullptr;
    }

    FMOD_SOUND* sound = nullptr;
    FMOD_RESULT res;

#ifdef ANDROID
    if (androidAssetManager) {
        // On Android, try loading from assets first
        AAssetManager* assetMgr = static_cast<AAssetManager*>(androidAssetManager);
        AAsset* asset = AAssetManager_open(assetMgr, assetPath.c_str(), AASSET_MODE_BUFFER);
        
        if (asset) {
            off_t length = AAsset_getLength(asset);
            const void* buffer = AAsset_getBuffer(asset);
            
            FMOD_CREATESOUNDEXINFO exinfo = {};
            exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
            exinfo.length = static_cast<unsigned int>(length);
            
            res = FMOD_System_CreateSound(system, static_cast<const char*>(buffer), 
                                        FMOD_OPENMEMORY | FMOD_LOOP_OFF, &exinfo, &sound);
            AAsset_close(asset);
            
            if (res == FMOD_OK) {
                return sound;
            }
        }
    }
#endif

    // Fallback to file system loading
    res = FMOD_System_CreateSound(system, assetPath.c_str(), FMOD_LOOP_OFF, nullptr, &sound);
    if (res != FMOD_OK || !sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: Failed to create sound for ", assetPath, ": ", FMOD_ErrorString(res), "\n");
        return nullptr;
    }

    return sound;
}

FMOD_SOUND* AudioManager::CreateSoundFromMemory(const void* data, unsigned int length, const std::string& assetPath) {
    if (shuttingDown.load() || !data || length == 0) return nullptr;

    std::lock_guard<std::mutex> lock(mtx);
    if (!system) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: CreateSoundFromMemory called but system not initialized.\n");
        return nullptr;
    }

    FMOD_SOUND* sound = nullptr;
    FMOD_CREATESOUNDEXINFO exinfo = {};
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    exinfo.length = length;

    FMOD_RESULT res = FMOD_System_CreateSound(system, static_cast<const char*>(data), FMOD_OPENMEMORY | FMOD_LOOP_OFF, &exinfo, &sound);
    if (res != FMOD_OK || !sound) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: CreateSoundFromMemory failed for ", assetPath, ": ", FMOD_ErrorString(res), "\n");
        return nullptr;
    }

    return sound;
}

void AudioManager::ReleaseSound(FMOD_SOUND* sound, const std::string& assetPath) {
    if (shuttingDown.load()) return;

    std::lock_guard<std::mutex> lock(mtx);
    if (!sound) return;

    FMOD_RESULT res = FMOD_Sound_Release(sound);
    if (res != FMOD_OK) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioManager] ERROR: Failed to release sound ", assetPath, ": ", FMOD_ErrorString(res), "\n");
    }
}

#ifdef ANDROID
void AudioSystem::SetAndroidAssetManager(void* assetManager) {
    androidAssetManager = assetManager;
}
#endif

void AudioManager::CleanupStoppedChannels() {
    std::vector<ChannelHandle> toErase;
    
    for (auto& kv : channelMap) {
        if (kv.second.channel) {
            FMOD_BOOL playing = 0;
            FMOD_Channel_IsPlaying(kv.second.channel, &playing);
            
            if (!playing && kv.second.state != AudioSourceState::Paused) {
                kv.second.state = AudioSourceState::Stopped;
                toErase.push_back(kv.first);
            }
        }
    }
    
    for (auto id : toErase) {
        channelMap.erase(id);
    }
}

bool AudioManager::IsChannelValid(ChannelHandle channel) {
    auto it = channelMap.find(channel);
    return it != channelMap.end() && it->second.channel != nullptr;
}

void AudioManager::UpdateChannelState(ChannelHandle channel) {
    auto it = channelMap.find(channel);
    if (it == channelMap.end() || !it->second.channel) return;

    FMOD_BOOL playing = 0;
    FMOD_BOOL paused = 0;
    
    FMOD_Channel_IsPlaying(it->second.channel, &playing);
    FMOD_Channel_GetPaused(it->second.channel, &paused);

    if (!playing) {
        it->second.state = AudioSourceState::Stopped;
    } else if (paused) {
        it->second.state = AudioSourceState::Paused;
    } else {
        it->second.state = AudioSourceState::Playing;
    }
}