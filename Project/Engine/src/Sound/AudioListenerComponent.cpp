#include "pch.h"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioManager.hpp"
#include "Logging.hpp"

#pragma region Reflection
REFL_REGISTER_START(AudioListenerComponent)
    REFL_REGISTER_PROPERTY(enabled)
    // Removed: position, forward, up (now internal)
REFL_REGISTER_END
#pragma endregion

AudioListenerComponent::AudioListenerComponent() {
    // Initialize previous values to defaults
    previousPosition = position;
    previousForward = forward;
    previousUp = up;
}

void AudioListenerComponent::UpdateComponent() {
    if (!enabled) return;
    UpdateListenerPosition();
}

void AudioListenerComponent::OnTransformChanged(const Vector3D& newPosition, const Vector3D& newForward, const Vector3D& newUp) {
    // Check if any value has changed (optimization: only update FMOD if needed)
    bool hasChanged = (newPosition != previousPosition) || (newForward != previousForward) || (newUp != previousUp);
    if (!hasChanged) return;

    // Update internal state
    position = newPosition;
    forward = newForward;
    up = newUp;

    // Update previous values
    previousPosition = position;
    previousForward = forward;
    previousUp = up;

    UpdateListenerPosition();
}

void AudioListenerComponent::UpdateListenerPosition() {
    if (!enabled) return;
    AudioManager::GetInstance().SetListenerAttributes(0, position, Vector3D(0.0f, 0.0f, 0.0f), forward, up); // Listener 0, no velocity
}