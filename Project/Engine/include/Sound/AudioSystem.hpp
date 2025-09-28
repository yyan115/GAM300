#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include "Math/Vector3D.hpp"
#include "ECS/SystemManager.hpp"

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

class ENGINE_API AudioSystem : public System {
public:
    static AudioSystem& GetInstance();

    // Lifecycle
    bool Initialise();
    void Shutdown();

    // Per-frame update - call from main loop
    void Update();

    // Play/stop using ResourceManager Audio assets
    ChannelHandle PlayAudio(std::shared_ptr<Audio> audioAsset, bool loop = false, float volume = 1.0f);
    void Stop(ChannelHandle channel);
    bool IsPlaying(ChannelHandle channel);

    // Play on a named bus (channel group). Creates the group if it doesn't exist.
    ChannelHandle PlayAudioOnBus(std::shared_ptr<Audio> audioAsset, const std::string& busName, bool loop = false, float volume = 1.0f);
    // Create or get a channel group (bus)
    FMOD_CHANNELGROUP* GetOrCreateBus(const std::string& busName);

    // Spatialized playback at position
    ChannelHandle PlayAudioAtPosition(std::shared_ptr<Audio> audioAsset, const Vector3D& position, bool loop = false, float volume = 1.0f, float attenuation = 1.0f);
    void UpdateChannelPosition(ChannelHandle channel, const Vector3D& position);

    // Utility to set pitch on a channel
    void SetChannelPitch(ChannelHandle channel, float pitch);
    void SetChannelVolume(ChannelHandle channel, float volume);

    // New helpers to replace the removed AudioManager: create / release FMOD_SOUND resources.
    FMOD_SOUND* CreateSound(const std::string& assetPath);
    void ReleaseSound(FMOD_SOUND* sound, const std::string& assetPath);

public:
    AudioSystem();
    ~AudioSystem() = default;

    // Non-copyable
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

private:
    struct ChannelData {
        FMOD_CHANNEL* channel = nullptr;
        ChannelHandle id = 0;
    };

    std::mutex mtx;
    std::unordered_map<ChannelHandle, ChannelData> channelMap;

    // Channel groups (buses)
    std::unordered_map<std::string, FMOD_CHANNELGROUP*> busMap;

    std::atomic<ChannelHandle> nextChannelHandle{ 1 };

    FMOD_SYSTEM* system = nullptr;
};