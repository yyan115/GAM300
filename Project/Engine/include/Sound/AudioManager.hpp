#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include "Math/Vector3D.hpp"
#include "Engine.h"

// Forward declarations for FMOD types to keep header lightweight
typedef struct FMOD_SYSTEM FMOD_SYSTEM;
typedef struct FMOD_SOUND FMOD_SOUND;
typedef struct FMOD_CHANNEL FMOD_CHANNEL;
typedef struct FMOD_CHANNELGROUP FMOD_CHANNELGROUP;

// Simple handles used by the engine to refer to audio and playback channels.
using AudioHandle = uint64_t;
using ChannelHandle = uint64_t;

// Forward declare Audio class from ResourceManager system
class Audio;

// Audio source states (Unity-like)
enum class AudioSourceState {
    Stopped,
    Playing,
    Paused
};

// AudioManager: singleton backend for FMOD system management
// Handles low-level audio operations, channel management, and global audio state
class AudioManager {
public:
    ENGINE_API static AudioManager& GetInstance();

    // Lifecycle - explicit management only
    bool Initialise();
    void Shutdown();

    // Per-frame update - call from main loop
    void Update();

    // Unity-like Play/Stop/Pause API
    ChannelHandle PlayAudio(std::shared_ptr<Audio> audioAsset, bool loop = false, float volume = 1.0f);
    ChannelHandle PlayAudioAtPosition(std::shared_ptr<Audio> audioAsset, const Vector3D& position, bool loop = false, float volume = 1.0f, float attenuation = 1.0f);
    ChannelHandle PlayAudioOnBus(std::shared_ptr<Audio> audioAsset, const std::string& busName, bool loop = false, float volume = 1.0f);
    
    void Stop(ChannelHandle channel);
    void ENGINE_API StopAll();
    void Pause(ChannelHandle channel);
    void Resume(ChannelHandle channel);
    
    // State queries
    bool IsPlaying(ChannelHandle channel);
    bool IsPaused(ChannelHandle channel);
    AudioSourceState GetState(ChannelHandle channel);

    // Channel property setters
    void SetChannelVolume(ChannelHandle channel, float volume);
    void SetChannelPitch(ChannelHandle channel, float pitch);
    void SetChannelLoop(ChannelHandle channel, bool loop);
    void UpdateChannelPosition(ChannelHandle channel, const Vector3D& position);

    // Bus (channel group) management
    FMOD_CHANNELGROUP* GetOrCreateBus(const std::string& busName);
    void SetBusVolume(const std::string& busName, float volume);
    void SetBusPaused(const std::string& busName, bool paused);

    // Global audio settings
    void SetMasterVolume(float volume);
    float GetMasterVolume() const;
    void ENGINE_API SetGlobalPaused(bool paused);

    // Resource management helpers
    FMOD_SOUND* CreateSound(const std::string& assetPath);
    void ReleaseSound(FMOD_SOUND* sound, const std::string& assetPath);

    // Create sound from raw memory (useful on Android when reading APK assets into memory)
    FMOD_SOUND* CreateSoundFromMemory(const void* data, unsigned int length, const std::string& assetPath);
    void SetListenerAttributes(int listener, const Vector3D& position, const Vector3D& velocity, const Vector3D& forward, const Vector3D& up);
public:
    AudioManager();
    ~AudioManager() = default; // No automatic shutdown

    // Non-copyable
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

private:
    struct ChannelData {
        FMOD_CHANNEL* Channel = nullptr;
        ChannelHandle Id = 0;
        AudioSourceState State = AudioSourceState::Stopped;
        std::string AssetPath; // For debugging
    };

    // Thread safety
    mutable std::mutex Mutex;
    std::atomic<bool> ShuttingDown{ false };
    
    // FMOD handles
    FMOD_SYSTEM* System = nullptr;
    
    // Channel management
    std::unordered_map<ChannelHandle, ChannelData> ChannelMap;
    std::atomic<ChannelHandle> NextChannelHandle{ 1 };

    // Channel groups (buses)
    std::unordered_map<std::string, FMOD_CHANNELGROUP*> BusMap;

    // Global settings
    std::atomic<float> MasterVolume{ 1.0f };
    std::atomic<bool> GlobalPaused{ false };

    // Internal helpers
    void CleanupStoppedChannels();
    bool IsChannelValid(ChannelHandle channel);
    void UpdateChannelState(ChannelHandle channel);
};