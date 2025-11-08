#pragma once
#include "Engine.h"
#include "Math/Vector3D.hpp"

struct AudioListenerComponent 
{
    REFL_SERIALIZABLE
    // Properties (exposed for editor)
    bool enabled = true;

    // Internal state (not exposed via reflection)
private:
    Vector3D position = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D forward = Vector3D(0.0f, 0.0f, 1.0f); // Default forward (Z-axis)
    Vector3D up = Vector3D(0.0f, 1.0f, 0.0f);       // Default up (Y-axis)

    // Previous values for optimization (change detection)
    Vector3D previousPosition = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D previousForward = Vector3D(0.0f, 0.0f, 1.0f);
    Vector3D previousUp = Vector3D(0.0f, 1.0f, 0.0f);

public:
    ENGINE_API AudioListenerComponent();
    ENGINE_API ~AudioListenerComponent() = default;

    // Getters for internal state (read-only access for external systems like AudioSystem)
    const Vector3D& GetPosition() const { return position; }
    const Vector3D& GetForward() const { return forward; }
    const Vector3D& GetUp() const { return up; }

    // Component interface
    void UpdateComponent();
    void OnTransformChanged(const Vector3D& newPosition, const Vector3D& newForward, const Vector3D& newUp);

private:
    void UpdateListenerPosition();
};