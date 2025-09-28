#pragma once
#include "Math/Vector3D.hpp"
#include "ECS/Component.hpp"
#include "../Engine.h"

enum class ENGINE_API BodyType {
    Static = 0,
    Kinematic = 1,
    Dynamic = 2
};

struct ENGINE_API RigidbodyComponent {
    BodyType bodyType = BodyType::Dynamic;
    float mass = 1.0f;
    float restitution = 0.5f;  // Bounciness
    float friction = 0.5f;
    bool isGravityEnabled = true;
    bool isTrigger = false;

    Vector3D velocity = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D angularVelocity = Vector3D(0.0f, 0.0f, 0.0f);

    // Internal physics body handle (will be set by PhysicsSystem)
    void* physicsBodyHandle = nullptr;

    // Flag to mark if this component needs physics body creation/update
    bool isDirty = true;
};