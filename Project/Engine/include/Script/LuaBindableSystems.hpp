#pragma once

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


namespace CharacterControllerWrappers {

    // Constructor wrapper
    inline CharacterController* Create(JPH::PhysicsSystem* physicsSystem) {
        return new CharacterController(physicsSystem);
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

    inline JPH::Vec3 GetPosition(CharacterController* controller) {
        return controller ? controller->GetPosition() : JPH::Vec3::sZero();
    }

    inline void SetVelocity(CharacterController* controller, const JPH::Vec3& vel) {
        if (controller)
            controller->SetVelocity(vel);
    }

    inline JPH::Vec3 GetVelocity(CharacterController* controller) {
        return controller ? controller->GetVelocity() : JPH::Vec3::sZero();
    }

}

#include "Physics/PhysicsSystem.hpp"
#include "ECS/ECSRegistry.hpp"

namespace PhysicsSystemWrappers {
    inline JPH::PhysicsSystem* GetSystem() {
        // Get active ECSManager as reference, then take address of physicsSystem
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.physicsSystem)
            return nullptr;

        return &ecsManager.physicsSystem->GetJoltSystem();
    }
}
