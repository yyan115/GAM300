#pragma once

#include <atomic>
#include <vector>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include "Math/Vector3D.hpp"
#include "Engine.h"

// Forward declarations for FMOD types to keep header lightweight
typedef struct FMOD_SYSTEM FMOD_SYSTEM;
typedef struct FMOD_SOUND FMOD_SOUND;
typedef struct FMOD_CHANNEL FMOD_CHANNEL;
typedef struct FMOD_CHANNELGROUP FMOD_CHANNELGROUP;
typedef struct FMOD_REVERB3D FMOD_REVERB3D;
typedef struct FMOD_REVERB_PROPERTIES FMOD_REVERB_PROPERTIES;

// Simple handles used by the engine to refer to audio and playback channels.
using AudioHandle = uint64_t;
using ChannelHandle = uint64_t;

// Forward declare Audio class from ResourceManager system
class Audio;

// Audio source states
enum class AudioSourceState {
    Stopped,
    Playing,
    Paused
};

// Channel update flags for batch processing
enum ChannelUpdateFlags {
    UPDATE_VOLUME = 1 << 0,
    UPDATE_PITCH = 1 << 1,
    UPDATE_POSITION = 1 << 2,
    UPDATE_LOOP = 1 << 3,
    UPDATE_3D_MINMAX = 1 << 4,
    UPDATE_REVERB_MIX = 1 << 5,
    UPDATE_PRIORITY = 1 << 6,
    UPDATE_STEREO_PAN = 1 << 7,
    UPDATE_DOPPLER_LEVEL = 1 << 8
};

// Structure to batch channel property updates
struct ChannelUpdate {
    float volume = 1.0f;
    float pitch = 1.0f;
    Vector3D position = Vector3D(0.0f, 0.0f, 0.0f);
    bool loop = false;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    float reverbMix = 0.0f;
    int priority = 128;
    float stereoPan = 0.0f;
    float dopplerLevel = 1.0f;
    uint32_t flags = 0; // Bitmask of ChannelUpdateFlags
};

// AudioManager: singleton backend for FMOD system management
// Handles low-level audio operations, channel management, and global audio state
class AudioManager {
public:
    ENGINE_API static AudioManager& GetInstance();

    // Lifecycle - explicit management only
    bool Initialise();
    void Shutdown();

    // Per-frame update - now lightweight (FMOD processing moved to dedicated audio thread)
    void Update();

    // Play/Stop/Pause API
    ChannelHandle PlayAudio(std::shared_ptr<Audio> audioAsset, bool loop = false, float volume = 1.0f);
    ChannelHandle PlayAudioAtPosition(std::shared_ptr<Audio> audioAsset, const Vector3D& position, bool loop = false, float volume = 1.0f, float attenuation = 1.0f, float minDistance = 1.0f, float maxDistance = 100.0f);
    ChannelHandle PlayAudioOnBus(std::shared_ptr<Audio> audioAsset, const std::string& busName, bool loop = false, float volume = 1.0f);
    
    void Stop(ChannelHandle channel);
    void ENGINE_API StopAll();
    void Pause(ChannelHandle channel);
    void Resume(ChannelHandle channel);
    
    // State queries
    bool IsPlaying(ChannelHandle channel);
    bool IsPaused(ChannelHandle channel);
    AudioSourceState GetState(ChannelHandle channel);

    // Channel property setters (now batched)
    void SetChannelVolume(ChannelHandle channel, float volume);
    void SetChannelPitch(ChannelHandle channel, float pitch);
    void SetChannelLoop(ChannelHandle channel, bool loop);
    void UpdateChannelPosition(ChannelHandle channel, const Vector3D& position);
    void SetChannel3DMinMaxDistance(ChannelHandle channel, float minDistance, float maxDistance);
    void SetChannelReverbMix(ChannelHandle channel, float reverbMix);
    void SetChannelPriority(ChannelHandle channel, int priority);
    void SetChannelStereoPan(ChannelHandle channel, float pan);
    void SetChannelDopplerLevel(ChannelHandle channel, float level);

    // Batch update processing
    void ApplyBatchUpdates();

    // Bus (channel group) management
    FMOD_CHANNELGROUP* GetOrCreateBus(const std::string& busName);
    void ENGINE_API SetBusVolume(const std::string& busName, float volume);
    float GetBusVolume(const std::string& busName);
    void SetBusPaused(const std::string& busName, bool paused);

    // Global audio settings
    void ENGINE_API SetMasterVolume(float volume);
    float GetMasterVolume() const;
    void ENGINE_API SetGlobalPaused(bool paused);

    // Resource management helpers
    FMOD_SOUND* CreateSound(const std::string& assetPath);
    void ReleaseSound(FMOD_SOUND* sound, const std::string& assetPath);

    // Create sound from raw memory (useful on Android when reading APK assets into memory)
    FMOD_SOUND* CreateSoundFromMemory(const void* data, unsigned int length, const std::string& assetPath);
    void SetListenerAttributes(int listener, const Vector3D& position, const Vector3D& velocity, const Vector3D& forward, const Vector3D& up);

    // Reverb Zone Management
    FMOD_REVERB3D* CreateReverbZone();
    void ReleaseReverbZone(FMOD_REVERB3D* reverb);
    void SetReverbZoneAttributes(FMOD_REVERB3D* reverb, const Vector3D& position, float minDistance, float maxDistance);
    void SetReverbZoneProperties(FMOD_REVERB3D* reverb, const FMOD_REVERB_PROPERTIES* properties);
    //void SetChannelReverbMix(ChannelHandle channel, float reverbMix);
    
    // Get FMOD system for advanced use
    FMOD_SYSTEM* GetFMODSystem() const { return System; }
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

    // Thread safety - upgraded to shared_mutex for better concurrency
    mutable std::shared_mutex Mutex;
    std::atomic<bool> ShuttingDown{ false };
    
    // FMOD handles
    FMOD_SYSTEM* System = nullptr;
    
    // Channel management
    std::unordered_map<ChannelHandle, ChannelData> ChannelMap;
    std::atomic<ChannelHandle> NextChannelHandle{ 1 };

    // Pending batch updates
    std::unordered_map<ChannelHandle, ChannelUpdate> PendingUpdates;

    // Channel groups (buses)
    std::unordered_map<std::string, FMOD_CHANNELGROUP*> BusMap;

    // Pending bus volumes (for buses not yet created)
    std::unordered_map<std::string, float> PendingBusVolumes;

    // Global settings
    std::atomic<float> MasterVolume{ 1.0f };
    std::atomic<bool> GlobalPaused{ false };

    // Internal helpers
    void CleanupStoppedChannels();
    bool IsChannelValid(ChannelHandle channel);
    void UpdateChannelState(ChannelHandle channel);

    // Dedicated audio thread for FMOD processing
    void AudioThreadLoop();
    std::thread m_audioThread;
    std::atomic<bool> m_threadRunning{false};
};