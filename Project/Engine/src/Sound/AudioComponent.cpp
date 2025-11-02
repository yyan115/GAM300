#include "pch.h"
#include "Engine.h"
#include "Sound/AudioComponent.hpp"
#include "Sound/Audio.hpp"
#include "Logging.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"

#pragma region Reflection
REFL_REGISTER_START(AudioComponent)
    REFL_REGISTER_PROPERTY(enabled)
    REFL_REGISTER_PROPERTY(audioGUID)
    REFL_REGISTER_PROPERTY(Mute)
    REFL_REGISTER_PROPERTY(bypassListenerEffects)
    REFL_REGISTER_PROPERTY(PlayOnAwake)
    REFL_REGISTER_PROPERTY(Loop)
    REFL_REGISTER_PROPERTY(Priority)
    REFL_REGISTER_PROPERTY(Volume)
    REFL_REGISTER_PROPERTY(Pitch)
    REFL_REGISTER_PROPERTY(StereoPan)
    REFL_REGISTER_PROPERTY(reverbZoneMix)
	REFL_REGISTER_PROPERTY(Spatialize)
    REFL_REGISTER_PROPERTY(SpatialBlend)
    REFL_REGISTER_PROPERTY(DopplerLevel)
    REFL_REGISTER_PROPERTY(MinDistance)
    REFL_REGISTER_PROPERTY(MaxDistance)
REFL_REGISTER_END
#pragma endregion

AudioComponent::AudioComponent() {}

AudioComponent::~AudioComponent() {
    StopInternal();
    CachedAudioAsset = nullptr;
}

void AudioComponent::Play() {
    if (Mute) return;
    if (GetIsPlaying()) return;
    
    StopInternal();
    CurrentChannel = PlayInternal();
    if (CurrentChannel != 0) {
        IsPlaying = true;
        IsPaused = false;
        WasPlayingBeforePause = false;
    }
}

void AudioComponent::PlayDelayed(float delay) {
    (void)delay;
    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[AudioComponent] PlayDelayed not yet implemented\n");
    Play();
}

void AudioComponent::PlayOneShot(std::shared_ptr<Audio> clip) {
    if (Mute) return;
    
    std::shared_ptr<Audio> clipToPlay = clip ? clip : CachedAudioAsset;
    if (!clipToPlay && !EnsureAssetLoaded()) return;
    if (!clipToPlay) clipToPlay = CachedAudioAsset;
    
    AudioManager& audioMgr = AudioManager::GetInstance();
    ChannelHandle oneShotChannel;
    
    if (SpatialBlend > 0.0f) {
        oneShotChannel = audioMgr.PlayAudioAtPosition(clipToPlay, Position, false, Volume, SpatialBlend);
    } else if (!OutputAudioMixerGroup.empty()) {
        oneShotChannel = audioMgr.PlayAudioOnBus(clipToPlay, OutputAudioMixerGroup, false, Volume);
    } else {
        oneShotChannel = audioMgr.PlayAudio(clipToPlay, false, Volume);
    }
    
    if (oneShotChannel != 0) {
        audioMgr.SetChannelPitch(oneShotChannel, Pitch);
    }
}

void AudioComponent::PlayScheduled(double time) {
    (void)time;
    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[AudioComponent] PlayScheduled not yet implemented\n");
    Play();
}

void AudioComponent::Stop() {
    StopInternal();
    IsPlaying = false;
    IsPaused = false;
    WasPlayingBeforePause = false;
    PlayOnAwakeTriggered = false;
}

void AudioComponent::Pause() {
    if (CurrentChannel != 0 && GetIsPlaying()) {
        AudioManager::GetInstance().Pause(CurrentChannel);
        IsPlaying = false;
        IsPaused = true;
        WasPlayingBeforePause = true;
    }
}

void AudioComponent::UnPause() {
    if (CurrentChannel != 0 && GetIsPaused()) {
        AudioManager::GetInstance().Resume(CurrentChannel);
        IsPlaying = true;
        IsPaused = false;
        WasPlayingBeforePause = false;
    } else if (WasPlayingBeforePause && CurrentChannel == 0) {
        Play();
    }
}

bool AudioComponent::GetIsPlaying() const {
    if (CurrentChannel == 0) return false;
    return AudioManager::GetInstance().IsPlaying(CurrentChannel);
}

bool AudioComponent::GetIsPaused() const {
    return IsPaused;
}

AudioSourceState AudioComponent::GetState() const {
    if (CurrentChannel == 0) return AudioSourceState::Stopped;
    return AudioManager::GetInstance().GetState(CurrentChannel);
}

void AudioComponent::SetVolume(float newVolume) {
    Volume = std::clamp(newVolume, 0.0f, 1.0f);
    if (CurrentChannel != 0) {
        AudioManager::GetInstance().SetChannelVolume(CurrentChannel, Mute ? 0.0f : Volume);
    }
}

void AudioComponent::SetPitch(float newPitch) {
    Pitch = std::clamp(newPitch, 0.1f, 3.0f);
    if (CurrentChannel != 0) {
        AudioManager::GetInstance().SetChannelPitch(CurrentChannel, Pitch);
    }
}

void AudioComponent::SetLoop(bool shouldLoop) {
    Loop = shouldLoop;
    if (CurrentChannel != 0) {
        AudioManager::GetInstance().SetChannelLoop(CurrentChannel, Loop);
    }
}

void AudioComponent::SetMute(bool shouldMute) {
    Mute = shouldMute;
    if (CurrentChannel != 0) {
        AudioManager::GetInstance().SetChannelVolume(CurrentChannel, Mute ? 0.0f : Volume);
    }
}

