#include "pch.h"
#include "Sound/AudioMixerGroup.hpp"
#include "Sound/AudioMixer.hpp"
#include "Sound/AudioManager.hpp"
#include <fmod.h>
#include <fmod_errors.h>
#include "Logging.hpp"

void AudioMixerGroup::UpdateFMODState() {
    if (!FmodChannelGroup) {
        return;
    }

    // Apply volume (accounting for mute)
    float finalVolume = Muted ? 0.0f : Volume;
    FMOD_ChannelGroup_SetVolume(FmodChannelGroup, finalVolume);

    // Apply pitch
    FMOD_ChannelGroup_SetPitch(FmodChannelGroup, Pitch);

    // Apply pause
    FMOD_ChannelGroup_SetPaused(FmodChannelGroup, Paused);
}

#pragma region Reflection
REFL_REGISTER_START(AudioMixerGroup)
    REFL_REGISTER_PROPERTY(Name)
    REFL_REGISTER_PROPERTY(Volume)
    REFL_REGISTER_PROPERTY(Pitch)
    REFL_REGISTER_PROPERTY(Muted)
    REFL_REGISTER_PROPERTY(Solo)
    REFL_REGISTER_PROPERTY(Paused)
REFL_REGISTER_END
#pragma endregion

AudioMixerGroup::AudioMixerGroup()
    : Name("Unnamed")
    , OwnerMixer(nullptr)
    , Parent(nullptr)
    , Volume(1.0f)
    , Pitch(1.0f)
    , Muted(false)
    , Solo(false)
    , Paused(false)
    , FmodChannelGroup(nullptr)
{
}

AudioMixerGroup::AudioMixerGroup(const std::string& groupName, AudioMixer* ownerMixer)
    : Name(groupName)
    , OwnerMixer(ownerMixer)
    , Parent(nullptr)
    , Volume(1.0f)
    , Pitch(1.0f)
    , Muted(false)
    , Solo(false)
    , Paused(false)
    , FmodChannelGroup(nullptr)
{
    // Create FMOD channel group
    if (OwnerMixer) {
        FMOD_SYSTEM* fmodSystem = OwnerMixer->GetFMODSystem();
        if (fmodSystem) {
            FMOD_RESULT result = FMOD_System_CreateChannelGroup(fmodSystem, Name.c_str(), &FmodChannelGroup);
            if (result != FMOD_OK) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, 
                    "[AudioMixerGroup] Failed to create FMOD channel group: ", FMOD_ErrorString(result), "\n");
            }
        }
    }
}

AudioMixerGroup::~AudioMixerGroup() {
    // Release FMOD channel group
    if (FmodChannelGroup) {
        FMOD_ChannelGroup_Release(FmodChannelGroup);
        FmodChannelGroup = nullptr;
    }
}

const std::string& AudioMixerGroup::GetName() const {
    return Name;
}

void AudioMixerGroup::SetName(const std::string& newName) {
    Name = newName;
}

void AudioMixerGroup::SetVolume(float volumeLevel) {
    Volume = std::clamp(volumeLevel, 0.0f, 1.0f);
    UpdateFMODState();
}

float AudioMixerGroup::GetVolume() const {
    return Volume;
}

void AudioMixerGroup::SetPitch(float pitchLevel) {
    Pitch = std::clamp(pitchLevel, 0.5f, 2.0f);
    UpdateFMODState();
}

float AudioMixerGroup::GetPitch() const {
    return Pitch;
}

void AudioMixerGroup::SetMuted(bool muteState) {
    Muted = muteState;
    UpdateFMODState();
}

bool AudioMixerGroup::IsMuted() const {
    return Muted;
}

void AudioMixerGroup::SetSolo(bool soloState) {
    Solo = soloState;
    UpdateFMODState();
}

bool AudioMixerGroup::IsSolo() const {
    return Solo;
}

void AudioMixerGroup::SetPaused(bool pauseState) {
    Paused = pauseState;
    UpdateFMODState();
}

bool AudioMixerGroup::IsPaused() const {
    return Paused;
}

void AudioMixerGroup::SetParent(AudioMixerGroup* parentGroup) {
    // Remove from current parent
    if (Parent) {
        Parent->RemoveChild(this);
    }

    // Set new parent
    Parent = parentGroup;
    
    // Add to new parent's children
    if (Parent) {
        Parent->AddChild(this);
        
        // Update FMOD hierarchy
        if (FmodChannelGroup && Parent->FmodChannelGroup) {
            FMOD_ChannelGroup_AddGroup(Parent->FmodChannelGroup, FmodChannelGroup, false, nullptr);
        }
    }
}

AudioMixerGroup* AudioMixerGroup::GetParent() const {
    return Parent;
}

void AudioMixerGroup::AddChild(AudioMixerGroup* childGroup) {
    if (childGroup && std::find(Children.begin(), Children.end(), childGroup) == Children.end()) {
        Children.push_back(childGroup);
    }
}

void AudioMixerGroup::RemoveChild(AudioMixerGroup* childGroup) {
    auto it = std::find(Children.begin(), Children.end(), childGroup);
    if (it != Children.end()) {
        Children.erase(it);
    }
}

const std::vector<AudioMixerGroup*>& AudioMixerGroup::GetChildren() const {
    return Children;
}

FMOD_CHANNELGROUP* AudioMixerGroup::GetFMODChannelGroup() const {
    return FmodChannelGroup;
}

void AudioMixerGroup::SetFMODChannelGroup(FMOD_CHANNELGROUP* channelGroup) {
    FmodChannelGroup = channelGroup;
}

AudioMixer* AudioMixerGroup::GetOwnerMixer() const {
    return OwnerMixer;
}

void AudioMixerGroup::SetOwnerMixer(AudioMixer* mixer) {
    OwnerMixer = mixer;
}

std::string AudioMixerGroup::GetFullPath() const {
    if (!Parent) {
        return Name; // Root group
    }
    
    std::string parentPath = Parent->GetFullPath();
    return parentPath + "/" + Name;
}
