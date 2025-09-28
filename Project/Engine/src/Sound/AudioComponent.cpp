#include "pch.h"
#include "Engine.h"
#include "Sound/AudioComponent.hpp"
#include "Sound/Audio.hpp"

// Define the constructor out-of-line to reduce header dependencies and recompile time.
AudioComponent::AudioComponent() {}

AudioComponent::~AudioComponent() {
    // Ensure we stop any playing channels
    if (Channel) {
        AudioSystem::GetInstance().Stop(Channel);
        Channel = 0;
    }
    // audioAsset will be automatically cleaned up by shared_ptr
    audioAsset = nullptr;
}

void AudioComponent::SetAudioAssetPath(const std::string& path) {
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

void AudioComponent::Play() {
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

void AudioComponent::Pause() {
    if (Channel) {
        // FMOD doesn't have a direct pause, so we stop for simplicity
        // In a more complete implementation, you might want to track pause state
        AudioSystem::GetInstance().Stop(Channel);
        Channel = 0;
    }
}

void AudioComponent::Stop() {
    if (Channel) {
        AudioSystem::GetInstance().Stop(Channel);
        Channel = 0;
    }
}

void AudioComponent::UpdatePosition(const Vector3D& pos) {
    Position = pos;
    if (Spatialize && Channel) {
        AudioSystem::GetInstance().UpdateChannelPosition(Channel, Position);
    }
}

bool AudioComponent::IsPlaying() {
    return Channel != 0 && AudioSystem::GetInstance().IsPlaying(Channel);
}

void AudioComponent::SetVolume(float newVolume) {
    Volume = newVolume;
    if (Channel) {
        AudioSystem::GetInstance().SetChannelVolume(Channel, Volume);
    }
}

void AudioComponent::SetPitch(float pitch) {
    if (Channel) {
        AudioSystem::GetInstance().SetChannelPitch(Channel, pitch);
    }
}
