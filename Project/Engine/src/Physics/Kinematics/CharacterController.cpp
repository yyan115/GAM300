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

#include "ECS/System.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"


//Separate initialise with ctor -> cannot guaranteed exist before that e.g components like rigidbody + collider
CharacterController::CharacterController(JPH::PhysicsSystem* physicsSystem)
    : mPhysicsSystem(physicsSystem),
      mCharacter(nullptr),
      mVelocity(JPH::Vec3::sZero()),
      mCharacterLayer(Layers::CHARACTER),
      collider_offsetY(0.0f)
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
    std::cout << "[CharacterController] mPhysicsSystem = " << mPhysicsSystem << std::endl;

    if (!mPhysicsSystem) {
        std::cerr << "[CharacterController] ERROR: PhysicsSystem is NULL!" << std::endl;
        return false;
    }

    // SHAPE TYPE HAS TO BE A CAPSULE
    collider.shapeType = ColliderShapeType::Capsule;
    collider.layer = Layers::CHARACTER;

    // Calculate offset - this represents how much the capsule center is above the feet
    collider_offsetY = collider.center.y * transform.localScale.y;

    std::cout << "[CharacterController] Collider offset Y: " << collider_offsetY << std::endl;
    std::cout << "[CharacterController] Transform position: (" 
              << transform.localPosition.x << ", " 
              << transform.localPosition.y << ", " 
              << transform.localPosition.z << ")" << std::endl;

    std::cout << "Capsule radius=" << collider.capsuleRadius
        << " halfHeight=" << collider.capsuleHalfHeight << std::endl;


    // Create the capsule shape
    JPH::Ref<JPH::Shape> capsule = new JPH::CapsuleShape(
        collider.capsuleHalfHeight * transform.localScale.x,
        collider.capsuleRadius * transform.localScale.y
    );

    // Create character settings
    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = capsule;
    settings->mMass = 70.0f;   
    settings->mMaxStrength = 100.0f;

    // ADD THESE CRITICAL SETTINGS:
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings->mPenetrationRecoverySpeed = 5.0f;  // Try 2.0-5.0 if still phasing
    settings->mPredictiveContactDistance = 0.2f;
    settings->mMaxCollisionIterations = 10;  // More iterations = better collision
    settings->mMaxConstraintIterations = 15;
    settings->mMinTimeRemaining = 1.0e-4f;
    settings->mCollisionTolerance = 1.0e-3f;
    settings->mCharacterPadding = 0.02f;
    settings->mMaxNumHits = 256;
    settings->mHitReductionCosMaxAngle = 0.999f;
    settings->mEnhancedInternalEdgeRemoval = true;      //in-edge smoothing


    // CRITICAL FIX: Add the offset when creating the character
    // Transform position = feet position
    // Physics needs the capsule CENTER position, which is offset upward
    JPH::RVec3 physicsPosition(
        transform.localPosition.x, 
        transform.localPosition.y,
        transform.localPosition.z
    );

    std::cout << "[CharacterController] Creating character at physics position: (" 
              << physicsPosition.GetX() << ", " 
              << physicsPosition.GetY() << ", " 
              << physicsPosition.GetZ() << ")" << std::endl;

    // CREATE VIRTUAL CHARACTER at the correct physics position
    mCharacter = new JPH::CharacterVirtual(
        settings,
        physicsPosition,
        JPH::Quat::sIdentity(),
        mPhysicsSystem
    );

    if (!mCharacter)
    {
        std::cerr << "[CharacterController] Failed to create CharacterVirtual!" << std::endl;
        return false;
    }

    // Initialize the character's collision state
    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
    mCharacter->RefreshContacts(
        mPhysicsSystem->GetDefaultBroadPhaseLayerFilter(mCharacterLayer),
        mPhysicsSystem->GetDefaultLayerFilter(mCharacterLayer),
        {},
        {},
        temp_allocator
    );

    std::cout << "[CharacterController] Initialized with Ground State: "
        << (int)mCharacter->GetGroundState() << std::endl;

    return true;
}

