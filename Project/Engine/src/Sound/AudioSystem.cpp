#include "pch.h"
#include "Sound/AudioSystem.hpp"
#include <fmod.h>
#include <fmod_errors.h>
#include <iostream>
#include <filesystem>
#include "Asset Manager/MetaFilesManager.hpp"

AudioSystem& AudioSystem::GetInstance() {
    static AudioSystem inst;
    return inst;
}

AudioSystem::AudioSystem() : system(nullptr) {}
AudioSystem::~AudioSystem() { Shutdown(); }

bool AudioSystem::Initialise() {
    std::lock_guard<std::mutex> lock(mtx);
    if (system) return true;

    FMOD_RESULT result = FMOD_System_Create(&system, FMOD_VERSION);
    if (result != FMOD_OK) {
        std::cerr << "[AudioSystem] ERROR: FMOD_System_Create failed: " << FMOD_ErrorString(result) << "\n";
        system = nullptr;
        return false;
    }

    result = FMOD_System_Init(system, 512, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) {
        std::cerr << "[AudioSystem] ERROR: FMOD_System_Init failed: " << FMOD_ErrorString(result) << "\n";
        FMOD_System_Release(system);
        system = nullptr;
        return false;
    }

    std::cout << "[AudioSystem] FMOD initialized." << std::endl;
    return true;
}

void AudioSystem::Shutdown() {
    std::lock_guard<std::mutex> lock(mtx);
    // Stop and release channels
    for (auto& kv : channelMap) {
        if (kv.second.channel) {
            FMOD_Channel_Stop(kv.second.channel);
            kv.second.channel = nullptr;
        }
    }
    channelMap.clear();

    // Release sounds
    for (auto& kv : audioMap) {
        if (kv.second && kv.second->sound) {
            FMOD_Sound_Release(kv.second->sound);
            kv.second->sound = nullptr;
        }
    }
    audioMap.clear();
    pathToHandle.clear();

    for (auto& kv : busMap) {
        if (kv.second) {
            FMOD_ChannelGroup_Release(kv.second);
        }
    }
    busMap.clear();

    if (system) {
        FMOD_System_Close(system);
        FMOD_System_Release(system);
        system = nullptr;
    }

    std::cout << "[AudioSystem] Shutdown complete." << std::endl;
}

void AudioSystem::Update() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!system) return;

    FMOD_System_Update(system);

    // Prune stopped channels (optional)
    std::vector<ChannelHandle> toErase;
    for (auto& kv : channelMap) {
        bool isPlaying = false;
        if (kv.second.channel) {
            FMOD_Channel_IsPlaying(kv.second.channel, reinterpret_cast<FMOD_BOOL*>(&isPlaying));
            if (!isPlaying) {
                // release channel
                kv.second.channel = nullptr;
                toErase.push_back(kv.first);
            }
        }
    }
    for (auto id : toErase) channelMap.erase(id);
}

AudioHandle AudioSystem::LoadAudio(const std::string& assetPath) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!system) {
        std::cerr << "[AudioSystem] ERROR: LoadAudio called before Initialise." << std::endl;
        return 0;
    }

    std::filesystem::path p(assetPath);
    std::string normalized = p.generic_string();

    // Reuse existing handle if already loaded
    auto itPath = pathToHandle.find(normalized);
    if (itPath != pathToHandle.end()) {
        auto ah = itPath->second;
        auto it = audioMap.find(ah);
        if (it != audioMap.end()) {
            it->second->refCount++;
            return ah;
        }
    }

    // Create and load FMOD sound directly from file path
    FMOD_SOUND* sound = nullptr;
    // Allow 3D sounds by default when playing with position APIs; set 3D flag early to enable 3D features
    FMOD_MODE mode = FMOD_DEFAULT | FMOD_3D;
    FMOD_RESULT res = FMOD_System_CreateSound(system, normalized.c_str(), mode, nullptr, &sound);
    if (res != FMOD_OK) {
        // Try as stream
        res = FMOD_System_CreateStream(system, normalized.c_str(), mode, nullptr, &sound);
        if (res != FMOD_OK) {
            std::cerr << "[AudioSystem] ERROR: Failed to create sound for " << normalized << " FMOD result=" << FMOD_ErrorString(res) << "\n";
            return 0;
        }
    }

    AudioHandle handle = nextAudioHandle++;
    auto data = std::make_shared<AudioData>();
    data->sound = sound;
    data->assetPath = normalized;
    data->refCount = 1;
    data->is3D = true;
    audioMap[handle] = data;
    pathToHandle[normalized] = handle;

    // Set user data on the FMOD sound to allow callbacks to map back if needed
    FMOD_Sound_SetUserData(sound, reinterpret_cast<void*>(static_cast<uintptr_t>(handle)));

    std::cout << "[AudioSystem] Loaded audio: " << normalized << " -> handle " << handle << std::endl;
    return handle;
}

bool AudioSystem::UnloadAudio(AudioHandle handle) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = audioMap.find(handle);
    if (it == audioMap.end()) return false;

    auto& data = it->second;
    data->refCount--;
    if (data->refCount <= 0) {
        if (data->sound) {
            FMOD_Sound_Release(data->sound);
            data->sound = nullptr;
        }
        pathToHandle.erase(data->assetPath);
        audioMap.erase(it);
        std::cout << "[AudioSystem] Unloaded audio handle " << handle << std::endl;
    }
    return true;
}

FMOD_SOUND* AudioSystem::GetSound(AudioHandle handle) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = audioMap.find(handle);
    if (it == audioMap.end()) return nullptr;
    return it->second->sound;
}

