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
    if (!mPhysicsSystem) {
        std::cerr << "[CharacterController] ERROR: PhysicsSystem is NULL!" << std::endl;
        return false;
    }

    // SHAPE TYPE HAS TO BE A CAPSULE
    collider.shapeType = ColliderShapeType::Capsule;
    collider.layer = Layers::CHARACTER;

    collider.capsuleRadius = 0.25f;
    collider.capsuleHalfHeight = 0.4f;
    collider.center.y = collider.capsuleHalfHeight + collider.capsuleRadius;
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


    // Position the CharacterVirtual so the capsule center is above the feet.
    // GetPosition() subtracts collider_offsetY, so this ensures it returns the scene position.
    JPH::RVec3 physicsPosition(
        transform.localPosition.x,
        transform.localPosition.y + collider_offsetY,
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


CharacterController* CharacterController::CreateController(ColliderComponent& collider, Transform& transform)
{
    if (!mPhysicsSystem) {
        std::cerr << "[ERROR] Cannot create CharacterController - PhysicsSystem unavailable!" << std::endl;
        return nullptr;
    }

    CharacterController* controller = new CharacterController(mPhysicsSystem);

    if (!controller->Initialise(collider, transform)) {
        std::cerr << "[ERROR] CharacterController initialization failed!" << std::endl;
        delete controller;
        return nullptr;
    }
    return controller;
}


void CharacterController::Update(float deltaTime)
{
    if (!mCharacter || !mPhysicsSystem)
        return;

    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

    const JPH::Vec3 gravity = mPhysicsSystem->GetGravity();
    const JPH::Vec3 currentVel = mCharacter->GetLinearVelocity();

    JPH::CharacterVirtual::EGroundState groundState = mCharacter->GetGroundState();
    const bool isOnGround = (groundState == JPH::CharacterVirtual::EGroundState::OnGround);

    JPH::Vec3 newVelocity;

    if (isOnGround)
    {
        if (jump_Requested)
        {
            // Jump: preserve current XZ momentum, inject upward impulse only.
            // Lua's XZ is ignored this frame — the running momentum in currentVel
            // carries forward naturally, so the jump doesn't kill your run speed.
            newVelocity = JPH::Vec3(currentVel.GetX(), mVelocity.GetY(), currentVel.GetZ());
            jump_Requested = false;
        }
        else
        {
            // Grounded: start from platform velocity so moving platforms work,
            // then replace XZ entirely with what Lua says this frame.
            // Never add — Lua owns horizontal completely.
            JPH::Vec3 groundVel = mCharacter->GetGroundVelocity();
            newVelocity = JPH::Vec3(
                groundVel.GetX() + mVelocity.GetX(),
                0.0f,
                groundVel.GetZ() + mVelocity.GetZ()
            );
        }
    }
    else
    {
        // In air: Lua owns XZ exactly, C++ only accumulates vertical gravity.
        // Never add Lua XZ on top of currentVel — that compounds speed every frame.
        float verticalVel = currentVel.GetY() + gravity.GetY() * deltaTime;
        newVelocity = JPH::Vec3(mVelocity.GetX(), verticalVel, mVelocity.GetZ());
    }

    mCharacter->SetLinearVelocity(newVelocity);

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.2f, 0);  // was -0.5, too aggressive
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0);

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

    // Clear input — Lua sets this every frame it wants movement.
    // If Lua sends nothing next frame, character stops horizontally.
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
void CharacterController::SetPosition(Transform transform)
{
    JPH::RVec3 newPosition(transform.localPosition.x, transform.localPosition.y, transform.localPosition.z);

    mCharacter->SetPosition(newPosition);
    mCharacter->SetLinearVelocity(JPH::Vec3::sZero());
    mVelocity = JPH::Vec3::sZero();
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