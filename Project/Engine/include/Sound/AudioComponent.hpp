#pragma once
#include "Engine.h"
#include <string>
#include <memory>
#include "Sound/AudioManager.hpp"
#include "Math/Vector3D.hpp"
#include "Asset Manager/ResourceManager.hpp"

// Forward declare Audio class to avoid including the header
class Audio;

// AudioComponent: AudioSource component for ECS entities
// Mirrors Unity's AudioSource API and behavior patterns
struct ENGINE_API AudioComponent {
    // Unity-like public properties (Inspector editable)
    std::string Clip;            // Audio asset path (Unity: AudioClip reference)
    float Volume{ 1.0f };        // Volume multiplier (0.0 - 1.0)
    float Pitch{ 1.0f };         // Pitch multiplier (0.1 - 3.0)
    bool Loop{ false };          // Loop the audio
    bool PlayOnAwake{ false };   // Auto-play when entity is enabled (Unity naming)
    bool Mute{ false };          // Mute the audio source
    int Priority{ 128 };         // Channel priority (0-256, Unity standard)
    
    // 3D Audio Properties
    bool Spatialize{ false };    // Enable 3D spatial audio
    float MinDistance{ 1.0f };   // Distance for full volume
    float MaxDistance{ 100.0f }; // Distance for minimum volume
    float SpatialBlend{ 1.0f };  // 2D (0.0) to 3D (1.0) blend
    
    // Output routing
    std::string OutputAudioMixerGroup;  // Bus/Mixer group (Unity naming)

    // Runtime state (read-only in Unity inspector)
    bool IsPlaying{ false };
    bool IsPaused{ false };
    Vector3D Position{ 0.0f, 0.0f, 0.0f };

private:
    // Internal state
    ChannelHandle CurrentChannel{ 0 };
    std::shared_ptr<Audio> CachedAudioAsset{ nullptr };
    bool AssetLoaded{ false };
    bool WasPlayingBeforePause{ false };
    bool PlayOnAwakeTriggered{ false };

public:
    AudioComponent();
    ~AudioComponent();

    // Unity-like API
    void Play();
    void PlayDelayed(float delay);        // Unity: Play with delay
    void PlayOneShot(std::shared_ptr<Audio> clip = nullptr); // Unity: One-shot playback
    void PlayScheduled(double time);      // Unity: Scheduled playback (placeholder)
    void Stop();
    void Pause();
    void UnPause();
    
    // State queries
    bool GetIsPlaying() const;
    bool GetIsPaused() const;
    AudioSourceState GetState() const;
    
    // Property setters (with immediate effect if playing - Unity behavior)
    void SetVolume(float newVolume);
    void SetPitch(float newPitch);
    void SetLoop(bool shouldLoop);
    void SetMute(bool shouldMute);
    void SetSpatialize(bool enable);
    void SetSpatialBlend(float blend);
    void SetOutputAudioMixerGroup(const std::string& groupName);
    
    // Position updates (for spatial audio)
    void SetPosition(const Vector3D& pos);
    void OnTransformChanged(const Vector3D& newPosition);

    // Asset management
    void SetClip(const std::string& clipPath);
    void SetClip(std::shared_ptr<Audio> clip);
    bool HasValidClip() const;
    
    // For ECS AudioSystem integration
    void UpdateComponent();

private:
    // Internal helpers
    bool EnsureAssetLoaded();
    void UpdateChannelProperties();
    void UpdatePlaybackState();
    ChannelHandle PlayInternal(bool oneShot = false);
    void StopInternal();
};