ChannelHandle AudioSystem::Play(AudioHandle handle, bool loop, float volume) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!system) return 0;
    auto it = audioMap.find(handle);
    if (it == audioMap.end() || !it->second->sound) return 0;

    FMOD_CHANNEL* channel = nullptr;
    FMOD_RESULT res;
    FMOD_MODE mode = FMOD_DEFAULT;
    if (loop) mode |= FMOD_LOOP_NORMAL; else mode |= FMOD_LOOP_OFF;
    // Set sound mode for looping if needed
    FMOD_Sound_SetMode(it->second->sound, mode);

    res = FMOD_System_PlaySound(system, it->second->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        std::cerr << "[AudioSystem] ERROR: PlaySound failed: " << FMOD_ErrorString(res) << "\n";
        return 0;
    }

    // Set volume and unpause
    FMOD_Channel_SetVolume(channel, volume);
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = nextChannelHandle++;
    ChannelData chd;
    chd.channel = channel;
    chd.id = chId;
    channelMap[chId] = chd;

    // store user data if needed
    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

ChannelHandle AudioSystem::PlayOnBus(AudioHandle handle, const std::string& busName, bool loop, float volume) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!system) return 0;
    auto it = audioMap.find(handle);
    if (it == audioMap.end() || !it->second->sound) return 0;

    FMOD_CHANNELGROUP* group = GetOrCreateBus(busName);
    if (!group) return 0;

    FMOD_CHANNEL* channel = nullptr;
    FMOD_RESULT res;
    FMOD_MODE mode = FMOD_DEFAULT;
    if (loop) mode |= FMOD_LOOP_NORMAL; else mode |= FMOD_LOOP_OFF;
    FMOD_Sound_SetMode(it->second->sound, mode);

    res = FMOD_System_PlaySound(system, it->second->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        std::cerr << "[AudioSystem] ERROR: PlayOnBus PlaySound failed: " << FMOD_ErrorString(res) << "\n";
        return 0;
    }

    // Assign to channel group
    FMOD_Channel_SetChannelGroup(channel, group);
    FMOD_Channel_SetVolume(channel, volume);
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = nextChannelHandle++;
    ChannelData chd;
    chd.channel = channel;
    chd.id = chId;
    channelMap[chId] = chd;
    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

FMOD_CHANNELGROUP* AudioSystem::GetOrCreateBus(const std::string& busName) {
    auto it = busMap.find(busName);
    if (it != busMap.end()) return it->second;

    FMOD_CHANNELGROUP* group = nullptr;
    FMOD_RESULT res = FMOD_System_CreateChannelGroup(system, busName.c_str(), &group);
    if (res != FMOD_OK || !group) {
        std::cerr << "[AudioSystem] ERROR: Failed to create channel group (bus) " << busName << " result=" << FMOD_ErrorString(res) << "\n";
        return nullptr;
    }

    busMap[busName] = group;
    return group;
}

ChannelHandle AudioSystem::PlayAtPosition(AudioHandle handle, const Vector3D& position, bool loop, float volume, float attenuation) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!system) return 0;
    auto it = audioMap.find(handle);
    if (it == audioMap.end() || !it->second->sound) return 0;

    FMOD_CHANNEL* channel = nullptr;
    FMOD_RESULT res;
    FMOD_MODE mode = FMOD_DEFAULT | FMOD_3D;
    if (loop) mode |= FMOD_LOOP_NORMAL; else mode |= FMOD_LOOP_OFF;
    FMOD_Sound_SetMode(it->second->sound, mode);

    res = FMOD_System_PlaySound(system, it->second->sound, nullptr, true, &channel);
    if (res != FMOD_OK) {
        std::cerr << "[AudioSystem] ERROR: PlayAtPosition PlaySound failed: " << FMOD_ErrorString(res) << "\n";
        return 0;
    }

    // Set 3D attributes
    FMOD_VECTOR pos = { position.x, position.y, position.z };
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };
    FMOD_Channel_Set3DAttributes(channel, &pos, &vel);

    // Attenuation - implement simple distance scalar as user data on channel
    if (it->second) it->second->attenuation = attenuation;

    FMOD_Channel_SetVolume(channel, volume);
    FMOD_Channel_SetPaused(channel, false);

    ChannelHandle chId = nextChannelHandle++;
    ChannelData chd;
    chd.channel = channel;
    chd.id = chId;
    channelMap[chId] = chd;
    FMOD_Channel_SetUserData(channel, reinterpret_cast<void*>(static_cast<uintptr_t>(chId)));

    return chId;
}

void AudioSystem::UpdateChannelPosition(ChannelHandle channel, const Vector3D& position) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return;
    if (!it->second.channel) return;

    FMOD_VECTOR pos = { position.x, position.y, position.z };
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };
    FMOD_Channel_Set3DAttributes(it->second.channel, &pos, &vel);
}

void AudioSystem::Stop(ChannelHandle channel) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return;
    if (it->second.channel) {
        FMOD_Channel_Stop(it->second.channel);
        it->second.channel = nullptr;
    }
    channelMap.erase(it);
}

bool AudioSystem::IsPlaying(ChannelHandle channel) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return false;
    if (!it->second.channel) return false;
    FMOD_BOOL playing = 0;
    FMOD_Channel_IsPlaying(it->second.channel, &playing);
    return playing != 0;
}

void AudioSystem::SetChannelPitch(ChannelHandle channel, float pitch) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return;
    if (!it->second.channel) return;
    FMOD_Channel_SetPitch(it->second.channel, pitch);
}

void AudioSystem::SetChannelVolume(ChannelHandle channel, float volume) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = channelMap.find(channel);
    if (it == channelMap.end()) return;
    if (!it->second.channel) return;
    FMOD_Channel_SetVolume(it->second.channel, volume);
}
