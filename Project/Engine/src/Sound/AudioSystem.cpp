#include "pch.h"
#include "Sound/AudioSystem.hpp"
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioManager.hpp"
#include "Transform/TransformComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
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

        // Skip inactive entities (Unity-like behavior)
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                // Stop audio for inactive entities
                auto& audioComp = ecsManager.GetComponent<AudioComponent>(entity);
                if (audioComp.IsPlaying) {
                    audioComp.Stop();
                }
                continue;
            }
        }

        AudioComponent& audioComp = ecsManager.GetComponent<AudioComponent>(entity);

        // Skip disabled components (component-level enable/disable)
        if (!audioComp.enabled) {
            // Stop audio for disabled components
            if (audioComp.IsPlaying) {
                audioComp.Stop();
            }
            continue;
        }

        audioComp.UpdateComponent();

        // Update spatial audio position from Transform if applicable
        if (audioComp.Spatialize && ecsManager.HasComponent<Transform>(entity)) {
            const Transform& transform = ecsManager.GetComponent<Transform>(entity);
            audioComp.OnTransformChanged(transform.localPosition);
        }
    }

    for (const auto& entity : ecsManager.GetActiveEntities()) {
        if (!ecsManager.HasComponent<AudioListenerComponent>(entity)) continue;

        AudioListenerComponent& listenerComp = ecsManager.GetComponent<AudioListenerComponent>(entity);
        if (!listenerComp.enabled) continue;

        // Compute new values from Transform (if available)
        Vector3D newPosition = listenerComp.GetPosition(); // Fallback to internal via getter
        Vector3D newForward = listenerComp.GetForward();
        Vector3D newUp = listenerComp.GetUp();

        if (ecsManager.HasComponent<Transform>(entity)) {
            const Transform& transform = ecsManager.GetComponent<Transform>(entity);
            newPosition = transform.localPosition;
            // Compute forward/up from rotation (assuming Transform has localRotation as glm::quat or similar)
            // Use Vector3D math only; if rotation helpers exist in Math, use them. Otherwise, assume defaults or add simple rotation.
            // For simplicity, if no rotation support, keep defaults. Adjust if Transform provides forward/up directly.
            newForward = Vector3D(0.0f, 0.0f, 1.0f); // Placeholder; replace with actual rotation if available
            newUp = Vector3D(0.0f, 1.0f, 0.0f);
        }

        // Pass to component; it handles change detection and only updates FMOD if changed
        listenerComp.OnTransformChanged(newPosition, newForward, newUp);
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
