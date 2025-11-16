#include "pch.h"
#include "Sound/AudioSystem.hpp"
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "Sound/AudioManager.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void AudioSystem::Update(float deltaTime) {
	PROFILE_FUNCTION();
	(void)deltaTime; // Unused for now
    // First, update the AudioManager's internal FMOD system
    AudioManager::GetInstance().Update();
    
    // Then update all AudioComponents
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    
    // Update AudioListener components first
    for (const auto& entity : ecsManager.GetActiveEntities()) {
        if (!ecsManager.HasComponent<AudioListenerComponent>(entity)) continue;

        AudioListenerComponent& listenerComp = ecsManager.GetComponent<AudioListenerComponent>(entity);
        if (!listenerComp.enabled) continue;

        // Compute new values from Transform and Camera
        Vector3D newPosition = listenerComp.GetPosition();
        Vector3D newForward = Vector3D(0.0f, 0.0f, 1.0f); // Default forward
        Vector3D newUp = Vector3D(0.0f, 1.0f, 0.0f); // Default up

        if (ecsManager.HasComponent<Transform>(entity)) {
            const Transform& transform = ecsManager.GetComponent<Transform>(entity);
            newPosition = transform.localPosition;
        }

        if (ecsManager.HasComponent<CameraComponent>(entity)) {
            const CameraComponent& camera = ecsManager.GetComponent<CameraComponent>(entity);
            float yaw_rad = camera.yaw * (M_PI / 180.0f);
            float pitch_rad = camera.pitch * (M_PI / 180.0f);
            newForward.x = cos(yaw_rad) * cos(pitch_rad);
            newForward.y = sin(pitch_rad);
            newForward.z = sin(yaw_rad) * cos(pitch_rad);
            newForward.Normalize();
            Vector3D world_up(0.0f, 1.0f, 0.0f);
            Vector3D right = newForward.Cross(world_up);
            right.Normalize();
            right = -right;  // Negate right to fix left/right inversion
            newUp = right.Cross(newForward);
            newUp.Normalize();
        }

        listenerComp.OnTransformChanged(newPosition, newForward, newUp);
    }
    
    // Update AudioReverbZone components
    for (const auto& entity : ecsManager.GetActiveEntities()) {
        if (!ecsManager.HasComponent<AudioReverbZoneComponent>(entity)) continue;

        // Skip inactive entities
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) continue;
        }

        AudioReverbZoneComponent& reverbZoneComp = ecsManager.GetComponent<AudioReverbZoneComponent>(entity);
        
        // Update position from Transform
        if (ecsManager.HasComponent<Transform>(entity)) {
            const Transform& transform = ecsManager.GetComponent<Transform>(entity);
            reverbZoneComp.OnTransformChanged(transform.localPosition);
        }
        
        // Update the reverb zone
        reverbZoneComp.UpdateComponent();
    }
    
    // Update AudioComponent entities
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
