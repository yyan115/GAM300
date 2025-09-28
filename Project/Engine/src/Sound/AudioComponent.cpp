#include "pch.h"
#include "Engine.h"
#include "Sound/AudioComponent.hpp"
#include "Sound/Audio.hpp"
#include "Logging.hpp"

AudioComponent::AudioComponent() {}

AudioComponent::~AudioComponent() {
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().Stop(CurrentChannel);
        CurrentChannel = 0;
    }
    audioAsset = nullptr;
}

void AudioComponent::Play() {
    if (Mute) return;
    if (IsPlaying()) {
        return;
    }
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().Stop(CurrentChannel);
        CurrentChannel = 0;
    }
    CurrentChannel = PlayInternal();
    if (CurrentChannel != 0) {
        State = AudioSourceState::Playing;
        wasPlayingBeforePause = false;
    }
}

void AudioComponent::PlayOneShot() {
    if (Mute || !EnsureAssetLoaded()) return;
    AudioSystem& audioSys = AudioSystem::GetInstance();
    ChannelHandle oneShotChannel;
    if (Spatialize) {
        oneShotChannel = audioSys.PlayAudioAtPosition(audioAsset, Position, false, Volume, Attenuation);
    }
    else if (!BusName.empty()) {
        oneShotChannel = audioSys.PlayAudioOnBus(audioAsset, BusName, false, Volume);
    }
    else {
        oneShotChannel = audioSys.PlayAudio(audioAsset, false, Volume);
    }
    if (oneShotChannel != 0) {
        audioSys.SetChannelPitch(oneShotChannel, Pitch);
    }
}

void AudioComponent::Stop() {
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().Stop(CurrentChannel);
        CurrentChannel = 0;
    }
    State = AudioSourceState::Stopped;
    wasPlayingBeforePause = false;
}

void AudioComponent::Pause() {
    if (CurrentChannel != 0 && IsPlaying()) {
        AudioSystem::GetInstance().Pause(CurrentChannel);
        State = AudioSourceState::Paused;
        wasPlayingBeforePause = true;
    }
}

void AudioComponent::UnPause() {
    if (CurrentChannel != 0 && IsPaused()) {
        AudioSystem::GetInstance().Resume(CurrentChannel);
        State = AudioSourceState::Playing;
        wasPlayingBeforePause = false;
    }
    else if (wasPlayingBeforePause && CurrentChannel == 0) {
        Play();
    }
}

bool AudioComponent::IsPlaying() const {
    if (CurrentChannel == 0) return false;
    return AudioSystem::GetInstance().IsPlaying(CurrentChannel) && State == AudioSourceState::Playing;
}

bool AudioComponent::IsPaused() const { return State == AudioSourceState::Paused; }

bool AudioComponent::IsStopped() const { return State == AudioSourceState::Stopped || CurrentChannel == 0; }

void AudioComponent::SetVolume(float newVolume) {
    Volume = std::clamp(newVolume, 0.0f, 1.0f);
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().SetChannelVolume(CurrentChannel, Mute ? 0.0f : Volume);
    }
}

void AudioComponent::SetPitch(float newPitch) {
    Pitch = std::clamp(newPitch, 0.1f, 3.0f);
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().SetChannelPitch(CurrentChannel, Pitch);
    }
}

void AudioComponent::SetLoop(bool shouldLoop) {
    Loop = shouldLoop;
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().SetChannelLoop(CurrentChannel, Loop);
    }
}

void AudioComponent::SetMute(bool shouldMute) {
    Mute = shouldMute;
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().SetChannelVolume(CurrentChannel, Mute ? 0.0f : Volume);
    }
}

void AudioComponent::SetSpatialize(bool enable) {
    bool wasPlaying = IsPlaying();
    if (wasPlaying && Spatialize != enable) {
        Stop();
        Spatialize = enable;
        Play();
    }
    else {
        Spatialize = enable;
    }
}

void AudioComponent::SetPosition(const Vector3D& pos) {
    Position = pos;
    OnTransformChanged(pos);
}

void AudioComponent::SetBus(const std::string& busName) {
    if (BusName != busName) {
        bool wasPlaying = IsPlaying();
        if (wasPlaying) { Stop(); }
        BusName = busName;
        if (wasPlaying) { Play(); }
    }
}

