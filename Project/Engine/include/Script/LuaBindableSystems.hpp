#pragma once
#include "Math/Vector3D.hpp"

#include "Input/Keys.h"
#include "Input/InputManager.hpp"

// Wrapper functions to convert int to enum for Input functions
namespace InputWrappers {
    inline bool GetKey(int key) {
        return InputManager::GetKey(static_cast<Input::Key>(key));
    }

    inline bool GetKeyDown(int key) {
        return InputManager::GetKeyDown(static_cast<Input::Key>(key));
    }

    inline bool GetMouseButton(int button) {
        return InputManager::GetMouseButton(static_cast<Input::MouseButton>(button));
    }

    inline bool GetMouseButtonDown(int button) {
        return InputManager::GetMouseButtonDown(static_cast<Input::MouseButton>(button));
    }
}


#include "Physics/PhysicsSystem.hpp"
#include "ECS/ECSRegistry.hpp"

namespace PhysicsSystemWrappers {
    inline PhysicsSystem* g_PhysicsSystem = nullptr;

    inline JPH::PhysicsSystem* GetSystem() {
        if (!g_PhysicsSystem) {
            std::cerr << "[ERROR] PhysicsSystem not initialized in wrappers!" << std::endl;
            return nullptr;
        }
        return &g_PhysicsSystem->GetJoltSystem();
    }
}


#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Transform/TransformComponent.hpp"
namespace CharacterControllerWrappers {
    // Constructor wrapper
    inline CharacterController* Create() {
        JPH::PhysicsSystem* physicsSystem = PhysicsSystemWrappers::GetSystem();

        if (!physicsSystem) {
            std::cerr << "[ERROR] Cannot create CharacterController - PhysicsSystem unavailable!" << std::endl;
            return nullptr;
        }
        return new CharacterController(physicsSystem);
    }

    inline void Initialise(CharacterController* controller,
        ColliderComponent* collider,
        Transform* transform) {

        if (controller && collider && transform) {
            controller->Initialise(*collider, *transform);
        }
    }

    // Movement wrappers
    inline void Move(CharacterController* controller, float x, float y, float z) {
        if (controller)
            controller->Move(x, y, z);
    }

    inline void Jump(CharacterController* controller, float height) {
        if (controller)
            controller->Jump(height);
    }

    inline void Update(CharacterController* controller, float deltaTime) {
        if (controller)
            controller->Update(deltaTime);
    }

    inline Vector3D GetPosition(CharacterController* controller) {
        return controller->GetPosition();
    }

    inline void SetVelocity(CharacterController* controller, Vector3D& vel) {
        return controller->SetVelocity(vel);
    }

    inline Vector3D GetVelocity(CharacterController* controller) {
        return controller->GetVelocity();
    }

    inline void Destroy(CharacterController* controller)
    {
        if (controller)
            delete controller;
    }

}