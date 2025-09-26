#pragma once
#include <string>
#include "Sound/AudioSystem.hpp"
#include "Math/Vector3D.hpp"

struct AudioComponent {
    // Public editable properties used by inspector
    std::string AudioAssetPath; // original asset path under Resources
    AudioHandle AudioHandle{0};
    float Volume{1.0f};
    bool Loop{false};
    bool PlayOnAwake{false};
    bool Spatialize{false};
    float Attenuation{1.0f};

    // Runtime channel
    ChannelHandle Channel{0};

    // Optional position - when spatialize is enabled inspector / transform system should update this
    Vector3D Position{0.0f, 0.0f, 0.0f};

    AudioComponent() {}

    ~AudioComponent() {
        // Ensure we release audio resource
        if (Channel) {
            AudioSystem::GetInstance().Stop(Channel);
            Channel = 0;
        }
        if (AudioHandle) {
            AudioSystem::GetInstance().UnloadAudio(AudioHandle);
            AudioHandle = 0;
        }
    }

    // Called when inspector sets a new asset path
    void SetAudioAssetPath(const std::string& path) {
        if (path == AudioAssetPath) return;
        // Release old
        if (Channel) {
            AudioSystem::GetInstance().Stop(Channel);
            Channel = 0;
        }
        if (AudioHandle) {
            AudioSystem::GetInstance().UnloadAudio(AudioHandle);
            AudioHandle = 0;
        }

        AudioAssetPath = path;
        if (!AudioAssetPath.empty()) {
            AudioHandle = AudioSystem::GetInstance().LoadAudio(AudioAssetPath);
            if (PlayOnAwake && AudioHandle) {
                if (Spatialize)
                    Channel = AudioSystem::GetInstance().PlayAtPosition(AudioHandle, Position, Loop, Volume, Attenuation);
                else
                    Channel = AudioSystem::GetInstance().Play(AudioHandle, Loop, Volume);
            }
        }
    }

    void Play() {
        if (!AudioHandle && !AudioAssetPath.empty()) {
            AudioHandle = AudioSystem::GetInstance().LoadAudio(AudioAssetPath);
        }
        if (AudioHandle) {
            if (Spatialize)
                Channel = AudioSystem::GetInstance().PlayAtPosition(AudioHandle, Position, Loop, Volume, Attenuation);
            else
                Channel = AudioSystem::GetInstance().Play(AudioHandle, Loop, Volume);
        }
    }

    void Pause() {
        if (Channel) {
            // Pause by setting volume to 0 or using FMOD Channel pause - use Stop for simplicity here
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
};