void AudioComponent::SetAudioAssetPath(const std::string& path) {
    // Only skip if the path is the same AND the asset is already loaded
    if (path == AudioAssetPath && assetLoaded && audioAsset) {
        return;
    }
    if (CurrentChannel != 0) {
        AudioSystem::GetInstance().Stop(CurrentChannel);
        CurrentChannel = 0;
    }
    AudioAssetPath = path;
    audioAsset = nullptr;
    assetLoaded = false;
    State = AudioSourceState::Stopped;
    // Don't load or play immediately - let UpdateComponent handle it based on game state
}

bool AudioComponent::HasValidAsset() const { return audioAsset != nullptr && assetLoaded; }

void AudioComponent::UpdateComponent() {
    // Ensure asset is loaded if we have a valid path
    if (!AudioAssetPath.empty() && !HasValidAsset()) {
        EnsureAssetLoaded();
    }

    UpdatePlaybackState();

    // Only handle PlayOnAwake if we're in PLAY_MODE
    if (PlayOnStart && !IsPlaying() && HasValidAsset() && State == AudioSourceState::Stopped) {
        if (Engine::IsPlayMode()) {
            Play();
        }
    }
}

void AudioComponent::OnTransformChanged(const Vector3D& newPosition) {
    Position = newPosition;
    if (CurrentChannel != 0 && Spatialize) {
        AudioSystem::GetInstance().UpdateChannelPosition(CurrentChannel, Position);
    }
}

bool AudioComponent::EnsureAssetLoaded() {
    if (assetLoaded && audioAsset) return true;
    if (AudioAssetPath.empty()) return false;

    try {
        // Use forceLoad=false to prevent unnecessary reloads during hot-reload
        audioAsset = ResourceManager::GetInstance().GetResource<Audio>(AudioAssetPath, false);
        assetLoaded = (audioAsset != nullptr);
        if (!assetLoaded) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioComponent] Failed to load audio asset: ", AudioAssetPath, "\n");
        }
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AudioComponent] Exception loading audio asset ", AudioAssetPath, ": ", e.what(), "\n");
        assetLoaded = false;
    }
    return assetLoaded;
}

void AudioComponent::UpdateChannelProperties() {
    if (CurrentChannel == 0) return;
    AudioSystem& audioSys = AudioSystem::GetInstance();
    audioSys.SetChannelVolume(CurrentChannel, Mute ? 0.0f : Volume);
    audioSys.SetChannelPitch(CurrentChannel, Pitch);
    audioSys.SetChannelLoop(CurrentChannel, Loop);
    if (Spatialize) { audioSys.UpdateChannelPosition(CurrentChannel, Position); }
}

void AudioComponent::UpdatePlaybackState() {
    if (CurrentChannel == 0) {
        if (State != AudioSourceState::Stopped) { State = AudioSourceState::Stopped; }
        return;
    }
    AudioSourceState actualState = AudioSystem::GetInstance().GetState(CurrentChannel);
    if (actualState == AudioSourceState::Stopped && State != AudioSourceState::Stopped) {
        CurrentChannel = 0;
        State = AudioSourceState::Stopped;
        wasPlayingBeforePause = false;
    }
    else if (actualState != State) {
        State = actualState;
    }
}

ChannelHandle AudioComponent::PlayInternal(bool oneShot) {
    if (!EnsureAssetLoaded()) { return 0; }
    AudioSystem& audioSys = AudioSystem::GetInstance();
    ChannelHandle channel = 0;
    if (Spatialize) {
        channel = audioSys.PlayAudioAtPosition(audioAsset, Position, Loop && !oneShot, Volume, Attenuation);
    }
    else if (!BusName.empty()) {
        channel = audioSys.PlayAudioOnBus(audioAsset, BusName, Loop && !oneShot, Volume);
    }
    else {
        channel = audioSys.PlayAudio(audioAsset, Loop && !oneShot, Volume);
    }
    if (channel != 0) {
        audioSys.SetChannelPitch(channel, Pitch);
        if (Mute) { audioSys.SetChannelVolume(channel, 0.0f); }
    }
    return channel;
}