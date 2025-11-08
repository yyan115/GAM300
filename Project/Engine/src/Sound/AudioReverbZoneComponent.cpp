#include "pch.h"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "Logging.hpp"

#pragma region Reflection
REFL_REGISTER_START(AudioReverbZoneComponent)
    REFL_REGISTER_PROPERTY(enabled)
    REFL_REGISTER_PROPERTY(MinDistance)
    REFL_REGISTER_PROPERTY(MaxDistance)
    REFL_REGISTER_PROPERTY(reverbPresetIndex)
    REFL_REGISTER_PROPERTY(decayTime)
    REFL_REGISTER_PROPERTY(earlyDelay)
    REFL_REGISTER_PROPERTY(lateDelay)
    REFL_REGISTER_PROPERTY(hfReference)
    REFL_REGISTER_PROPERTY(hfDecayRatio)
    REFL_REGISTER_PROPERTY(diffusion)
    REFL_REGISTER_PROPERTY(density)
    REFL_REGISTER_PROPERTY(lowShelfFrequency)
    REFL_REGISTER_PROPERTY(lowShelfGain)
    REFL_REGISTER_PROPERTY(highCut)
    REFL_REGISTER_PROPERTY(earlyLateMix)
    REFL_REGISTER_PROPERTY(wetLevel)
REFL_REGISTER_END
#pragma endregion

AudioReverbZoneComponent::AudioReverbZoneComponent() {
    // Apply default preset on construction
    ApplyPreset(GetReverbPreset());
}

AudioReverbZoneComponent::~AudioReverbZoneComponent() {
    // Cleanup if needed
}

void AudioReverbZoneComponent::SetPosition(const Vector3D& pos) {
    Position = pos;
    OnTransformChanged(pos);
}

void AudioReverbZoneComponent::OnTransformChanged(const Vector3D& newPosition) {
    Position = newPosition;
    // Future: Update FMOD reverb zone position if needed
}

