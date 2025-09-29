#pragma once
#include "Engine.h"
#include <string>
#include <memory>
#include <iostream>
#include "Sound/AudioManager.hpp"
#include "Math/Vector3D.hpp"
#include "Asset Manager/ResourceManager.hpp"

// Forward declare Audio class to avoid including the header
class Audio;

struct ENGINE_API AudioComponent {
    // Unity-like public properties (Inspector editable)
    std::string AudioAssetPath;
    float Volume{ 1.0f };
    float Pitch{ 1.0f };
    bool Loop{ false };
    bool PlayOnStart{ false };
    bool Spatialize{ false };
    float MinDistance{ 1.0f };
    float MaxDistance{ 100.0f };
    float Attenuation{ 1.0f };
    bool Mute{ false };
    int Priority{ 128 }; // Unity-like priority (0-256)
    std::string BusName; // Channel group assignment

    // Runtime state (read-only)
    ChannelHandle CurrentChannel{ 0 };
    AudioSourceState State{ AudioSourceState::Stopped };
    Vector3D Position{ 0.0f, 0.0f, 0.0f };

private:
    // Internal cached asset
    std::shared_ptr<Audio> audioAsset{ nullptr };
    bool assetLoaded{ false };
    bool wasPlayingBeforePause{ false };
    bool playOnStartTriggered{ false };

public:
    AudioComponent();
    ~AudioComponent();

    // Unity-like API
    void Play();
    void PlayOneShot(); // Play without affecting current state
    void Stop();
    void Pause();
    void UnPause(); // Resume from pause
    
    // State queries
    bool IsPlaying() const;
    bool IsPaused() const;
    bool IsStopped() const;
    
    // Property setters (with immediate effect if playing)
    void SetVolume(float newVolume);
    void SetPitch(float newPitch);
    void SetLoop(bool shouldLoop);
    void SetMute(bool shouldMute);
    void SetSpatialize(bool enable);
    void SetPosition(const Vector3D& pos);
    void SetBus(const std::string& busName);

    // Asset management
    void SetAudioAssetPath(const std::string& path);
    bool HasValidAsset() const;
    
    // For ECS system integration
    void UpdateComponent(); // Called by AudioManager each frame
    void OnTransformChanged(const Vector3D& newPosition); // Called by transform system

private:
    // Internal helpers
    bool EnsureAssetLoaded();
    void UpdateChannelProperties();
    void UpdatePlaybackState();
    ChannelHandle PlayInternal(bool oneShot = false);
};