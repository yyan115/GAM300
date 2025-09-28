#pragma once
#include "Math/Vector3D.hpp"
#include "ECS/Component.hpp"
#include "../Engine.h"

enum class ENGINE_API ColliderType {
    Box = 0,
    Sphere = 1,
    Capsule = 2,
    Mesh = 3
};

struct ENGINE_API ColliderComponent {
    ColliderType type = ColliderType::Box;
    Vector3D size = Vector3D(1.0f, 1.0f, 1.0f);  // For Box: width, height, depth; For Sphere: radius in x; For Capsule: radius in x, height in y
    Vector3D center = Vector3D(0.0f, 0.0f, 0.0f);  // Local offset from transform

    // For mesh colliders
    std::string meshPath = "";

    // Internal physics shape handle (will be set by PhysicsSystem)
    void* physicsShapeHandle = nullptr;

    // Flag to mark if this component needs physics shape creation/update
    bool isDirty = true;
};