void AudioReverbZoneComponent::ApplyPreset(ReverbPreset preset) {
    reverbPresetIndex = static_cast<int>(preset);
    
    // Apply FMOD reverb preset parameters based on Unity-like presets
    // These values are approximations based on typical reverb characteristics
    switch (preset) {
        case ReverbPreset::Off:
            wetLevel = -80.0f;
            break;
            
        case ReverbPreset::Generic:
            decayTime = 1.49f;
            earlyDelay = 0.007f;
            lateDelay = 0.011f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.83f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::PaddedCell:
            decayTime = 0.17f;
            earlyDelay = 0.001f;
            lateDelay = 0.002f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.10f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 5000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -3.0f;
            break;
            
        case ReverbPreset::Room:
            decayTime = 0.40f;
            earlyDelay = 0.002f;
            lateDelay = 0.003f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.83f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Bathroom:
            decayTime = 1.40f;
            earlyDelay = 0.007f;
            lateDelay = 0.011f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.54f;
            diffusion = 100.0f;
            density = 60.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 5000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -4.0f;
            break;
            
        case ReverbPreset::LivingRoom:
            decayTime = 0.50f;
            earlyDelay = 0.003f;
            lateDelay = 0.004f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.10f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::StoneRoom:
            decayTime = 2.31f;
            earlyDelay = 0.012f;
            lateDelay = 0.017f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.64f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Auditorium:
            decayTime = 4.32f;
            earlyDelay = 0.020f;
            lateDelay = 0.030f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.59f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::ConcertHall:
            decayTime = 3.92f;
            earlyDelay = 0.020f;
            lateDelay = 0.029f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.70f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Cave:
            decayTime = 2.91f;
            earlyDelay = 0.015f;
            lateDelay = 0.022f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.83f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Arena:
            decayTime = 7.24f;
            earlyDelay = 0.020f;
            lateDelay = 0.030f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.33f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 30.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Hangar:
            decayTime = 10.05f;
            earlyDelay = 0.020f;
            lateDelay = 0.030f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.23f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 30.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::CarpettedHallway:
            decayTime = 0.30f;
            earlyDelay = 0.002f;
            lateDelay = 0.003f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.10f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 5000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Hallway:
            decayTime = 1.49f;
            earlyDelay = 0.007f;
            lateDelay = 0.011f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.59f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::StoneCorridor:
            decayTime = 2.70f;
            earlyDelay = 0.013f;
            lateDelay = 0.020f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.79f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 50.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Alley:
            decayTime = 1.49f;
            earlyDelay = 0.007f;
            lateDelay = 0.011f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.86f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Forest:
            decayTime = 1.49f;
            earlyDelay = 0.162f;
            lateDelay = 0.088f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.54f;
            diffusion = 79.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 5000.0f;
            earlyLateMix = 30.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::City:
            decayTime = 1.49f;
            earlyDelay = 0.007f;
            lateDelay = 0.011f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.67f;
            diffusion = 50.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = -3.0f;
            highCut = 5000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Mountains:
            decayTime = 1.49f;
            earlyDelay = 0.300f;
            lateDelay = 0.100f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.21f;
            diffusion = 27.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = -6.0f;
            highCut = 5000.0f;
            earlyLateMix = 20.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Quarry:
            decayTime = 1.49f;
            earlyDelay = 0.061f;
            lateDelay = 0.025f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.83f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 30.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::Plain:
            decayTime = 1.49f;
            earlyDelay = 0.179f;
            lateDelay = 0.100f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.50f;
            diffusion = 21.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 5000.0f;
            earlyLateMix = 20.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::ParkingLot:
            decayTime = 1.65f;
            earlyDelay = 0.008f;
            lateDelay = 0.012f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.87f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -6.0f;
            break;
            
        case ReverbPreset::SewerPipe:
            decayTime = 2.81f;
            earlyDelay = 0.014f;
            lateDelay = 0.021f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.14f;
            diffusion = 80.0f;
            density = 60.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 5000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -4.0f;
            break;
            
        case ReverbPreset::Underwater:
            decayTime = 1.49f;
            earlyDelay = 0.007f;
            lateDelay = 0.011f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.10f;
            diffusion = 100.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 500.0f;
            earlyLateMix = 50.0f;
            wetLevel = -3.0f;
            break;
            
        case ReverbPreset::Drugged:
            decayTime = 8.39f;
            earlyDelay = 0.002f;
            lateDelay = 0.003f;
            hfReference = 5000.0f;
            hfDecayRatio = 1.39f;
            diffusion = 50.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 40.0f;
            wetLevel = -3.0f;
            break;
            
        case ReverbPreset::Dizzy:
            decayTime = 17.23f;
            earlyDelay = 0.020f;
            lateDelay = 0.030f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.56f;
            diffusion = 50.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 30.0f;
            wetLevel = -3.0f;
            break;
            
        case ReverbPreset::Psychotic:
            decayTime = 7.56f;
            earlyDelay = 0.020f;
            lateDelay = 0.030f;
            hfReference = 5000.0f;
            hfDecayRatio = 0.91f;
            diffusion = 50.0f;
            density = 100.0f;
            lowShelfFrequency = 250.0f;
            lowShelfGain = 0.0f;
            highCut = 20000.0f;
            earlyLateMix = 25.0f;
            wetLevel = -3.0f;
            break;
            
        case ReverbPreset::Custom:
            // Don't modify parameters for custom preset
            break;
    }
    
    presetApplied = true;
}

void AudioReverbZoneComponent::SetReverbPreset(ReverbPreset preset) {
    if (GetReverbPreset() != preset) {
        ApplyPreset(preset);
    }
}

void AudioReverbZoneComponent::SetReverbPresetByIndex(int index) {
    if (index >= 0 && index <= static_cast<int>(ReverbPreset::Custom)) {
        SetReverbPreset(static_cast<ReverbPreset>(index));
    }
}

void AudioReverbZoneComponent::UpdateComponent() {
    if (!enabled) return;
    
    // Apply preset if not yet applied
    if (!presetApplied) {
        ApplyPreset(GetReverbPreset());
    }
    
    // Future: Update FMOD reverb zone if needed
}
