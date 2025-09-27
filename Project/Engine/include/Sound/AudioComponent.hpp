
#pragma once
#include <string>
#include <memory>
#include <iostream>
#include "Sound/AudioSystem.hpp"
#include "Math/Vector3D.hpp"
#include "Asset Manager/ResourceManager.hpp" // load Audio assets via ResourceManager

// Forward declare Audio class to avoid including the header
class Audio;

struct AudioComponent {
    // Public editable properties used by inspector
    std::string AudioAssetPath; // original asset path under Resources
    float Volume{ 1.0f };
    bool Loop{ false };
    bool PlayOnAwake{ false };
    bool Spatialize{ false };
    float Attenuation{ 1.0f };

    // Runtime data
    std::shared_ptr<Audio> audioAsset{ nullptr };
    ChannelHandle Channel{ 0 };

    // Optional position - when spatialize is enabled inspector / transform system should update this
    Vector3D Position{ 0.0f, 0.0f, 0.0f };

    AudioComponent() {}

    ~AudioComponent() {
        // Ensure we stop any playing channels
        if (Channel) {
            AudioSystem::GetInstance().Stop(Channel);
            Channel = 0;
        }
        // audioAsset will be automatically cleaned up by shared_ptr
        audioAsset = nullptr;
    }

    // Called when inspector sets a new asset path
    void SetAudioAssetPath(const std::string& path) {
        if (path == AudioAssetPath) return;

        // Stop current audio if playing
        if (Channel) {
            AudioSystem::GetInstance().Stop(Channel);
            Channel = 0;
        }

        AudioAssetPath = path;
        audioAsset = nullptr;

        if (!AudioAssetPath.empty()) {
            // Preload the audio asset synchronously when the path is set
                audioAsset = ResourceManager::GetInstance().GetResource<Audio>(AudioAssetPath, true);
            if (!audioAsset) {
                std::cerr << "[AudioComponent] ERROR: Failed to load audio asset: " << AudioAssetPath << std::endl;
            }
            // If configured to play on awake, attempt to play now (Play will use the preloaded asset)
            if (PlayOnAwake && audioAsset) {
                Play();
            }
        }
    }

    void Play() {
        // Load the asset lazily via ResourceManager if not already loaded
        if (!audioAsset && !AudioAssetPath.empty()) {
            // Force load so editor/Inspector playback works immediately
            audioAsset = ResourceManager::GetInstance().GetResource<Audio>(AudioAssetPath, true);
            if (!audioAsset) {
                std::cerr << "[AudioComponent] ERROR: Failed to load audio asset: " << AudioAssetPath << std::endl;
                return;
            }
        }

        if (!audioAsset) {
            std::cerr << "[AudioComponent] ERROR: No audio asset to play." << std::endl;
            return;
        }

        // Ensure the AudioSystem (FMOD) is initialized before attempting to play.
        AudioSystem& AudioSys = AudioSystem::GetInstance();
        if (!AudioSys.Initialise()) {
            std::cerr << "[AudioComponent] ERROR: AudioSystem failed to initialize." << std::endl;
            return;
        }

        if (Spatialize) {
            Channel = AudioSys.PlayAudioAtPosition(audioAsset, Position, Loop, Volume, Attenuation);
        }
        else {
            Channel = AudioSys.PlayAudio(audioAsset, Loop, Volume);
        }
    }

    void Pause() {
        if (Channel) {
            // FMOD doesn't have a direct pause, so we stop for simplicity
            // In a more complete implementation, you might want to track pause state
            AudioSystem::GetInstance().Stop(Channel);
            Channel = 0;
        }
    }

    void Stop() {
        if (Channel) {
            AudioSystem::GetInstance().Stop(Channel);
            Channel = 0;
        }
    }

    // Called every frame (or when transform updates) to keep 3D channel position updated
    void UpdatePosition(const Vector3D& pos) {
        Position = pos;
        if (Spatialize && Channel) {
            AudioSystem::GetInstance().UpdateChannelPosition(Channel, Position);
        }
    }

    bool IsPlaying() {
        return Channel != 0 && AudioSystem::GetInstance().IsPlaying(Channel);
    }

    void SetVolume(float newVolume) {
        Volume = newVolume;
        if (Channel) {
            AudioSystem::GetInstance().SetChannelVolume(Channel, Volume);
        }
    }

    void SetPitch(float pitch) {
        if (Channel) {
            AudioSystem::GetInstance().SetChannelPitch(Channel, pitch);
        }
    }
};