void CharacterController::Update(float deltaTime) {
    if (!mCharacter || !mPhysicsSystem)
        return;

    //std::cout << "=== CharacterController Update ===" << std::endl;

    JPH::Vec3 gravity = mPhysicsSystem->GetGravity();

    // FIRST: Refresh contacts to get accurate ground state
    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
    mCharacter->RefreshContacts(
        mPhysicsSystem->GetDefaultBroadPhaseLayerFilter(mCharacterLayer),
        mPhysicsSystem->GetDefaultLayerFilter(mCharacterLayer),
        {},
        {},
        temp_allocator
    );

    // Get current state AFTER refreshing contacts
    JPH::Vec3 currentVelocity = mCharacter->GetLinearVelocity();
    JPH::CharacterVirtual::EGroundState groundState = mCharacter->GetGroundState();

    //std::cout << "[C++] Current Ground State: " << (int)groundState << std::endl;
    //std::cout << "[C++] Current Position Y: " << mCharacter->GetPosition().GetY() << std::endl;
    //std::cout << "[C++] Velocity BEFORE calculation: y=" << currentVelocity.GetY() << std::endl;

    // Calculate new velocity based on ground state
    JPH::Vec3 newVelocity;

    if (groundState == JPH::CharacterVirtual::EGroundState::OnGround) {

        if (jump_Requested)
        {
            newVelocity = currentVelocity;
            newVelocity.SetY(mVelocity.GetY());

            groundState = JPH::CharacterVirtual::EGroundState::InAir;

            jump_Requested = false;
        }
        else
        {

            // On ground: Use ground velocity (platform movement) + horizontal input
            // DON'T add gravity - we're supported by the ground!
            newVelocity = mCharacter->GetGroundVelocity();

            // Add any player horizontal movement to this
            newVelocity += JPH::Vec3(mVelocity.GetX(), 0, mVelocity.GetZ());

            // Reset vertical component when on ground
            newVelocity.SetY(0);

            //std::cout << "[C++] On ground - using ground velocity, no gravity" << std::endl;
        }
    }
    else {
        // In air: Apply gravity to current velocity
        newVelocity = currentVelocity + gravity * deltaTime;
        
        // Add player input (allows air control)
        newVelocity += JPH::Vec3(mVelocity.GetX(), mVelocity.GetY(), mVelocity.GetZ()) * deltaTime;
        
        //std::cout << "[C++] In air - applying gravity" << std::endl;
    }

    //std::cout << "[C++] New velocity calculated: y=" << newVelocity.GetY() << std::endl;

    // Set velocity BEFORE ExtendedUpdate
    mCharacter->SetLinearVelocity(newVelocity);

    //std::cout << "[C++] Position BEFORE ExtendedUpdate: y=" << mCharacter->GetPosition().GetY() << std::endl;

    // Configure update settings
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0);

    // Perform the character update
    mCharacter->ExtendedUpdate(
        deltaTime,
        gravity,
        updateSettings,
        mPhysicsSystem->GetDefaultBroadPhaseLayerFilter(mCharacterLayer),
        mPhysicsSystem->GetDefaultLayerFilter(mCharacterLayer),
        {},
        {},
        temp_allocator
    );

    //std::cout << "[C++] Position AFTER ExtendedUpdate: y=" << mCharacter->GetPosition().GetY() << std::endl;
    //std::cout << "[C++] Velocity AFTER: y=" << mCharacter->GetLinearVelocity().GetY() << std::endl;
    //std::cout << "[C++] Ground State AFTER: " << (int)mCharacter->GetGroundState() << std::endl;

    // Debug ground contact info
    if (mCharacter->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
        JPH::Vec3 groundNormal = mCharacter->GetGroundNormal();
        JPH::Vec3 groundVelocity = mCharacter->GetGroundVelocity();
        JPH::BodyID groundBodyID = mCharacter->GetGroundBodyID();
        
        //std::cout << "[C++] ON GROUND! Normal: (" << groundNormal.GetX() << ", " 
        //          << groundNormal.GetY() << ", " << groundNormal.GetZ() << ")" << std::endl;
        //std::cout << "[C++] Ground Body ID: " << groundBodyID.GetIndex() << std::endl;
        //std::cout << "[C++] Ground Velocity: " << groundVelocity.GetY() << std::endl;
    }
    
    //std::cout << "===================================" << std::endl;

    // Clear player input velocity after applying it
    mVelocity = JPH::Vec3::sZero();
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
    jump_Requested = true;
}

Vector3D CharacterController::GetPosition() const
{
    if (mCharacter)
    {
        // Physics returns the capsule CENTER position
        JPH::Vec3 physicsPosition = mCharacter->GetPosition();
        
        // Subtract offset to get the feet position (matches Transform convention)
        Vector3D feetPosition(
            physicsPosition.GetX(), 
            physicsPosition.GetY() - collider_offsetY,
            physicsPosition.GetZ()
        );        
        return feetPosition;
    }
    return Vector3D(0, 0, 0);
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
    if (mCharacter)
    {
        return FromJoltVec3(mCharacter->GetLinearVelocity());
    }
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