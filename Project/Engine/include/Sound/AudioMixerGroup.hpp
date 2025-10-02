#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Reflection/ReflectionBase.hpp"
#include "../Engine.h"

// Forward declarations
typedef struct FMOD_CHANNELGROUP FMOD_CHANNELGROUP;
class AudioMixer;

/**
 * @brief Represents an audio mixer group (bus) in the audio hierarchy.
 * 
 * AudioMixerGroups follow Unity's standard, allowing hierarchical control over
 * multiple audio sources. Each group can have child groups and parent groups,
 * enabling complex audio mixing setups with volume, pitch, and effect control.
 */
class ENGINE_API AudioMixerGroup {
public:
    REFL_SERIALIZABLE

    AudioMixerGroup(const std::string& groupName, AudioMixer* ownerMixer);
    AudioMixerGroup(); // Default constructor for reflection
    ~AudioMixerGroup();

    // Core properties
    const std::string& GetName() const;
    void SetName(const std::string& newName);

    // Volume control (0.0 to 1.0)
    void SetVolume(float volumeLevel);
    float GetVolume() const;

    // Pitch control (0.5 to 2.0, 1.0 is normal)
    void SetPitch(float pitchLevel);
    float GetPitch() const;

    // Mute/Solo controls
    void SetMuted(bool muteState);
    bool IsMuted() const;
    
    void SetSolo(bool soloState);
    bool IsSolo() const;

    // Pause control
    void SetPaused(bool pauseState);
    bool IsPaused() const;

    // Hierarchy management
    void SetParent(AudioMixerGroup* parentGroup);
    AudioMixerGroup* GetParent() const;
    
    void AddChild(AudioMixerGroup* childGroup);
    void RemoveChild(AudioMixerGroup* childGroup);
    const std::vector<AudioMixerGroup*>& GetChildren() const;

    // FMOD channel group access (for internal use)
    FMOD_CHANNELGROUP* GetFMODChannelGroup() const;
    void SetFMODChannelGroup(FMOD_CHANNELGROUP* channelGroup);

    // Get the full path of this group (e.g., "Master/Music/Ambient")
    std::string GetFullPath() const;

    // Owner mixer
    AudioMixer* GetOwnerMixer() const;
    void SetOwnerMixer(AudioMixer* mixer);

private:
    std::string Name;
    AudioMixer* OwnerMixer;
    AudioMixerGroup* Parent;
    std::vector<AudioMixerGroup*> Children;
    
    // Audio properties
    float Volume;
    float Pitch;
    bool Muted;
    bool Solo;
    bool Paused;

    // FMOD backend (not serialized)
    FMOD_CHANNELGROUP* FmodChannelGroup;

    // Helper to update FMOD state
    void UpdateFMODState();
};
