#pragma once
#include "pch.h"
#include "Physics/Kinematics/CharacterController.hpp"
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
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


//Box = 0,
//Sphere,
//Capsule,
//Cylinder


void CharacterController::Initialise(ColliderComponent& collider, Transform& transform)
{
    //SHAPE TYPE HAS TO BE A CAPSULE..
    collider.shapeType = ColliderShapeType::Capsule;
    collider.layer = Layers::CHARACTER;
    float height = collider.capsuleHalfHeight * 2.0f;
    float radius = collider.capsuleRadius;

    JPH::Ref<JPH::Shape> capsule = new JPH::CapsuleShape(collider.capsuleHalfHeight, collider.capsuleRadius);
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = capsule;
    settings->mMass = 70.0f;    //EXPOSE MASS AS COMPONENT AND INTIIALISE?
    settings->mMaxStrength = 100.0f;

    //  CREATE VIRTUAL CHARACTER    
    mCharacter = new JPH::CharacterVirtual(
        settings,
        JPH::RVec3(transform.localPosition.x, transform.localPosition.y, transform.localPosition.z),
        JPH::Quat::sIdentity(),
        mPhysicsSystem);
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