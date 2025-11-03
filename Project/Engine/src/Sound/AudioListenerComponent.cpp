#include "pch.h"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioManager.hpp"
#include "Logging.hpp"

#pragma region Reflection
REFL_REGISTER_START(AudioListenerComponent)
    REFL_REGISTER_PROPERTY(enabled)
    //REFL_REGISTER_PROPERTY(position)
REFL_REGISTER_END
#pragma endregion

AudioListenerComponent::AudioListenerComponent() {}

void AudioListenerComponent::UpdateComponent() {
    if (!enabled) return;
    UpdateListenerPosition();
}

void AudioListenerComponent::OnTransformChanged(const Vector3D& newPosition) {
    position = newPosition;
    UpdateListenerPosition();
}

void AudioListenerComponent::UpdateListenerPosition() {
    if (!enabled) return;
    //AudioManager::GetInstance().SetListenerPosition(position);
}