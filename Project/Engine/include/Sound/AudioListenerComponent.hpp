#pragma once
#include "Engine.h"
#include "Math/Vector3D.hpp"

struct AudioListenerComponent 
{
    REFL_SERIALIZABLE
    // Properties
    bool enabled = true;
    Vector3D position = Vector3D(0.0f, 0.0f, 0.0f);

public:
    ENGINE_API AudioListenerComponent();
    ENGINE_API ~AudioListenerComponent() = default;

    // Component interface
    void UpdateComponent();
    void OnTransformChanged(const Vector3D& newPosition);

private:
    void UpdateListenerPosition();
};