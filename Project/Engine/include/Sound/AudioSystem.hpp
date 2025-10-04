#pragma once

#include "ECS/SystemManager.hpp"
#include "Engine.h"

// AudioSystem: ECS system that manages AudioComponent updates
// Handles per-frame updates for all audio components and spatial audio synchronization
class AudioSystem : public System {
public:
    AudioSystem() = default;
    ~AudioSystem() = default;

    // System lifecycle
    void Update(float deltaTime);
    void OnEntityDestroyed(Entity entity);
    
private:
    // Track entities that need position updates
    void UpdateSpatialAudio();
};
