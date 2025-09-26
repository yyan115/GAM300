#pragma once

//#include <fmod.h> // FMOD types are forward-declared to avoid requiring FMOD headers in all translation units
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Asset Manager/Asset.hpp"
#include "Utilities/GUID.hpp"
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

class ENGINE_API AudioSystem : public System {
public:
    static AudioSystem& GetInstance();

    // Lifecycle
    bool Initialise();
    void Shutdown();

    // Per-frame update - call from main loop
    void Update();

    // Load/unload audio asset. assetPath should be the original asset path under Resources (eg "Resources/Audio/boom.wav").
    // Returns 0 on failure.
    AudioHandle LoadAudio(const std::string& assetPath);
    bool UnloadAudio(AudioHandle handle);

    // Query raw FMOD sound pointer (may be nullptr)
    FMOD_SOUND* GetSound(AudioHandle handle);

    // Play/stop
    ChannelHandle Play(AudioHandle handle, bool loop = false, float volume = 1.0f);
    void Stop(ChannelHandle channel);
    bool IsPlaying(ChannelHandle channel);

    // Play on a named bus (channel group). Creates the group if it doesn't exist.
    ChannelHandle PlayOnBus(AudioHandle handle, const std::string& busName, bool loop = false, float volume = 1.0f);
    // Create or get a channel group (bus)
    FMOD_CHANNELGROUP* GetOrCreateBus(const std::string& busName);

    // Spatialized playback at position
    ChannelHandle PlayAtPosition(AudioHandle handle, const Vector3D& position, bool loop = false, float volume = 1.0f, float attenuation = 1.0f);
    void UpdateChannelPosition(ChannelHandle channel, const Vector3D& position);

    // Utility to set pitch on a channel
    void SetChannelPitch(ChannelHandle channel, float pitch);
    void SetChannelVolume(ChannelHandle channel, float volume);

public:
    AudioSystem();
    ~AudioSystem();

    // Non-copyable
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

public:
    // Minimal IAsset-based Audio asset so ResourceManager can manage audio assets via GetResource<Audio>
    class Audio : public IAsset {
    public:
        Audio() = default;
        virtual ~Audio() = default;

        // For this engine we'll treat CompileToResource as identity (no conversion) and LoadResource as a no-op placeholder.
        std::string CompileToResource(const std::string& assetPath) override { return assetPath; }
        bool LoadResource(const std::string& assetPath) override { assetFilePath = assetPath; return true; }
        std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) override { assetPath; currentMetaData; return nullptr; }

        std::string assetFilePath;
    };

private:
    struct AudioData {
        FMOD_SOUND* sound = nullptr;
        std::string assetPath;
        int refCount = 0;
        bool is3D = false;
        float attenuation = 1.0f; // user attenuation multiplier
    };

    struct ChannelData {
        FMOD_CHANNEL* channel = nullptr;
        ChannelHandle id = 0;
    };

    std::mutex mtx;
    std::unordered_map<AudioHandle, std::shared_ptr<AudioData>> audioMap;
    std::unordered_map<std::string, AudioHandle> pathToHandle;
    std::unordered_map<ChannelHandle, ChannelData> channelMap;

    // Channel groups (buses)
    std::unordered_map<std::string, FMOD_CHANNELGROUP*> busMap;

    std::atomic<AudioHandle> nextAudioHandle{1};
    std::atomic<ChannelHandle> nextChannelHandle{1};

    FMOD_SYSTEM* system = nullptr;
};