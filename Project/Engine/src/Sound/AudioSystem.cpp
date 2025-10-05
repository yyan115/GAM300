#include "pch.h"
#include "Sound/AudioSystem.hpp"
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioManager.hpp"
#include "Transform/TransformComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Performance/PerformanceProfiler.hpp"

void AudioSystem::Update(float deltaTime) {
	PROFILE_FUNCTION();
	(void)deltaTime; // Unused for now
    // First, update the AudioManager's internal FMOD system
    AudioManager::GetInstance().Update();
    
    // Then update all AudioComponents
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    
    // Iterate through all entities managed by this system
    for (const auto& entity : entities) {
        if (!ecsManager.HasComponent<AudioComponent>(entity)) continue;
        
        AudioComponent& audioComp = ecsManager.GetComponent<AudioComponent>(entity);
        audioComp.UpdateComponent();
        
        // Update spatial audio position from Transform if applicable
        if (audioComp.Spatialize && ecsManager.HasComponent<Transform>(entity)) {
            const Transform& transform = ecsManager.GetComponent<Transform>(entity);
            audioComp.OnTransformChanged(transform.localPosition);
        }
    }
}

void AudioSystem::OnEntityDestroyed(Entity entity) {
    // Cleanup any active channels for this entity
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    
    if (ecsManager.HasComponent<AudioComponent>(entity)) {
        AudioComponent& audioComp = ecsManager.GetComponent<AudioComponent>(entity);
        audioComp.Stop();  // Ensure proper cleanup
    }
}

void AudioSystem::UpdateSpatialAudio() {
    // Reserved for future optimizations
    // Could batch spatial position updates or use spatial hashing
}
