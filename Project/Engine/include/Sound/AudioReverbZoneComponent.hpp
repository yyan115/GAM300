#pragma once
#include "Engine.h"
#include "Math/Vector3D.hpp"

// Forward declarations for FMOD types
typedef struct FMOD_REVERB3D FMOD_REVERB3D;

// AudioReverbZoneComponent: Defines a spherical reverb zone for spatial audio
// Similar to Unity's Audio Reverb Zone component
struct AudioReverbZoneComponent 
{
    REFL_SERIALIZABLE
    
    // Component enabled state
    bool enabled{ true };
    
    // Zone properties (Unity-like)
    float MinDistance{ 10.0f };   // Distance at which reverb starts (inner radius)
    float MaxDistance{ 30.0f };   // Distance at which reverb is at full effect (outer radius)
    
    // Reverb preset (Unity-like reverb types)
    enum class ReverbPreset {
        Off,
        Generic,
        PaddedCell,
        Room,
        Bathroom,
        LivingRoom,
        StoneRoom,
        Auditorium,
        ConcertHall,
        Cave,
        Arena,
        Hangar,
        CarpettedHallway,
        Hallway,
        StoneCorridor,
        Alley,
        Forest,
        City,
        Mountains,
        Quarry,
        Plain,
        ParkingLot,
        SewerPipe,
        Underwater,
        Drugged,
        Dizzy,
        Psychotic,
        Custom
    };
    
    // Reverb preset as integer for serialization (not directly exposed)
    int reverbPresetIndex{ static_cast<int>(ReverbPreset::Generic) };
    
    // Custom reverb parameters (used when reverbPreset is Custom)
    // Based on FMOD reverb properties
    float decayTime{ 1.49f };           // Decay time in seconds (0.1 to 20.0)
    float earlyDelay{ 0.007f };         // Early reflections delay in seconds (0.0 to 0.3)
    float lateDelay{ 0.011f };          // Late reverberation delay in seconds (0.0 to 0.1)
    float hfReference{ 5000.0f };       // High frequency reference in Hz (20.0 to 20000.0)
    float hfDecayRatio{ 0.83f };        // High frequency decay ratio (0.1 to 2.0)
    float diffusion{ 100.0f };          // Echo density in percent (0.0 to 100.0)
    float density{ 100.0f };            // Modal density in percent (0.0 to 100.0)
    float lowShelfFrequency{ 250.0f };  // Low shelf frequency in Hz (20.0 to 1000.0)
    float lowShelfGain{ 0.0f };         // Low shelf gain in dB (-36.0 to 12.0)
    float highCut{ 20000.0f };          // High cut frequency in Hz (20.0 to 20000.0)
    float earlyLateMix{ 50.0f };        // Early/Late mix percentage (0.0 to 100.0)
    float wetLevel{ -80.0f };           // Wet level in dB (-80.0 to 20.0)
    
    // Runtime state (read-only)
    Vector3D Position{ 0.0f, 0.0f, 0.0f };
    
    // FMOD reverb handle (not serialized)
    FMOD_REVERB3D* reverbHandle{ nullptr };
    int reverbInstanceIndex{ -1 };  // FMOD reverb instance index for tracking
    
    ENGINE_API AudioReverbZoneComponent();
    ENGINE_API ~AudioReverbZoneComponent();
    
    // Position updates (for zone tracking)
    void SetPosition(const Vector3D& pos);
    void OnTransformChanged(const Vector3D& newPosition);
    
    // Preset application
    void ApplyPreset(ReverbPreset preset);
    
    // Getter/Setter for reverb preset
    ReverbPreset GetReverbPreset() const { return static_cast<ReverbPreset>(reverbPresetIndex); }
    void SetReverbPreset(ReverbPreset preset);
    void ENGINE_API SetReverbPresetByIndex(int index);
    
    // FMOD Integration
    void ENGINE_API CreateReverbZone();
    void ENGINE_API ReleaseReverbZone();
    void ENGINE_API UpdateReverbZone();
    
    // For ECS AudioSystem integration
    void ENGINE_API UpdateComponent();
    
private:
    // Internal state tracking
    bool presetApplied{ false };
    bool needsUpdate{ true };
};