void AudioComponent::SetSpatialBlend(float blend) {
    SpatialBlend = std::clamp(blend, 0.0f, 1.0f);
    if (CurrentChannel != 0) {
        UpdateChannelProperties();
    }
}

void AudioComponent::SetOutputAudioMixerGroup(const std::string& groupName) {
    if (OutputAudioMixerGroup != groupName) {
        bool wasPlaying = GetIsPlaying();
        if (wasPlaying) { Stop(); }
        OutputAudioMixerGroup = groupName;
        if (wasPlaying) { Play(); }
    }
}

void AudioComponent::SetPosition(const Vector3D& pos) {
    Position = pos;
    OnTransformChanged(pos);
}

void AudioComponent::OnTransformChanged(const Vector3D& newPosition) {
    Position = newPosition;
    if (CurrentChannel != 0 && SpatialBlend > 0.0f) {
        AudioManager::GetInstance().UpdateChannelPosition(CurrentChannel, Position);
    }
}

void AudioComponent::SetClip(const GUID_128& guid) {
    if (guid == audioGUID && AssetLoaded && CachedAudioAsset) return;

    StopInternal();
    audioGUID = guid;
    CachedAudioAsset = nullptr;
    AssetLoaded = false;
    IsPlaying = false;
    IsPaused = false;
    PlayOnAwakeTriggered = false;
}

void AudioComponent::SetClip(std::shared_ptr<Audio> clip) {
    StopInternal();
    CachedAudioAsset = clip;
    AssetLoaded = (clip != nullptr);
    audioGUID = clip ? AssetManager::GetInstance().GetGUID128FromAssetMeta(clip->assetPath) : GUID_128{0, 0};
    IsPlaying = false;
    IsPaused = false;
    PlayOnAwakeTriggered = false;
}

bool AudioComponent::HasValidClip() const {
    return CachedAudioAsset != nullptr && AssetLoaded;
}

void AudioComponent::UpdateComponent() {
    if (!AssetLoaded && (audioGUID.high != 0 || audioGUID.low != 0)) {
        EnsureAssetLoaded();
    }

    UpdatePlaybackState();

    if (PlayOnAwake) {
        if (!PlayOnAwakeTriggered && !GetIsPlaying() && HasValidClip() && !IsPlaying) {
            if (Engine::IsPlayMode()) {
                Play();
                PlayOnAwakeTriggered = true;
            }
        }
    } else {
        PlayOnAwakeTriggered = false;
    }
}

bool AudioComponent::EnsureAssetLoaded() {
    if (AssetLoaded && CachedAudioAsset) return true;
    if (audioGUID.high == 0 && audioGUID.low == 0) return false;

    try {
        std::string assetPath = AssetManager::GetInstance().GetAssetPathFromGUID(audioGUID);
        CachedAudioAsset = ResourceManager::GetInstance().GetResourceFromGUID<Audio>(audioGUID, assetPath);
        AssetLoaded = (CachedAudioAsset != nullptr);
        if (!AssetLoaded) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioComponent] Failed to load audio via GUID: ", GUIDUtilities::ConvertGUID128ToString(audioGUID), "\n");
        }
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioComponent] Exception loading audio via GUID ", GUIDUtilities::ConvertGUID128ToString(audioGUID), ": ", e.what(), "\n");
        AssetLoaded = false;
    }
    return AssetLoaded;
}

void AudioComponent::UpdateChannelProperties() {
    if (CurrentChannel == 0) return;
    AudioManager& audioMgr = AudioManager::GetInstance();
    audioMgr.SetChannelVolume(CurrentChannel, Mute ? 0.0f : Volume);
    audioMgr.SetChannelPitch(CurrentChannel, Pitch);
    audioMgr.SetChannelLoop(CurrentChannel, Loop);
    if (SpatialBlend > 0.0f) {
        audioMgr.UpdateChannelPosition(CurrentChannel, Position);
    }
}

void AudioComponent::UpdatePlaybackState() {
    if (CurrentChannel == 0) {
        if (IsPlaying || IsPaused) {
            IsPlaying = false;
            IsPaused = false;
        }
        return;
    }
    
    AudioSourceState actualState = AudioManager::GetInstance().GetState(CurrentChannel);
    if (actualState == AudioSourceState::Stopped) {
        CurrentChannel = 0;
        IsPlaying = false;
        IsPaused = false;
        WasPlayingBeforePause = false;
    } else {
        IsPlaying = (actualState == AudioSourceState::Playing);
        IsPaused = (actualState == AudioSourceState::Paused);
    }
}

ChannelHandle AudioComponent::PlayInternal(bool oneShot) {
    if (!EnsureAssetLoaded()) return 0;
    
    AudioManager& audioMgr = AudioManager::GetInstance();
    ChannelHandle channel = 0;
    
    if (SpatialBlend > 0.0f) {
        channel = audioMgr.PlayAudioAtPosition(CachedAudioAsset, Position, Loop && !oneShot, Volume, SpatialBlend);
    } else if (!OutputAudioMixerGroup.empty()) {
        channel = audioMgr.PlayAudioOnBus(CachedAudioAsset, OutputAudioMixerGroup, Loop && !oneShot, Volume);
    } else {
        channel = audioMgr.PlayAudio(CachedAudioAsset, Loop && !oneShot, Volume);
    }
    
    if (channel != 0) {
        audioMgr.SetChannelPitch(channel, Pitch);
        if (Mute) {
            audioMgr.SetChannelVolume(channel, 0.0f);
        }
    }
    return channel;
}

void AudioComponent::StopInternal() {
    if (CurrentChannel != 0) {
        AudioManager::GetInstance().Stop(CurrentChannel);
        CurrentChannel = 0;
    }
}