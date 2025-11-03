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
struct AudioComponent 
{
    REFL_SERIALIZABLE
    // Unity-like public properties (Inspector editable)
    bool enabled{ true };        // Component enabled state (can be toggled in inspector)
    GUID_128 audioGUID{};          // Audio asset GUID
    bool Mute{ false };          // Mute the audio source
	bool bypassListenerEffects{ false }; // Bypass listener effects 
    bool PlayOnAwake{ false };   // Auto-play when entity is enabled 
    bool Loop{ false };          // Loop the audio
    int Priority{ 128 };         // Channel priority (0-256)
    float Volume{ 1.0f };        // Volume multiplier (0.0 - 1.0)
    float Pitch{ 1.0f };         // Pitch multiplier (0.1 - 3.0)
	float StereoPan{ 0.0f };     // -1.0 (left) to 1.0 (right) panning
	float reverbZoneMix{ 1.0f }; // Reverb zone mix level   
    
    // 3D Audio Properties
    bool Spatialize{ false };    // Enable 3D spatial audio
    float SpatialBlend{ 0.0f };  // 2D (0.0) to 3D (1.0) blend
	float DopplerLevel{ 1.0f };   // Doppler effect level
    float MinDistance{ 1.0f };   // Distance for full volume
    float MaxDistance{ 100.0f }; // Distance for minimum volume    
    
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
    ENGINE_API AudioComponent();
    ENGINE_API ~AudioComponent();

    // Unity-like API
    void ENGINE_API Play();
    void PlayDelayed(float delay);        // Unity: Play with delay
    void PlayOneShot(std::shared_ptr<Audio> clip = nullptr); // Unity: One-shot playback
    void PlayScheduled(double time);      // Unity: Scheduled playback (placeholder)
    void ENGINE_API Stop();
    void ENGINE_API Pause();
    void ENGINE_API UnPause();
    
    // State queries
    bool GetIsPlaying() const;
    bool GetIsPaused() const;
    AudioSourceState GetState() const;
    
    // Property setters (with immediate effect if playing - Unity behavior)
    void SetVolume(float newVolume);
    void SetPitch(float newPitch);
    void SetLoop(bool shouldLoop);
    void SetMute(bool shouldMute);
    void ENGINE_API SetSpatialBlend(float blend);
    void SetOutputAudioMixerGroup(const std::string& groupName);
    
    // Position updates (for spatial audio)
    void SetPosition(const Vector3D& pos);
    void OnTransformChanged(const Vector3D& newPosition);

    // Asset management
    void ENGINE_API SetClip(const GUID_128& guid);
    void SetClip(std::shared_ptr<Audio> clip);
    bool HasValidClip() const;
    
    // For ECS AudioSystem integration
    void ENGINE_API UpdateComponent();

private:
    // Internal helpers
    bool EnsureAssetLoaded();
    void UpdateChannelProperties();
    void UpdatePlaybackState();
    ChannelHandle PlayInternal(bool oneShot = false);
    void StopInternal();
};