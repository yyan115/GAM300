#pragma once
#include "pch.h"
#include "Physics/Kinematics/CharacterController.hpp"
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include "Physics/CollisionFilters.hpp"
#include "Physics/CollisionLayers.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Math/Vector3D.hpp"



//Separate initialise with ctor -> cannot guaranteed exist before that e.g components like rigidbody + collider
CharacterController::CharacterController(JPH::PhysicsSystem* physicsSystem)
    : mPhysicsSystem(physicsSystem),
      mCharacter(nullptr),
      mVelocity(JPH::Vec3::sZero()),
      mCharacterLayer(Layers::CHARACTER)
{}


CharacterController::~CharacterController()
{
    if (mCharacter)
    {
        delete mCharacter;
        mCharacter = nullptr;
    }
}
bool CharacterController::Initialise(ColliderComponent& collider, Transform& transform)
{
    // Use the collider settings configured by the user in the editor
    collider.layer = Layers::CHARACTER;

    // Create capsule shape using user's configured dimensions
    // Use safe defaults if values seem invalid
    float halfHeight = collider.capsuleHalfHeight;
    float radius = collider.capsuleRadius;

    // Validate and clamp values to reasonable ranges
    if (halfHeight <= 0.0f || halfHeight > 100.0f) halfHeight = 0.5f;
    if (radius <= 0.0f || radius > 100.0f) radius = 0.3f;

    std::cout << "[CharacterController] Creating capsule: halfHeight=" << halfHeight
              << ", radius=" << radius << std::endl;

    JPH::Ref<JPH::Shape> shape = new JPH::CapsuleShape(halfHeight, radius);

    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = shape;
    settings->mMass = 70.0f;
    settings->mMaxStrength = 100.0f;

    // Offset the shape so its bottom is at the character position (feet)
    // Capsule center is at halfHeight + radius above the bottom
    // Validate center offset to avoid garbage values
    float centerX = collider.center.x;
    float centerY = collider.center.y;
    float centerZ = collider.center.z;
    if (std::abs(centerX) > 100.0f) centerX = 0.0f;
    if (std::abs(centerY) > 100.0f) centerY = 0.0f;
    if (std::abs(centerZ) > 100.0f) centerZ = 0.0f;

    float shapeOffsetY = halfHeight + radius + centerY;
    settings->mShapeOffset = JPH::Vec3(centerX, shapeOffsetY, centerZ);

    std::cout << "[CharacterController] Shape offset: (" << centerX << ", "
              << shapeOffsetY << ", " << centerZ << ")" << std::endl;

    //  CREATE VIRTUAL CHARACTER
    mCharacter = new JPH::CharacterVirtual(
        settings,
        JPH::RVec3(transform.localPosition.x, transform.localPosition.y, transform.localPosition.z),
        JPH:: Quat::sIdentity(),
        mPhysicsSystem);

    if (!mCharacter)
    {
        std::cerr << "[CharacterController] Failed to create CharacterVirtual!" << std::endl;
        return false;
    }
    return true;
}

void CharacterController::Update(float deltaTime) {
    if (!mCharacter || !mPhysicsSystem)
        return;

    JPH::Vec3 gravity = mPhysicsSystem->GetGravity();

    // Set velocity BEFORE update 
    mCharacter->SetLinearVelocity(mVelocity);



    // Extended update settings
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0);

    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

    // ExtendedUpdate handles:
    // - Applying gravity
    // - Ground detection
    // - Collision resolution
    // - Velocity updates
    mCharacter->ExtendedUpdate(
        deltaTime,
        gravity,  // Jolt applies this internally
        updateSettings,
        mPhysicsSystem->GetDefaultBroadPhaseLayerFilter(mCharacterLayer),
        mPhysicsSystem->GetDefaultLayerFilter(mCharacterLayer),
        {},
        {},
        temp_allocator
    );
}

void CharacterController::Move(float x, float y, float z)
{
    // Set the desired velocity
    mVelocity = JPH::Vec3(x, y, z);
}

void CharacterController::Jump(float height)
{
        // Calculate jump velocity needed to reach desired height
        // Using: v = sqrt(2 * g * h)
        float gravity = 9.81f;
        float jumpVelocity = JPH::sqrt(2.0f * gravity * height);

        // Set upward velocity
        mVelocity.SetY(jumpVelocity);
}

Vector3D CharacterController::GetPosition() const
{
    if (mCharacter)
    {
        JPH::Vec3 position = mCharacter->GetPosition();
        return FromJoltVec3(position);
    }
    return { 0,0,0 };
}

void CharacterController::SetVelocity(const Vector3D vel)
{
    JPH::Vec3 velocity = ToJoltVec3(vel);
    if (mCharacter)
    {
        mCharacter->SetLinearVelocity(velocity);
    }
}

Vector3D CharacterController::GetVelocity() const
{
    return FromJoltVec3(mVelocity);
}


bool CharacterController::IsGrounded() const
{
    if (mCharacter == nullptr)
    {
        std::cerr << "[DBG] IsGrounded: mCharacter is NULL\n";
        return false;
    }

    JPH::CharacterVirtual::EGroundState state = mCharacter->GetGroundState();
    //try
    //{
    //    state = mCharacter->GetGroundState();
    //}
    //catch (...)
    //{
    //    std::cerr << "[DBG] IsGrounded: Exception calling GetGroundState()\n";
    //    return false;
    //}

    return state == JPH::CharacterVirtual::EGroundState::OnGround;
}


Vector3D CharacterController::GetGravity() const
{
    JPH::Vec3 gravity = mPhysicsSystem->GetGravity();
    return FromJoltVec3(gravity);
}

void CharacterController::SetGravity(Vector3D gravity)
{
    mPhysicsSystem->SetGravity(ToJoltVec3(gravity));
}

