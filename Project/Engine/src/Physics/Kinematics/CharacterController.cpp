#pragma once
#include "pch.h"
#include "Physics/Kinematics/CharacterController.hpp"
#include <Jolt/Physics/Collision/ObjectLayer.h>




CharacterController::CharacterController(JPH::PhysicsSystem* physicsSystem)
    : mPhysicsSystem(physicsSystem)
{
    // Create character virtual settings
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();

    // Configure character shape (capsule is typical for humanoid characters)
    // Height = 1.8m, Radius = 0.3m (adjust as needed)
    float characterHeight = 1.8f;
    float characterRadius = 0.3f;

    // Create capsule shape and offset it
    JPH::Ref<JPH::Shape> capsule = new JPH::CapsuleShape(0.5f * characterHeight, characterRadius);

    //// Offset the shape so the bottom is at the character's feet
    //JPH::Vec3 offset(0, 0.5f * characterHeight + characterRadius, 0);
    //settings->mShape = new JPH::RotatedTranslatedShape(offset, JPH::Quat::sIdentity(), capsule);

    // Set character properties
    settings->mMass = 70.0f; // 70kg character
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f); // Max walkable slope
    settings->mMaxStrength = 100.0f; // Force to push other bodies
    settings->mCharacterPadding = 0.02f; // Padding around character
    settings->mPenetrationRecoverySpeed = 1.0f;
    settings->mPredictiveContactDistance = 0.1f;

    // Support for stairs/steps
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -characterRadius);

    // Create the character virtual
    mCharacter = new JPH::CharacterVirtual(settings, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), mPhysicsSystem);

    // Enable ground detection
    mCharacter->SetListener(nullptr); // You can add a listener for events if needed
}
CharacterController::~CharacterController()
{
    if (mCharacter)
    {
        delete mCharacter;
        mCharacter = nullptr;
    }
}

void CharacterController::Move(float x, float y, float z)
{
    // Set the desired velocity
    mVelocity = JPH::Vec3(x, y, z);
}

void CharacterController::Jump(float height)
{
    // Only jump if on ground
    if (mCharacter->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
    {
        // Calculate jump velocity needed to reach desired height
        // Using: v = sqrt(2 * g * h)
        float gravity = 9.81f;
        float jumpVelocity = JPH::sqrt(2.0f * gravity * height);

        // Set upward velocity
        mVelocity.SetY(jumpVelocity);
    }
}

void CharacterController::Update(float deltaTime)
{
    if (!mCharacter || !mPhysicsSystem)
        return;

    // Apply gravity
    JPH::Vec3 gravity = mPhysicsSystem->GetGravity();

    // Only apply gravity if not on ground or moving upward
    if (mCharacter->GetGroundState() != JPH::CharacterVirtual::EGroundState::OnGround ||
        mVelocity.GetY() > 0.0f)
    {
        mVelocity += gravity * deltaTime;
    }
    else
    {
        // On ground, cancel out vertical velocity
        mVelocity.SetY(0.0f);
    }

    // Create extended update settings
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0); // Step down distance
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0); // Max step up height

    // Temp allocator for collision checks
    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

    // Update character position
    mCharacter->ExtendedUpdate(
        deltaTime,
        gravity,
        updateSettings,
        //Change "0" later on.. currently placeholder
        mPhysicsSystem->GetDefaultBroadPhaseLayerFilter(0),
        mPhysicsSystem->GetDefaultLayerFilter(0),
        {},
        {},
        temp_allocator
    );
    // Apply velocity
    mCharacter->SetLinearVelocity(mVelocity);
}

JPH::Vec3 CharacterController::GetPosition() const
{
    if (mCharacter)
    {
        return JPH::Vec3(mCharacter->GetPosition());
    }
    return JPH::Vec3::sZero();
}

void CharacterController::SetVelocity(const JPH::Vec3& velocity)
{
    mVelocity = velocity;
    if (mCharacter)
    {
        mCharacter->SetLinearVelocity(velocity);
    }
}

JPH::Vec3 CharacterController::GetVelocity() const
{
    return mVelocity;
}