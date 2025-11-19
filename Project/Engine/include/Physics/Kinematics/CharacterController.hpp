#pragma once
#include "pch.h"
#include "Physics/JoltInclude.hpp"
#include "Math/Vector3D.hpp"

// Pure runtime class that handles movement logic
class CharacterController
{
public:
    // Constructor: Needs a PhysicsSystem reference
    CharacterController(JPH::PhysicsSystem* physicsSystem);

    // Destructor
    ~CharacterController();

    // Movement functions
    void Move(float x, float y, float z);   // Set linear velocity
    void Jump(float height);                // Jump

    // Called each frame
    void Update(float deltaTime);

    // Get current position
    JPH::Vec3 GetPosition() const;

    // Set/Get velocity
    void SetVelocity(const JPH::Vec3& velocity);
    JPH::Vec3 GetVelocity() const;

    // Expose raw character for internal use
    const JPH::CharacterVirtual* GetCharacterVirtual() const { return mCharacter; }

private:
    JPH::PhysicsSystem* mPhysicsSystem = nullptr;
    JPH::CharacterVirtual* mCharacter = nullptr;

    // Cache velocity
    JPH::Vec3 mVelocity = JPH::Vec3::sZero();
};
