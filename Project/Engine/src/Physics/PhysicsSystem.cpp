/*********************************************************************************
* @File			PhysicsSystem.cpp
* @Author		Ang Jia Jun Austin, a,jiajunaustin@digipen.edu
* @Co-Author	-
* @Date			23/10/2025
* @Brief		Physics simulation system using Jolt Physics Engine. Manages
*				physics initialization, simulation updates, collision detection,
*				and synchronization between physics state and ECS components.
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/


#pragma once
#include "pch.h"
#include "ECS/System.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/LayerManager.hpp"
//#include "Physics/JoltInclude.hpp"

#include "Physics/PhysicsSystem.hpp"
#include "Physics/CollisionFilters.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "Transform/TransformComponent.hpp"
#include <cstdarg>

#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CollideShape.h>   // <-- gives CollideShapeSettings
#include <Jolt/Physics/Body/BodyFilter.h>          // BodyFilter
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>    // ShapeFilter
#include "Game AI/NavSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "Dialogue/DialogueManager.hpp"


#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <cmath>
#include <Hierarchy/EntityGUIDRegistry.hpp>
#include <Hierarchy/ParentComponent.hpp>


#define STR2(x) #x
#define STR(x) STR2(x)
#pragma message("JPH_OBJECT_STREAM=" STR(JPH_OBJECT_STREAM))
#pragma message("JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=" STR(JPH_FLOATING_POINT_EXCEPTIONS_ENABLED))
#pragma message("JPH_PROFILE_ENABLED=" STR(JPH_PROFILE_ENABLED))

const uint32_t MAX_BODIES = 65536;
const uint32_t NUM_BODY_MUTEXES = 0; // 0 = default
const uint32_t MAX_BODY_PAIRS = 65536;
const uint32_t MAX_CONTACT_CONSTRAINTS = 10240;


static void JoltTrace(const char* fmt, ...)
{
    va_list a; va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    fputc('\n', stderr);
    va_end(a);
}

inline bool JoltAssertFailed(const char* expr, const char* msg, const char* file, JPH::uint line)
{
    fprintf(stderr, "[Jolt Assert] %s : %s (%s:%u)\n", expr, msg ? msg : "", file, (unsigned)line);
    return false;
}

namespace {
constexpr float kMinSafePhysicsDt = 1.0e-6f;
const std::string kOnTriggerEnter = "OnTriggerEnter";
const std::string kOnCollisionEnter = "OnCollisionEnter";
const std::string kOnTriggerExit = "OnTriggerExit";
const std::string kOnCollisionExit = "OnCollisionExit";
const std::string kOnTriggerStay = "OnTriggerStay";
const std::string kOnCollisionStay = "OnCollisionStay";

bool IsFiniteVector3D(const Vector3D& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFiniteQuaternion(const Quaternion& value)
{
    return std::isfinite(value.w) && std::isfinite(value.x) &&
        std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFiniteJoltVec3(const JPH::Vec3& value)
{
    return std::isfinite(value.GetX()) && std::isfinite(value.GetY()) && std::isfinite(value.GetZ());
}

bool IsFiniteJoltRVec3(const JPH::RVec3& value)
{
    return std::isfinite(value.GetX()) && std::isfinite(value.GetY()) && std::isfinite(value.GetZ());
}

bool IsFiniteJoltQuat(const JPH::Quat& value)
{
    return std::isfinite(value.GetW()) && std::isfinite(value.GetX()) &&
        std::isfinite(value.GetY()) && std::isfinite(value.GetZ());
}

float GetSafePhysicsDt(float fixedDt)
{
    if (std::isfinite(fixedDt) && fixedDt > kMinSafePhysicsDt) {
        return fixedDt;
    }
    return 1.0f / 60.0f;
}

std::pair<Entity, Entity> MakeInteractionKey(Entity a, Entity b)
{
    return a < b ? std::pair{a, b} : std::pair{b, a};
}

void InsertInteraction(
    std::vector<std::pair<Entity, Entity>>& interactions,
    Entity a,
    Entity b)
{
    const auto key = MakeInteractionKey(a, b);
    const auto position =
        std::lower_bound(interactions.begin(), interactions.end(), key);
    if (position == interactions.end() || *position != key) {
        interactions.insert(position, key);
    }
}

void EraseInteraction(
    std::vector<std::pair<Entity, Entity>>& interactions,
    Entity a,
    Entity b)
{
    const auto key = MakeInteractionKey(a, b);
    const auto position =
        std::lower_bound(interactions.begin(), interactions.end(), key);
    if (position != interactions.end() && *position == key) {
        interactions.erase(position);
    }
}

JPH::Quat NormalizeOrFallback(const JPH::Quat& value, const JPH::Quat& fallback)
{
    JPH::Quat safeFallback = JPH::Quat::sIdentity();
    if (IsFiniteJoltQuat(fallback)) {
        const float fallbackLenSq = fallback.GetW() * fallback.GetW() + fallback.GetX() * fallback.GetX() +
            fallback.GetY() * fallback.GetY() + fallback.GetZ() * fallback.GetZ();
        if (fallbackLenSq > kMinSafePhysicsDt) {
            if (std::abs(fallbackLenSq - 1.0f) <= 1.0e-6f) {
                safeFallback = fallback;
            }
            else {
                const JPH::Quat normalizedFallback = fallback.Normalized();
                if (IsFiniteJoltQuat(normalizedFallback)) {
                    safeFallback = normalizedFallback;
                }
            }
        }
    }
    if (!IsFiniteJoltQuat(value)) {
        return safeFallback;
    }

    const float lenSq = value.GetW() * value.GetW() + value.GetX() * value.GetX() +
        value.GetY() * value.GetY() + value.GetZ() * value.GetZ();
    if (!(lenSq > kMinSafePhysicsDt)) {
        return safeFallback;
    }

    // TransformSystem already keeps world rotations normalized. This is the
    // hot kinematic/hurtbox path, so only pay for Jolt normalization when the
    // value has actually drifted.
    if (std::abs(lenSq - 1.0f) <= 1.0e-6f) {
        return value;
    }

    const JPH::Quat normalized = value.Normalized();
    return IsFiniteJoltQuat(normalized) ? normalized : safeFallback;
}

JPH::Quat MakeSafeJoltQuat(const Quaternion& value, const JPH::Quat& fallback)
{
    if (!IsFiniteQuaternion(value)) {
        return NormalizeOrFallback(fallback, JPH::Quat::sIdentity());
    }
    return NormalizeOrFallback(JPH::Quat(value.x, value.y, value.z, value.w), fallback);
}
}



bool PhysicsSystem::InitialiseJolt() {
    // Jolt one-time bootstrap (types/allocator) - shared across all instances
    static bool joltTypesInitialized = false;

    if (!joltTypesInitialized) {
#ifdef __ANDROID__
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Starting Jolt initialization...");
#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Registering default allocator...");
        JPH::RegisterDefaultAllocator();
#endif
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Setting trace and assert handlers...");
#else
        JPH::RegisterDefaultAllocator();
#endif
        JPH::Trace = JoltTrace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed; )

#ifdef __ANDROID__
            //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Creating Factory instance...");
#endif
        JPH::Factory::sInstance = new JPH::Factory();

#ifdef __ANDROID__
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Registering Jolt types...");
#ifdef JPH_PROFILE_ENABLED
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_PROFILE_ENABLED=%d", 1);
#else
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_PROFILE_ENABLED=%d", 0);
#endif
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_OBJECT_STREAM=%d", JPH_OBJECT_STREAM);
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=%d", JPH_FLOATING_POINT_EXCEPTIONS_ENABLED);
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_DISABLE_CUSTOM_ALLOCATOR=%d",
#ifdef JPH_DISABLE_CUSTOM_ALLOCATOR
        //    1
#else
        //    0
#endif
        //);
        (void)0;



#ifdef JPH_OBJECT_LAYER_BITS
        //JPH_OBJECT_LAYER_BITS
#else
        //16  // default
#endif
        //);
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_ENABLE_ASSERTS=%d",
#ifdef JPH_ENABLE_ASSERTS
            //1
#else
            //0
#endif
            //);
#endif
            JPH::RegisterTypes();

#ifdef __ANDROID__
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Jolt initialization complete!");
#endif
        joltTypesInitialized = true;
    }

    // Only initialize THIS instance's physics system once (calling physics.Init() again would wipe all bodies!)
    if (!m_joltInitialized) {
        //std::cout << "[Physics] InitialiseJolt: Creating physics world for this instance..." << std::endl;

        if (!temp) temp = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
        unsigned int physicsWorkerCount =
            std::max(1u, std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() - 1 : 1u);
#ifdef __ANDROID__
        // Animation already runs beside physics. Leave cores for engine work
        // instead of nesting a near-hardware-sized Jolt pool inside that job.
        const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
        physicsWorkerCount = std::max(1u, std::min(3u, hardwareThreads > 3u ? hardwareThreads - 3u : 1u));
#endif
        if (!jobs) jobs = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
            physicsWorkerCount);

        physics.Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
            broadphase, objVsBP, objPair);
        physics.SetGravity(JPH::Vec3(0, -9.81f, 0));  // gravity set

        // Configure physics settings for better collision resolution
        JPH::PhysicsSettings settings;
        settings.mNumVelocitySteps = 6;       // 6 is sufficient for action game (default 10 is for heavy sims)
        settings.mNumPositionSteps = 2;       // Increase from default (2) to 3-4
        settings.mBaumgarte = 0.2f;           // Penetration correction factor
        settings.mSpeculativeContactDistance = 0.02f;  // Predict contacts earlier
        settings.mPenetrationSlop = 0.02f;    // Allow small penetrations
        settings.mLinearCastThreshold = 0.75f; // CCD threshold
        physics.SetPhysicsSettings(settings);

//#ifdef __ANDROID__
//        JPH::Vec3 gravity = physics.GetGravity();
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Gravity set to: (%f, %f, %f)",
//            gravity.GetX(), gravity.GetY(), gravity.GetZ());
//#endif

        // Create contact listener with access to the same map
        contactListener = std::make_unique<MyContactListener>(bodyToEntityMap);
        physics.SetContactListener(contactListener.get());

        m_joltInitialized = true;
    } else {
        //std::cout << "[Physics] InitialiseJolt: This instance already initialized, skipping Init()" << std::endl;
    }

    return true;
}


void PhysicsSystem::Initialise(ECSManager& ecsManager) {
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] physicsAuthoring called, entities=%zu", entities.size());
//#endif

    if (ecsManager.characterControllerSystem)
        ecsManager.characterControllerSystem->Shutdown();

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // Remove previously created bodies
    for (auto& [entity, bodyId] : entityBodyMap) {
        if (!bodyId.IsInvalid()) {
            if (bi.IsAdded(bodyId)) {
                bi.RemoveBody(bodyId);
            }
            bi.DestroyBody(bodyId);
        }
    }
    entityBodyMap.clear();
    bodyToEntityMap.clear();
    m_activeInteractions.clear();
    if (m_activeInteractions.capacity() < 256) {
        m_activeInteractions.reserve(256);
    }

    // =========================================================
    // FLUSH STALE PHYSICS EVENTS
    // =========================================================
    // Removing bodies above (and during the previous session's Shutdown) 
    // causes Jolt to queue a backlog of "OnContactRemoved" events. 
    // We must drain and discard them into the void so they don't instantly 
    // cancel out interactions for newly recycled Entity IDs in the new session!
    if (contactListener) {
        auto resolveRootEntity = [&ecsManager](int rawEntity) -> int {
            Entity current = static_cast<Entity>(rawEntity);
            auto& guidRegistry = EntityGUIDRegistry::GetInstance();

            while (ecsManager.HasComponent<ParentComponent>(current)) {
                auto& parentComp = ecsManager.GetComponent<ParentComponent>(current);
                Entity parentEntity = guidRegistry.GetEntityByGUID(parentComp.parent);
                if (parentEntity == static_cast<Entity>(-1) || parentEntity == UINT32_MAX) {
                    break;
                }
                current = parentEntity;
            }

            return static_cast<int>(current);
        };
        contactListener->SetRootResolver(resolveRootEntity);

        m_enterEventsScratch.clear();
        m_exitEventsScratch.clear();
        contactListener->DrainEvents(m_enterEventsScratch, m_exitEventsScratch);

        //  Wipe the activeCollisions memory bank!
        // This prevents recycled Entity IDs from being ignored by the 
        // `if (activeCollisions.insert(key).second)` check in OnContactAdded.
        contactListener->ClearCollisions();
    }

    // Create bodies for all existing entities
    for (auto& e : entities) {
        CreatePhysicsBody(e, ecsManager);
    }
}

void PhysicsSystem::PostInitialize(ECSManager& ecsManager) {
    NavSystem::Get().Build(*this, ecsManager);
}

//KINEMATIC: NOT AFFECTED BY GRAVITY, FORCES, IMPULSES, OTHER BODIES MOVING IT.
//MOVE MANUALLY VIA POS, ROTATION E.T.C
//DYNAMIC: USE PHYSICS SIMULATION. IF ANY CHANGES TO BE MADE, ADJUST VIA FORCES, NOT POS
void PhysicsSystem::Update(float fixedDt, ECSManager& ecsManager) {
    PROFILE_FUNCTION();
    const float safeFixedDt = GetSafePhysicsDt(fixedDt);
//#ifdef __ANDROID__
//    static int updateCount = 0;
//    if (updateCount++ % 60 == 0) {
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Update called, fixedDt=%f, entities=%zu", fixedDt, entities.size());
//    }
//#endif

    // Most authored bodies are static. Build compact moving-body lists while
    // activation state is already hot instead of rescanning every body for
    // each kinematic and dynamic phase. Clear them before any early return so
    // PhysicsSyncBack never consumes a previous frame's entity list.
    m_kinematicEntitiesScratch.clear();
    m_dynamicEntitiesScratch.clear();
    m_dynamicBodyIdsScratch.clear();

    if (entities.empty()) return;

    JPH::BodyInterface& bi = physics.GetBodyInterface();
    JPH::BodyInterface& biNoLock = physics.GetBodyInterfaceNoLock();
    const JPH::BodyLockInterface& bodyLockInterface = physics.GetBodyLockInterface();

    // =========================================================================================
    // 1. MANAGE BODY ACTIVATION STATE (Add/Remove from World)
    //    This ensures that if an entity is disabled in the hierarchy OR the collider is disabled,
    //    it is removed from the physics simulation entirely.
    // =========================================================================================
    for (auto& e : entities) {
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        const bool shouldBeActive = col.enabled && rb.enabled &&
            ecsManager.IsEntityActiveInHierarchy(e);

        // Disabled/inactive authored bodies can be created lazily when enabled.
        // This also prevents CharacterController-owned entities (whose regular
        // rigid body is deliberately removed and disabled) from being recreated.
        if (rb.id.IsInvalid()) {
            if (!shouldBeActive) continue;
            CreatePhysicsBody(e, ecsManager);
        }

        const JPH::BodyID bodyId = rb.id;
        if (bodyId.IsInvalid()) continue;

        if (shouldBeActive && !rb.physicsWorldAdded) {
            // Re-add to the world (Wake it up)
            bi.AddBody(bodyId, JPH::EActivation::Activate);
            rb.physicsWorldAdded = true;
        }
        else if (!shouldBeActive && rb.physicsWorldAdded) {
            // Remove from the world (Stops all collisions and processing)
            bi.RemoveBody(bodyId);
            rb.physicsWorldAdded = false;
        }

        if (!rb.physicsWorldAdded) continue;

        const bool gravityChanged =
            rb.gravityFactor != rb.appliedGravityFactor;
        if (gravityChanged) {
            bi.SetGravityFactor(bodyId, rb.gravityFactor);
            rb.appliedGravityFactor = rb.gravityFactor;
        }

        const bool sensorChanged = rb.isTrigger != rb.appliedIsTrigger;
        if (sensorChanged) {
            bi.SetIsSensor(bodyId, rb.isTrigger);
            rb.appliedIsTrigger = rb.isTrigger;
        }

        if (rb.motion == Motion::Kinematic) {
            m_kinematicEntitiesScratch.push_back(e);
        }
        else if (rb.motion == Motion::Dynamic) {
            m_dynamicEntitiesScratch.push_back(e);
            m_dynamicBodyIdsScratch.push_back(bodyId);
        }
    }

    // =========================================================================================
    // 2. UPDATE KINEMATIC BODIES BEFORE PHYSICS STEP
    // =========================================================================================
    for (Entity e : m_kinematicEntitiesScratch) {
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        const JPH::BodyID bodyId = rb.id;

        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);

            // FIX: Use World Scale
            Vector3D scaledOffset = {
                col.center.x * tr.worldScale.x,
                col.center.y * tr.worldScale.y,
                col.center.z * tr.worldScale.z
            };
            if (!IsFiniteVector3D(tr.worldScale) || !IsFiniteVector3D(scaledOffset) || !IsFiniteVector3D(tr.worldPosition)) {
                continue;
            }

            // TransformSystem normalizes world rotations before physics runs.
            // Avoid a body read/lock solely to obtain an invalid-data fallback.
            JPH::Quat targetRot = MakeSafeJoltQuat(
                tr.worldRotation, JPH::Quat::sIdentity());

            // Rotate offset to world space
            JPH::Vec3 offsetInWorld = targetRot * JPH::Vec3(scaledOffset.x, scaledOffset.y, scaledOffset.z);
            if (!IsFiniteJoltVec3(offsetInWorld)) {
                continue;
            }

            // FIX: Use World Position as base
            JPH::RVec3 basePos(tr.worldPosition.x, tr.worldPosition.y, tr.worldPosition.z);
            if (!IsFiniteJoltRVec3(basePos)) {
                continue;
            }
            JPH::RVec3 targetPos = basePos + offsetInWorld;
            if (!IsFiniteJoltRVec3(targetPos)) {
                continue;
            }

            // [FIX START] Handle Triggers differently from Physical Objects
            if (rb.isTrigger) {
                // For Triggers/Sensors: Force teleport.
                // This ensures overlaps are detected even if the body is stationary.
                // 'MoveKinematic' relies on velocity simulation which can be culled when V=0.
                // Keep one write lock across both BodyInterface operations. The
                // no-lock interface performs the same broadphase notification
                // and activation while the body is protected by this scope.
                JPH::BodyLockWrite lock(bodyLockInterface, bodyId);
                if (lock.Succeeded()) {
                    biNoLock.SetPositionAndRotation(
                        bodyId, targetPos, targetRot, JPH::EActivation::Activate);

                    // Clear velocities so it doesn't have "momentum" if it turns dynamic later.
                    // The microscopic velocity keeps stationary sensors awake for overlap checks.
                    biNoLock.SetLinearAndAngularVelocity(
                        bodyId,
                        JPH::Vec3(0.0f, -0.001f, 0.0f),
                        JPH::Vec3::sZero());
                }
            }
            else {
                // For Solid Objects (Moving Platforms): Use MoveKinematic.
                // This enables friction/pushing of characters standing on them.
                // MoveKinematic computes both velocities from the body's current
                // pose under one write lock and activates it when movement is
                // non-zero. Doing that manually first duplicated the same math
                // and took four additional body locks per collider.
                bi.MoveKinematic(bodyId, targetPos, targetRot, safeFixedDt);
            }

            //bi.SetLinearVelocity(bodyId, linearVel);
            //bi.SetAngularVelocity(bodyId, angularVel);
            //bi.MoveKinematic(bodyId, targetPos, targetRot, fixedDt);

			rb.transform_dirty = false;
    }

    // =========================================================================================
    // 3. SYNC ECS -> JOLT (for dynamic bodies)
    // =========================================================================================
    for (Entity e : m_dynamicEntitiesScratch) {
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        const JPH::BodyID bodyId = rb.id;

        if (rb.isTeleporting)
        {
            auto& tr = ecsManager.GetComponent<Transform>(e);
            // 1. Read the Transform you set in Lua
            JPH::Vec3 newPos;
            const Quaternion* sourceRotation = nullptr;

            if (tr.isDirty) {
                // Lua just set this, so World values are STALE. Use LOCAL values.
                // (Assuming feather is a root object with no parent)
                if (!IsFiniteVector3D(tr.localPosition)) {
                    continue;
                }
                newPos = JPH::Vec3(tr.localPosition.x, tr.localPosition.y, tr.localPosition.z);
                sourceRotation = &tr.localRotation;
            }
            else {
                // Safe to use World values
                if (!IsFiniteVector3D(tr.worldPosition)) {
                    continue;
                }
                newPos = JPH::Vec3(tr.worldPosition.x, tr.worldPosition.y, tr.worldPosition.z);
                sourceRotation = &tr.worldRotation;
            }

            JPH::BodyLockWrite lock(bodyLockInterface, bodyId);
            const JPH::Quat fallbackRotation = lock.Succeeded()
                ? lock.GetBody().GetRotation()
                : JPH::Quat::sIdentity();
            const JPH::Quat newRot = MakeSafeJoltQuat(*sourceRotation, fallbackRotation);
            if (!IsFiniteJoltVec3(newPos) || !IsFiniteJoltQuat(newRot)) {
                continue;
            }

            if (lock.Succeeded()) {
                // Force the body pose and clear both velocities under one lock.
                biNoLock.SetPositionAndRotation(
                    bodyId, newPos, newRot, JPH::EActivation::Activate);
                biNoLock.SetLinearAndAngularVelocity(
                    bodyId, JPH::Vec3::sZero(), JPH::Vec3::sZero());
            }

            // 3. Reset the flag
            rb.isTeleporting = false;
        }

        const bool hasLinearVelocity =
            rb.linearVel.x != 0.0f || rb.linearVel.y != 0.0f ||
            rb.linearVel.z != 0.0f;
        const bool hasAngularVelocity =
            rb.angularVel.x != 0.0f || rb.angularVel.y != 0.0f ||
            rb.angularVel.z != 0.0f;
        if (hasLinearVelocity && hasAngularVelocity) {
            bi.SetLinearAndAngularVelocity(
                bodyId,
                ToJoltVec3(rb.linearVel),
                ToJoltVec3(rb.angularVel));
        }
        else if (hasLinearVelocity) {
            bi.SetLinearVelocity(bodyId, ToJoltVec3(rb.linearVel));
        }
        else if (hasAngularVelocity) {
            bi.SetAngularVelocity(bodyId, ToJoltVec3(rb.angularVel));
        }
        if (hasLinearVelocity) {
            rb.linearVel = Vector3D(0, 0, 0);
        }
        if (hasAngularVelocity) {
            rb.angularVel = Vector3D(0, 0, 0);
        }

        const bool hasForce =
            rb.forceApplied.x != 0.0f || rb.forceApplied.y != 0.0f ||
            rb.forceApplied.z != 0.0f;
        const bool hasTorque =
            rb.torqueApplied.x != 0.0f || rb.torqueApplied.y != 0.0f ||
            rb.torqueApplied.z != 0.0f;
        if (hasForce && hasTorque) {
            bi.AddForceAndTorque(
                bodyId,
                ToJoltVec3(rb.forceApplied),
                ToJoltVec3(rb.torqueApplied));
        }
        else if (hasForce) {
            bi.AddForce(bodyId, ToJoltVec3(rb.forceApplied));
        }
        else if (hasTorque) {
            bi.AddTorque(bodyId, ToJoltVec3(rb.torqueApplied));
        }
        if (hasForce) {
            rb.forceApplied = Vector3D(0.0f, 0.0f, 0.0f);
        }
        if (hasTorque) {
            rb.torqueApplied = Vector3D(0.0f, 0.0f, 0.0f);
        }
        if (rb.impulseApplied.x != 0.0f || rb.impulseApplied.y != 0.0f || rb.impulseApplied.z != 0.0f) {
            bi.AddImpulse(bodyId, ToJoltVec3(rb.impulseApplied));
            rb.impulseApplied = Vector3D(0.0f, 0.0f, 0.0f);
        }
    }

    // ========== RUN PHYSICS SIMULATION ==========
    physics.Update(safeFixedDt, /*collisionSteps=*/1, temp.get(), jobs.get());

    // ========== DISPATCH COLLISION/TRIGGER EVENTS TO LUA ==========
    if (contactListener && ecsManager.scriptSystem) {
        m_enterEventsScratch.clear();
        m_exitEventsScratch.clear();
        contactListener->DrainEvents(m_enterEventsScratch, m_exitEventsScratch);

        // PROCESS ENTER EVENTS.
        for (const auto& evt : m_enterEventsScratch) {
            Entity a = static_cast<Entity>(evt.entityA);
            Entity b = static_cast<Entity>(evt.entityB);

            InsertInteraction(m_activeInteractions, a, b);

            bool aIsTrigger = false, bIsTrigger = false;
            if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(a))
                aIsTrigger = rigidBody->get().isTrigger;
            if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(b))
                bIsTrigger = rigidBody->get().isTrigger;

            const std::string& fn = (aIsTrigger || bIsTrigger) ? kOnTriggerEnter : kOnCollisionEnter;
            ecsManager.scriptSystem->CallEntityFunctionWithInt(a, fn, evt.entityB, ecsManager);
            ecsManager.scriptSystem->CallEntityFunctionWithInt(b, fn, evt.entityA, ecsManager);

            // Notify DialogueManager for trigger-based dialogue advancement
            if (aIsTrigger || bIsTrigger) {
                if (aIsTrigger) NarrativeDialogueManager::GetInstance().OnTriggerEnter(a, b);
                if (bIsTrigger) NarrativeDialogueManager::GetInstance().OnTriggerEnter(b, a);
            }
        }

        // PROCESS EXIT EVENTS.
        for (const auto& evt : m_exitEventsScratch) {
            Entity a = static_cast<Entity>(evt.entityA);
            Entity b = static_cast<Entity>(evt.entityB);

            EraseInteraction(m_activeInteractions, a, b);

            bool aIsTrigger = false, bIsTrigger = false;
            if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(a))
                aIsTrigger = rigidBody->get().isTrigger;
            if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(b))
                bIsTrigger = rigidBody->get().isTrigger;

            const std::string& fn = (aIsTrigger || bIsTrigger) ? kOnTriggerExit : kOnCollisionExit;
            ecsManager.scriptSystem->CallEntityFunctionWithInt(a, fn, evt.entityB, ecsManager);
            ecsManager.scriptSystem->CallEntityFunctionWithInt(b, fn, evt.entityA, ecsManager);
        }

        // PROCESS STAY EVENTS
        // Iterate through all currently active pairs and fire "OnTriggerStay"
        for (std::size_t interactionIndex = 0;
            interactionIndex < m_activeInteractions.size();) {
            const auto [a, b] = m_activeInteractions[interactionIndex];

            // Safety check for a missed exit event after entity destruction.
            if (!ecsManager.IsEntityAlive(a) || !ecsManager.IsEntityAlive(b)) {
                m_activeInteractions.erase(
                    m_activeInteractions.begin() +
                    static_cast<std::ptrdiff_t>(interactionIndex));
                continue;
            }

            const bool aHasScripts =
                ecsManager.HasComponent<ScriptComponentData>(a);
            const bool bHasScripts =
                ecsManager.HasComponent<ScriptComponentData>(b);

            // Stay callbacks are uncommon (the current game has one). Keep the
            // interaction for correct future component changes, but skip all
            // hierarchy/component work when neither endpoint can consume Stay.
            const bool aMayHandleStay = aHasScripts &&
                (ecsManager.scriptSystem->CanEntityHandleIntEvent(a, kOnTriggerStay) ||
                 ecsManager.scriptSystem->CanEntityHandleIntEvent(a, kOnCollisionStay));
            const bool bMayHandleStay = bHasScripts &&
                (ecsManager.scriptSystem->CanEntityHandleIntEvent(b, kOnTriggerStay) ||
                 ecsManager.scriptSystem->CanEntityHandleIntEvent(b, kOnCollisionStay));
            if (!aMayHandleStay && !bMayHandleStay) {
                ++interactionIndex;
                continue;
            }

            if (!ecsManager.IsEntityActiveInHierarchy(a) ||
                !ecsManager.IsEntityActiveInHierarchy(b)) {
                m_activeInteractions.erase(
                    m_activeInteractions.begin() +
                    static_cast<std::ptrdiff_t>(interactionIndex));
                continue;
            }

            bool aIsTrigger = false, bIsTrigger = false;
            // Check components again as they might have changed at runtime
            if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(a))
                aIsTrigger = rigidBody->get().isTrigger;
            if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(b))
                bIsTrigger = rigidBody->get().isTrigger;

            // Determine if this is a Trigger Stay or Collision Stay
            const std::string& fn = (aIsTrigger || bIsTrigger) ? kOnTriggerStay : kOnCollisionStay;

            if (aHasScripts &&
                ecsManager.scriptSystem->CanEntityHandleIntEvent(a, fn)) {
                ecsManager.scriptSystem->CallEntityFunctionWithInt(
                    a, fn, static_cast<int>(b), ecsManager);
            }
            if (bHasScripts &&
                ecsManager.scriptSystem->CanEntityHandleIntEvent(b, fn)) {
                ecsManager.scriptSystem->CallEntityFunctionWithInt(
                    b, fn, static_cast<int>(a), ecsManager);
            }

            ++interactionIndex;
        }
    }

    // ========== SYNC JOLT -> ECS (after physics step) ==========
    PhysicsSyncBack(ecsManager);
}

void PhysicsSystem::EditorUpdate(ECSManager& ecs) {
    //for (const auto& entity : entities) {
    //    if (ecs.HasComponent<Transform>(entity)) {
    //        auto& transform = ecs.GetComponent<Transform>(entity);

    //        // If the transform system marked this as dirty (via Gizmo or SetDirtyRecursive)
    //        if (transform.isDirty) {
    //            SyncPhysicsBodyToTransform(entity, ecs);

    //            // Note: We DO NOT clear transform.isDirty here. 
    //            // The TransformSystem should clear it at the end of the frame 
    //            // after all systems (Rendering, Physics, etc.) have had their turn.
    //        }
    //    }
    //}
}

void PhysicsSystem::PhysicsSyncBack(ECSManager& ecsManager) {
//#ifdef __ANDROID__
//    static int syncCount = 0;
//    if (syncCount++ % 60 == 0) {
//        __android_log_print(ANDROID_LOG_INFO, "GAM300",
//            "[Physics] physicsSyncBack called, entities=%zu", entities.size());
//    }
    //#endif

    // Update() already built aligned compact entity/body lists. Lock all body
    // mutex stripes once instead of taking one read lock per dynamic body.
    const std::size_t dynamicCount = std::min(
        m_dynamicEntitiesScratch.size(), m_dynamicBodyIdsScratch.size());
    if (dynamicCount == 0) return;

    JPH::BodyLockMultiRead bodyLocks(
        physics.GetBodyLockInterface(),
        m_dynamicBodyIdsScratch.data(),
        static_cast<int>(dynamicCount));

    for (std::size_t index = 0; index < dynamicCount; ++index) {
        const Entity e = m_dynamicEntitiesScratch[index];
        // Collision callbacks dispatched earlier this update may have destroyed
        // entities or removed their physics components, so never assume the
        // lists built at the start of Update() are still fully valid.
        auto rbOpt = ecsManager.TryGetComponent<RigidBodyComponent>(e);
        if (!rbOpt) continue;
        auto& rb = rbOpt->get();
        if (!rb.enabled || rb.motion != Motion::Dynamic || !rb.physicsWorldAdded) continue;

        const JPH::Body* body = bodyLocks.GetBody(static_cast<int>(index));
        if (!body) continue;
        auto trOpt = ecsManager.TryGetComponent<Transform>(e);
        auto colOpt = ecsManager.TryGetComponent<ColliderComponent>(e);
        if (!trOpt || !colOpt) continue;
        auto& tr = trOpt->get();
        auto& col = colOpt->get();

        const JPH::RVec3 p = body->GetPosition();
        const JPH::Quat r = body->GetRotation();

            // Commented out as not used to fix warnings.
            // float offsetY = col.center.y * tr.localScale.y;     //in case meshes pivot start from the bottom instead of center

            Vector3D scaledOffset = {
                col.center.x * tr.worldScale.x,
                col.center.y * tr.worldScale.y,
                col.center.z * tr.worldScale.z
            };

            // Rotate offset to world space and SUBTRACT to get entity position
            JPH::Vec3 offsetInWorld = r * JPH::Vec3(scaledOffset.x, scaledOffset.y, scaledOffset.z);
            JPH::RVec3 entityPos = p - offsetInWorld;

            Quaternion entityWorldRot(r.GetW(), r.GetX(), r.GetY(), r.GetZ());

            // WRITE to ECS Transform (so renderer/other systems can see it)
            if (ecsManager.HasComponent<ParentComponent>(e)) {
                // Use TransformSystem to set world position (it handles parent conversion)
                ecsManager.transformSystem->SetWorldPosition(e, Vector3D(entityPos.GetX(), entityPos.GetY(), entityPos.GetZ()));
                ecsManager.transformSystem->SetWorldRotation(e, entityWorldRot);
            }
            else {
                ecsManager.transformSystem->SetLocalPosition(e, Vector3D(entityPos.GetX(), entityPos.GetY(), entityPos.GetZ()));
                ecsManager.transformSystem->SetLocalRotation(e, entityWorldRot);
            }

//#ifdef __ANDROID__
//            if (syncCount % 60 == 0) {
//                __android_log_print(ANDROID_LOG_INFO, "GAM300",
//                    "[Physics] Dynamic body pos: (%f, %f, %f)",
//                    p.GetX(), p.GetY(), p.GetZ());
//            }
//#endif
    }
}


void PhysicsSystem::Shutdown() {
    Shutdown(ECSRegistry::GetInstance().GetActiveECSManager());
}

void PhysicsSystem::Shutdown(ECSManager& ecs) {
    // 1. Remove and destroy all bodies using entityBodyMap
    auto& bi = physics.GetBodyInterface();
    for (auto& [entity, bodyId] : entityBodyMap) {
        if (!bodyId.IsInvalid()) {
            if (bi.IsAdded(bodyId)) {
                bi.RemoveBody(bodyId);
            }
            bi.DestroyBody(bodyId);
        }
    }
    entityBodyMap.clear();
    bodyToEntityMap.clear();
    m_activeInteractions.clear();

    // 2. Drop shapes and runtime handles on the ECS manager that owns this
    //    system. Scene transitions can switch the active manager before the
    //    old scene's entities are cleared, so never look it up globally here.
    for (auto e : entities) {
        if (auto rigidBody = ecs.TryGetComponent<RigidBodyComponent>(e)) {
            rigidBody->get().id = JPH::BodyID();
            rigidBody->get().physicsWorldAdded = false;
        }
        if (auto collider = ecs.TryGetComponent<ColliderComponent>(e)) {
            collider->get().shape = nullptr;
        }
    }
    //entities.clear();

    // 3. Destroy PhysicsSystem *before* releasing job/temp allocators
    //physics.~PhysicsSystem();   // or wrap in unique_ptr and reset()

    // 4. Now release allocators
    //jobs.reset();
    //temp.reset();

    // 5. Finally unregister types if you registered them
    // JPH::UnregisterTypes();
}

void PhysicsSystem::CreatePhysicsBody(Entity e, ECSManager& ecsManager) {
    if (!ecsManager.HasComponent<RigidBodyComponent>(e) ||
        !ecsManager.HasComponent<ColliderComponent>(e) ||
        !ecsManager.HasComponent<Transform>(e)) {
        return;
    }

    auto& tr = ecsManager.GetComponent<Transform>(e);
    auto& col = ecsManager.GetComponent<ColliderComponent>(e);
    auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // Runtime Jolt handles are never persistent component state. Keep failed
    // body creation from leaving a stale handle for the direct-ID hot paths.
    rb.id = JPH::BodyID();
    rb.physicsWorldAdded = false;
    rb.motion = static_cast<Motion>(rb.motionID);

    // --- FIX FOR NEWLY SPAWNED ENTITIES ---
    // If an entity is spawned in a script, its worldMatrix might still be Identity 
    // because TransformSystem hasn't run yet. We calculate a temporary one here 
    // to ensure the body spawns at the correct position/rotation.
    Matrix4x4 spawnMatrix = tr.worldMatrix;
    if (tr.isDirty) {
        // Simple TRS calculation (ignoring parent for frame 1 stability)
        spawnMatrix = TransformSystem::CalculateModelMatrix(
            tr.localPosition, tr.localScale, tr.localRotation
        );
    }

    // 1. Extract Raw Axes from the Matrix
    JPH::Vec3 axisX(spawnMatrix.m.m00, spawnMatrix.m.m10, spawnMatrix.m.m20);
    JPH::Vec3 axisY(spawnMatrix.m.m01, spawnMatrix.m.m11, spawnMatrix.m.m21);
    JPH::Vec3 axisZ(spawnMatrix.m.m02, spawnMatrix.m.m12, spawnMatrix.m.m22);

    // 2. Extract Scale
    float sx = axisX.Length();
    float sy = axisY.Length();
    float sz = axisZ.Length();

    // 3. Extract Rotation
    JPH::Vec3 normX = (sx > 0.0001f) ? axisX / sx : JPH::Vec3(1, 0, 0);
    JPH::Vec3 normY = (sy > 0.0001f) ? axisY / sy : JPH::Vec3(0, 1, 0);
    JPH::Vec3 normZ = (sz > 0.0001f) ? axisZ / sz : JPH::Vec3(0, 0, 1);

    JPH::Mat44 rotationMatrix = JPH::Mat44::sIdentity();
    rotationMatrix.SetColumn3(0, normX);
    rotationMatrix.SetColumn3(1, normY);
    rotationMatrix.SetColumn3(2, normZ);
    JPH::Quat rot = rotationMatrix.GetRotation().GetQuaternion();

    // 4. Calculate Center Position
    JPH::Vec3 centerOffset = (axisX * col.center.x) + (axisY * col.center.y) + (axisZ * col.center.z);
    JPH::RVec3 updatedPos = JPH::RVec3(spawnMatrix.m.m03, spawnMatrix.m.m13, spawnMatrix.m.m23) + centerOffset;

    // ... Layer Logic ...
    int ecsLayerIndex = -1;
    if (ecsManager.HasComponent<LayerComponent>(e))
        ecsLayerIndex = ecsManager.GetComponent<LayerComponent>(e).layerIndex;

    const int groundIdx = LayerManager::GetInstance().GetLayerIndex("Ground");
    const int obstacleIdx = LayerManager::GetInstance().GetLayerIndex("Obstacle");

    if (col.layer == Layers::HURTBOX) { /*...*/ }
    else if (col.layer == Layers::CHAIN_HITBOX) { /*...*/ }
    else if (col.layer == Layers::CHARACTER) { /* preserve editor/CharacterController layer */ }
    else if (rb.isTrigger) col.layer = Layers::SENSOR;
    else if (ecsLayerIndex == groundIdx) col.layer = Layers::NAV_GROUND;
    else if (ecsLayerIndex == obstacleIdx) col.layer = Layers::NAV_OBSTACLE;
    else {
        if (rb.motion == Motion::Static) col.layer = Layers::NON_MOVING;
        else col.layer = Layers::MOVING;
    }

    // Create Shape (Copied logic)
    {
        switch (col.shapeType) {
        case ColliderShapeType::Box: {
            float hx = std::abs(col.boxHalfExtents.x) * sx;
            float hy = std::abs(col.boxHalfExtents.y) * sy;
            float hz = std::abs(col.boxHalfExtents.z) * sz;
            constexpr float kMinHalf = 0.05f;
            JPH::BoxShapeSettings settings(JPH::Vec3(std::max(hx, kMinHalf), std::max(hy, kMinHalf), std::max(hz, kMinHalf)));
            settings.mConvexRadius = 0.0f;
            JPH::Shape::ShapeResult result = settings.Create();
            col.shape = result.IsValid() ? result.Get() : new JPH::BoxShape(JPH::Vec3(hx, hy, hz));
            break;
        }
        case ColliderShapeType::Sphere:
            col.shape = new JPH::SphereShape(col.sphereRadius * std::max({ sx, sy, sz }));
            break;
        case ColliderShapeType::Capsule:
            col.shape = new JPH::CapsuleShape(col.capsuleHalfHeight * sy, col.capsuleRadius * std::max(sx, sz));
            break;
        case ColliderShapeType::Cylinder:
            col.shape = new JPH::CylinderShape(col.cylinderHalfHeight * sy, col.cylinderRadius * std::max(sx, sz));
            break;
        case ColliderShapeType::MeshShape: {
            if (ecsManager.HasComponent<ModelRenderComponent>(e)) {
                auto& rc = ecsManager.GetComponent<ModelRenderComponent>(e);
                if (rc.model && rc.model->meshes.size() > 0) {
                    JPH::TriangleList triangles;
                    for (const auto& mesh : rc.model->meshes) {
                        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                            const glm::vec3& p0Source =
                                mesh.GetCollisionPosition(mesh.indices[i]);
                            const glm::vec3& p1Source =
                                mesh.GetCollisionPosition(mesh.indices[i + 1]);
                            const glm::vec3& p2Source =
                                mesh.GetCollisionPosition(mesh.indices[i + 2]);
                            JPH::Float3 p0(p0Source.x * sx, p0Source.y * sy, p0Source.z * sz);
                            JPH::Float3 p1(p1Source.x * sx, p1Source.y * sy, p1Source.z * sz);
                            JPH::Float3 p2(p2Source.x * sx, p2Source.y * sy, p2Source.z * sz);
                            triangles.push_back(JPH::Triangle(p0, p1, p2));
                        }
                    }
                    JPH::MeshShapeSettings meshSettings(triangles);
                    JPH::Shape::ShapeResult result = meshSettings.Create();
                    if (result.IsValid()) col.shape = result.Get();
                }
            }
            if (!col.shape) return; // Skip if mesh failed
            break;
        }
        }
    }

    // Apply local shape rotation if set (Euler degrees → quaternion, wrapped via RotatedTranslatedShape)
    {
        const auto& sr = col.shapeRotation;
        if (sr.x != 0.0f || sr.y != 0.0f || sr.z != 0.0f) {
            constexpr float kDeg2Rad = 0.01745329252f;
            JPH::Quat shapeRot = JPH::Quat::sEulerAngles(
                JPH::Vec3(sr.x * kDeg2Rad, sr.y * kDeg2Rad, sr.z * kDeg2Rad));
            JPH::RotatedTranslatedShapeSettings rts(JPH::Vec3::sZero(), shapeRot, col.shape);
            auto result = rts.Create();
            if (result.IsValid()) col.shape = result.Get();
        }
    }

    const auto motionType =
        rb.motion == Motion::Static ? JPH::EMotionType::Static :
        rb.motion == Motion::Kinematic ? JPH::EMotionType::Kinematic :
        JPH::EMotionType::Dynamic;

    JPH::BodyCreationSettings bcs(col.shape.GetPtr(), updatedPos, rot, motionType, col.layer);
    bcs.mAllowDynamicOrKinematic = true;  // Allow runtime motion type changes (needed for hurtbox conversion)

    if (motionType == JPH::EMotionType::Dynamic)
        bcs.mMotionQuality = rb.ccd ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
    if (motionType == JPH::EMotionType::Kinematic) {
        bcs.mCollideKinematicVsNonDynamic = true;
        bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
    }

    bcs.mRestitution = 0.2f;
    bcs.mFriction = 0.5f;
    bcs.mLinearDamping = rb.linearDamping;
    bcs.mAngularDamping = rb.angularDamping;
    bcs.mGravityFactor = rb.gravityFactor;
    bcs.mIsSensor = rb.isTrigger;

    JPH::BodyID newBodyId = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);
    entityBodyMap[e] = newBodyId;
    bodyToEntityMap[newBodyId] = e;
    rb.id = newBodyId;
    rb.collider_seen_version = col.version;
    rb.transform_dirty = rb.motion_dirty = false;
    rb.physicsWorldAdded = !newBodyId.IsInvalid();
    rb.appliedGravityFactor = rb.gravityFactor;
    rb.appliedIsTrigger = rb.isTrigger;
    //bi.SetMaxAngularVelocity(newBodyId, 2.0f);
}

void PhysicsSystem::RemoveBody(Entity entity, ECSManager& ecsManager) {
    auto it = entityBodyMap.find(entity);
    if (it == entityBodyMap.end()) return;

    JPH::BodyID bodyId = it->second;
    JPH::BodyInterface& bi = physics.GetBodyInterface();

    if (!bodyId.IsInvalid()) {
        if (bi.IsAdded(bodyId))
            bi.RemoveBody(bodyId);
        bi.DestroyBody(bodyId);
    }

    bodyToEntityMap.erase(bodyId);
    entityBodyMap.erase(it);

    if (auto rigidBody = ecsManager.TryGetComponent<RigidBodyComponent>(entity)) {
        rigidBody->get().id = JPH::BodyID();
        rigidBody->get().physicsWorldAdded = false;
    }
}

JPH::BodyID PhysicsSystem::GetBodyID(Entity entity) const {
    auto it = entityBodyMap.find(entity);
    if (it == entityBodyMap.end()) return JPH::BodyID();
    return it->second;
}

void PhysicsSystem::ConvertToKinematicHurtbox(Entity entity) {
    auto it = entityBodyMap.find(entity);
    if (it == entityBodyMap.end() || it->second.IsInvalid()) return;

    JPH::BodyID bodyId = it->second;
    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // Convert to kinematic so it doesn't respond to forces but stays in broadphase
    bi.SetMotionType(bodyId, JPH::EMotionType::Kinematic, JPH::EActivation::Activate);

    // Move to MOVING layer so broadphase handles kinematic updates correctly
    bi.SetObjectLayer(bodyId, Layers::MOVING);
}

// Custom filter that ignores sensors and character layer (for camera collision)
class CameraRaycastBroadPhaseFilter : public JPH::BroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override {
        // Hit everything except character layer
        return inLayer != BroadPhaseLayers::CHARACTER;
    }
};

class CameraRaycastObjectFilter : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        // Only hit solid geometry — ignore sensors, characters, hurtboxes, and kinematic bodies
        return inLayer != Layers::SENSOR
            && inLayer != Layers::CHARACTER
            && inLayer != Layers::MOVING
            && inLayer != Layers::HURTBOX;
    }
};

PhysicsSystem::RaycastResult PhysicsSystem::Raycast(const Vector3D& origin, const Vector3D& direction, float maxDistance) {
    RaycastResult result;

    // Normalize direction
    float dirLen = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (dirLen < 0.0001f) return result;

    JPH::Vec3 dir(direction.x / dirLen, direction.y / dirLen, direction.z / dirLen);
    JPH::RVec3 start(origin.x, origin.y, origin.z);

    // Create the ray - direction vector represents the full ray extent
    JPH::RRayCast ray(start, dir * maxDistance);

    // Get the narrow phase query interface
    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();

    // Filter out sensors, characters, and kinematic hurtboxes (MOVING layer)
    CameraRaycastBroadPhaseFilter bpFilter;
    CameraRaycastObjectFilter objFilter;
    JPH::RayCastResult hit;
    if (query.CastRay(ray, hit, bpFilter, objFilter)) {
        result.hit = true;
        result.distance = hit.mFraction * maxDistance;

        // Calculate hit point
        JPH::RVec3 hitPos = start + dir * result.distance;
        result.hitPoint = Vector3D(static_cast<float>(hitPos.GetX()),
                                   static_cast<float>(hitPos.GetY()),
                                   static_cast<float>(hitPos.GetZ()));

        result.bodyId = hit.mBodyID;
        result.entityId = GetEntityFromBody(hit.mBodyID);
    }

    return result;
}

// Line-of-sight raycast: only hits solid static world geometry (NON_MOVING).
// Ignores nav mesh layers, debris, chain hitboxes — things that shouldn't block sight.
class LOSObjectFilter : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == Layers::NON_MOVING;
    }
};

PhysicsSystem::RaycastResult PhysicsSystem::RaycastLOS(const Vector3D& origin, const Vector3D& direction, float maxDistance) {
    RaycastResult result;

    float dirLen = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (dirLen < 0.0001f) return result;

    JPH::Vec3 dir(direction.x / dirLen, direction.y / dirLen, direction.z / dirLen);
    JPH::RVec3 start(origin.x, origin.y, origin.z);
    JPH::RRayCast ray(start, dir * maxDistance);

    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();

    CameraRaycastBroadPhaseFilter bpFilter;
    LOSObjectFilter objFilter;
    JPH::RayCastResult hit;
    if (query.CastRay(ray, hit, bpFilter, objFilter)) {
        result.hit = true;
        result.distance = hit.mFraction * maxDistance;

        JPH::RVec3 hitPos = start + dir * result.distance;
        result.hitPoint = Vector3D(static_cast<float>(hitPos.GetX()),
                                   static_cast<float>(hitPos.GetY()),
                                   static_cast<float>(hitPos.GetZ()));

        result.bodyId = hit.mBodyID;
        result.entityId = GetEntityFromBody(hit.mBodyID);
    }

    return result;
}

//PhysicsSystem::RaycastResult PhysicsSystem::RaycastGroundOnly(
//    const Vector3D& origin,
//    float maxDistance
//) {
//    RaycastResult result;
//
//    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();
//    JPH::BodyInterface& bi = physics.GetBodyInterface();
//
//    // Down ray
//    const JPH::Vec3 dir(0.0f, -1.0f, 0.0f);
//
//    JPH::RVec3 start(origin.x, origin.y, origin.z);
//    float remaining = maxDistance;
//
//    constexpr int   MAX_SKIPS = 24;
//    constexpr float SKIP_EPS = 0.02f; // move slightly past the hit surface
//
//    for (int i = 0; i < MAX_SKIPS && remaining > 0.0f; ++i)
//    {
//        JPH::RRayCast ray(start, dir * remaining);
//
//        JPH::RayCastResult hit;
//        if (!query.CastRay(ray, hit))
//            return result; // no hit at all
//
//        const JPH::BodyID body = hit.mBodyID;
//        if (body.IsInvalid())
//            return result;
//
//        // How far along this sub-ray we hit
//        const float traveled = hit.mFraction * remaining;
//
//        // Decide whether to ignore this hit
//        bool ignore = false;
//
//        // Ignore Jolt layers you never want nav ground to be
//        const JPH::ObjectLayer ol = bi.GetObjectLayer(body);
//        if (ol == Layers::SENSOR || ol == Layers::CHARACTER)
//            ignore = true;
//
//        // Ignore ECS "Obstacle" layer (pedestal/pillars)
//        if (!ignore && BodyIsObstacle(body))
//            ignore = true;
//
//        if (ignore)
//        {
//            // Move start slightly past this surface and keep raycasting down
//            const float advance = traveled + SKIP_EPS;
//            start = start + dir * advance;
//            remaining -= advance;
//            continue;
//        }
//
//        // Accept this as ground
//        result.hit = true;
//        result.distance = traveled;
//
//        const JPH::RVec3 hitPos = start + dir * traveled;
//        result.hitPoint = Vector3D(
//            (float)hitPos.GetX(),
//            (float)hitPos.GetY(),
//            (float)hitPos.GetZ()
//        );
//        result.bodyId = body;
//        return result;
//    }
//
//    return result;
//}
//
//PhysicsSystem::RaycastResult PhysicsSystem::RaycastGroundIgnoreObstacles(
//    const Vector3D& origin, const Vector3D& direction, float maxDistance)
//{
//    RaycastResult result;
//
//    float dirLen = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
//    if (dirLen < 0.0001f) return result;
//
//    JPH::Vec3 dir(direction.x / dirLen, direction.y / dirLen, direction.z / dirLen);
//
//    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();
//
//    // We may hit an obstacle first (pedestal/stairs/pillar). Skip it and continue downward.
//    JPH::RVec3 start(origin.x, origin.y, origin.z);
//
//    constexpr int   kMaxSkips = 8;
//    constexpr float kEps = 0.02f;
//
//    for (int i = 0; i < kMaxSkips; ++i)
//    {
//        JPH::RRayCast ray(start, dir * maxDistance);
//
//        JPH::RayCastResult hit;
//        if (!query.CastRay(ray, hit))
//            return result; // no hit
//
//        // We hit something
//        const float dist = hit.mFraction * maxDistance;
//        const JPH::RVec3 hitPos = start + dir * dist;
//
//        const JPH::BodyID body = hit.mBodyID;
//        if (!body.IsInvalid() && BodyIsObstacle(body))
//        {
//            // Skip obstacle and continue slightly past it
//            start = hitPos + JPH::RVec3(0.0, -kEps, 0.0);
//            maxDistance -= dist;
//            if (maxDistance <= 0.0f) return result;
//            continue;
//        }
//
//        // Accept this as ground
//        result.hit = true;
//        result.distance = dist;
//        result.hitPoint = Vector3D((float)hitPos.GetX(), (float)hitPos.GetY(), (float)hitPos.GetZ());
//        result.bodyId = body;
//        return result;
//    }
//
//    return result;
//}

class NavRaycastObjectFilter : public JPH::ObjectLayerFilter
{
public:
    bool acceptObstacle = false;

    explicit NavRaycastObjectFilter(bool inAcceptObstacle)
        : acceptObstacle(inAcceptObstacle) {
    }

    bool ShouldCollide(JPH::ObjectLayer inLayer) const override
    {
        if (inLayer == Layers::NAV_GROUND) return true;
        if (acceptObstacle && inLayer == Layers::NAV_OBSTACLE) return true;
        return false; // ignore everything else (ceiling, knives, etc.)
    }
};

PhysicsSystem::RaycastResult PhysicsSystem::RaycastGround(
    const Vector3D& origin,
    const Vector3D& direction,
    float maxDistance,
    ECSManager& ecs,
    int groundIdx,
    int obstacleIdx,
    bool acceptObstacleAsHit,
    bool debugLog)
{
    RaycastResult result{};

    // normalize direction
    float dirLen = std::sqrt(direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);
    if (dirLen < 0.0001f) return result;

    const JPH::Vec3 dir(direction.x / dirLen,
        direction.y / dirLen,
        direction.z / dirLen);

    const JPH::RVec3 start(origin.x, origin.y, origin.z);
    const JPH::RRayCast ray(start, dir * maxDistance);

    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();

    // Filters: only hit NAV_GROUND (and NAV_OBSTACLE if enabled)
    NavRaycastObjectFilter objFilter(acceptObstacleAsHit);

    // Broadphase filter can stay permissive; object filter is doing the real work
    class NavRaycastBroadPhaseFilter : public JPH::BroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override
        {
            // NAV layers are in NON_MOVING broadphase; allow NON_MOVING
            return true;
        }
    } bpFilter;

    JPH::RayCastResult hit;
    const bool ok = query.CastRay(ray, hit, bpFilter, objFilter);

    if (!ok)
    {
        if (debugLog)
            //std::cout << "[RaycastGround] no hit\n";
        return result;
    }

    result.hit = true;
    result.bodyId = hit.mBodyID;

    // FIX: Use Jolt's exact hit position instead of recalculating
    // GetPointOnRay returns the exact hit point with full precision
    const JPH::RVec3 hitPos = ray.GetPointOnRay(hit.mFraction);

    result.hitPoint = Vector3D(static_cast<float>(hitPos.GetX()),
        static_cast<float>(hitPos.GetY()),
        static_cast<float>(hitPos.GetZ()));

    result.distance = hit.mFraction * maxDistance;

    //result.distance = hit.mFraction * maxDistance;

    //const JPH::RVec3 hitPos = start + dir * result.distance;
    //result.hitPoint = Vector3D((float)hitPos.GetX(),
    //    (float)hitPos.GetY(),
    //    (float)hitPos.GetZ());


    if (debugLog)
    {
        Entity e = GetEntityFromBody(result.bodyId);
        int layer = -1;
        if ((int)e != 0 && ecs.HasComponent<LayerComponent>(e))
            layer = ecs.GetComponent<LayerComponent>(e).layerIndex;

        const char* nm = "<noname>";
        if ((int)e != 0 && ecs.HasComponent<NameComponent>(e))
            nm = ecs.GetComponent<NameComponent>(e).name.c_str();

        /*std::cout << "[RaycastGround] HIT ent=" << (int)e
            << " name=" << nm
            << " ecsLayer=" << layer
            << " hitY=" << result.hitPoint.y
            << "\n";*/
    }

    return result;
}

PhysicsSystem::OverlapResult PhysicsSystem::OverlapCapsule(
    const Vector3D& center,
    float halfHeight,
    float radius
) {
    OverlapResult out;

    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();

    // capsule shape
    JPH::CapsuleShapeSettings capSettings(halfHeight, radius);
    auto shapeRes = capSettings.Create();
    if (!shapeRes.IsValid())
        return out;

    JPH::ShapeRefC shape = shapeRes.Get();

    JPH::RVec3 pos(center.x, center.y, center.z);
    JPH::Quat rot = JPH::Quat::sIdentity();

    // Filters (keep yours)
    class NavBroadPhaseFilter : public JPH::BroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override {
            return inLayer != BroadPhaseLayers::CHARACTER;
        }
    } bpFilter;

    class NavObjectLayerFilter : public JPH::ObjectLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            return inLayer != Layers::SENSOR && inLayer != Layers::CHARACTER;
        }
    } objFilter;

    // Collector: only treat ECS-layer Obstacle as blocking
    struct NavObstacleCollector : public JPH::CollideShapeCollector
    {
        const PhysicsSystem* ps = nullptr;
        bool anyHit = false;

        explicit NavObstacleCollector(const PhysicsSystem* inPs) : ps(inPs) {}

        void AddHit(const JPH::CollideShapeResult& hit) override
        {
            // For CollideShape, the hit body is usually mBodyID2.
            // If your compiler complains, change to hit.mBodyID1 or hit.mBodyID.
            const JPH::BodyID& body = hit.mBodyID2;

            if (ps && ps->BodyIsObstacle(body))
            {
                anyHit = true;
                ForceEarlyOut();
            }
        }
    } collector(this);

    JPH::CollideShapeSettings settings;
    JPH::Vec3 scale(1.0f, 1.0f, 1.0f);
    JPH::RVec3 baseOffset(0.0, 0.0, 0.0);

    query.CollideShape(
        shape.GetPtr(),
        scale,
        JPH::RMat44::sRotationTranslation(rot, pos),
        settings,
        baseOffset,
        collector,
        bpFilter,
        objFilter,
        JPH::BodyFilter(),
        JPH::ShapeFilter()
    );

    out.hit = collector.anyHit;
    return out;
}

bool PhysicsSystem::BodyIsInECSLayer(const JPH::BodyID& body, int layerIndex) const
{
    auto it = bodyToEntityMap.find(body);
    if (it == bodyToEntityMap.end()) return false;

    Entity e = static_cast<Entity>(it->second);

    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs.HasComponent<LayerComponent>(e)) return false;

    const auto& lc = ecs.GetComponent<LayerComponent>(e);
    return lc.layerIndex == layerIndex;
}

bool PhysicsSystem::BodyIsObstacle(const JPH::BodyID& body) const
{
    const int obstacleIdx = LayerManager::GetInstance().GetLayerIndex("Obstacle");
    return BodyIsInECSLayer(body, obstacleIdx);
}

bool PhysicsSystem::OverlapCapsuleObstacleLayer(
    const Vector3D& center,
    float halfHeight,
    float radius,
    ECSManager& ecsManager,
    int obstacleLayerIndex
) {
    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();

    // capsule shape
    JPH::CapsuleShapeSettings capSettings(halfHeight, radius);
    auto shapeRes = capSettings.Create();
    if (!shapeRes.IsValid())
        return false;

    JPH::ShapeRefC shape = shapeRes.Get();

    JPH::RVec3 pos(center.x, center.y, center.z);
    JPH::Quat rot = JPH::Quat::sIdentity();

    // Same filters you already use (ignore sensors + character layer)
    class NavBroadPhaseFilter : public JPH::BroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override {
            return inLayer != BroadPhaseLayers::CHARACTER;
        }
    } bpFilter;

    class NavObjectLayerFilter : public JPH::ObjectLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            return inLayer != Layers::SENSOR && inLayer != Layers::CHARACTER;
        }
    } objFilter;

    struct ObstacleOnlyCollector : public JPH::CollideShapeCollector {
        PhysicsSystem* phys = nullptr;
        ECSManager* ecs = nullptr;
        int obstacleIdx = -1;
        bool hitObstacle = false;

        void AddHit(const JPH::CollideShapeResult& hit) override {
            // Body that we collided with
            const JPH::BodyID bodyId = hit.mBodyID2;

            auto it = phys->bodyToEntityMap.find(bodyId);
            if (it == phys->bodyToEntityMap.end())
                return;

            Entity e = it->second;

            if (!ecs->HasComponent<LayerComponent>(e))
                return;

            auto& lc = ecs->GetComponent<LayerComponent>(e);
            if (lc.layerIndex == obstacleIdx) {
                hitObstacle = true;
                ForceEarlyOut();
            }
        }
    } collector;

    collector.phys = this;
    collector.ecs = &ecsManager;
    collector.obstacleIdx = obstacleLayerIndex;

    JPH::CollideShapeSettings settings;
    JPH::Vec3 scale(1.0f, 1.0f, 1.0f);
    JPH::RVec3 baseOffset(0.0, 0.0, 0.0);

    query.CollideShape(
        shape.GetPtr(),
        scale,
        JPH::RMat44::sRotationTranslation(rot, pos),
        settings,
        baseOffset,
        collector,
        bpFilter,
        objFilter,
        JPH::BodyFilter(),
        JPH::ShapeFilter()
    );

    return collector.hitObstacle;
}

Entity PhysicsSystem::GetEntityFromBody(const JPH::BodyID& id) const
{
    auto it = bodyToEntityMap.find(id);
    if (it != bodyToEntityMap.end())
        return static_cast<Entity>(it->second);

    return Entity{}; // replace with INVALID_ENTITY if your engine has one
}

bool PhysicsSystem::GetBodyWorldAABB(Entity e, JPH::AABox& outAABB) const
{
    auto it = entityBodyMap.find(e);
    if (it == entityBodyMap.end()) return false;

    const JPH::BodyID bodyId = it->second;
    if (bodyId.IsInvalid()) return false;

    const JPH::BodyLockInterface& bli = physics.GetBodyLockInterface();
    JPH::BodyLockRead lock(bli, bodyId);
    if (!lock.Succeeded()) return false;

    outAABB = lock.GetBody().GetWorldSpaceBounds();
    return true;
}

void PhysicsSystem::SyncPhysicsBodyToTransform(Entity entity, ECSManager& ecs) {
    if (!ecs.HasComponent<Transform>(entity)) return;

    auto& transform = ecs.GetComponent<Transform>(entity);

    // Get world transform data.
    Vector3D worldPos = transform.worldPosition;
    //Quaternion worldRot = Quaternion::FromEulerDegrees(transform.worldRotation);
    Vector3D worldScale = transform.worldScale;

    if (ecs.HasComponent<ColliderComponent>(entity)) {
        auto& col = ecs.GetComponent<ColliderComponent>(entity);
        
        Vector3D newColliderCenter = worldPos + col.center;

		col.center = newColliderCenter;
    }
}

void PhysicsSystem::UpdateColliderShapeScale(ColliderComponent& col, Vector3D worldScale) {
    if (col.shapeType == ColliderShapeType::Box) {
        col.boxHalfExtents *= worldScale;
    }
    else if (col.shapeType == ColliderShapeType::Sphere) {
        float maxScale = std::max({ worldScale.x, worldScale.y, worldScale.z });
		col.sphereRadius *= maxScale;
    }
    else if (col.shapeType == ColliderShapeType::Capsule) {
        float maxScale = std::max({ worldScale.x, worldScale.z }); // XZ for radius
        col.capsuleRadius *= maxScale;
        col.capsuleHalfHeight *= worldScale.y; // Y for height
	}
    else if (col.shapeType == ColliderShapeType::Cylinder) {
        float maxScale = std::max({ worldScale.x, worldScale.z }); // XZ for radius
        col.cylinderRadius *= maxScale;
		col.cylinderHalfHeight *= worldScale.y; // Y for height
    }
}

// Call: std::vector<Entity> out; GetOverlappingEntities(entity, out);
// Returns true if call succeeded (even if zero results). Results appended to 'out'.
bool PhysicsSystem::GetOverlappingEntities(Entity entity, std::vector<Entity>& out)
{
    out.clear();

    // 1) find the Jolt body
    auto it = entityBodyMap.find(entity);
    if (it == entityBodyMap.end()) return false;
    JPH::BodyID bodyId = it->second;
    if (bodyId.IsInvalid()) return false;

    // 2) lock the body for safe read access
    const JPH::BodyLockInterface& bli = physics.GetBodyLockInterface();
    JPH::BodyLockRead lock(bli, bodyId);
    if (!lock.Succeeded()) return false;

    // 3) grab shape pointer & transform from the locked body (safe while locked)
    const JPH::Body& body = lock.GetBody();
    const JPH::Shape* bodyShape = body.GetShape(); // Body::GetShape() is available.
    if (!bodyShape) return false;

    // Use the body's world transform (center-of-mass transform) for placing the shape
    JPH::RMat44 bodyTransform = body.GetWorldTransform();

    // 4) Prepare narrow-phase query + filters (reuse your existing filters)
    const JPH::NarrowPhaseQuery& query = physics.GetNarrowPhaseQuery();

    // Broadphase / object filters (reuse your existing rules - ignore character/sensor)
    class LocalBPFilter : public JPH::BroadPhaseLayerFilter { public: bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; } };
    class LocalObjFilter : public JPH::ObjectLayerFilter { public: bool ShouldCollide(JPH::ObjectLayer inLayer) const override { return inLayer != Layers::SENSOR && inLayer != Layers::CHARACTER; } } objFilter;
    LocalBPFilter bpFilter;

    // 5) Collector: gather unique hit body IDs (exclude self)
    struct CollectOverlaps : public JPH::CollideShapeCollector
    {
        PhysicsSystem* ps = nullptr;
        mutable std::vector<Entity>* out = nullptr;
        JPH::BodyID self{};
        void AddHit(const JPH::CollideShapeResult& hit) override
        {
            // Jolt reports collisions: use hit.mBodyID2 as the body we collided with (consistent with earlier code)
            const JPH::BodyID other = hit.mBodyID2;
            if (other.IsInvalid() || other == self) return;

            // Map to ECS entity
            auto it = ps->bodyToEntityMap.find(other);
            if (it != ps->bodyToEntityMap.end())
            {
                // Avoid duplicates: (simple check) only append if last != this one (or maintain set if many hits)
                // For small numbers of overlaps, linear check is fine:
                Entity e = static_cast<Entity>(it->second);
                bool found = false;
                for (auto existing : *out) { if (existing == e) { found = true; break; } }
                if (!found) out->push_back(e);
            }
        }
    } collector;

    collector.ps = this;
    collector.out = &out;
    collector.self = bodyId;

    // 6) Collide the shape at the body's transform (scale = 1, offset = zero)
    JPH::CollideShapeSettings settings;
    JPH::Vec3 scale(1.0f, 1.0f, 1.0f);
    JPH::RVec3 baseOffset(0.0f, 0.0f, 0.0f);

    // NOTE: this call performs narrow-phase checks for the given shape against the world.
    query.CollideShape(
        bodyShape,                    // shape pointer (no new allocation)
        scale,                        // shape scale (1 if shape already baked to world scale)
        bodyTransform,                // transform to place the shape in world
        settings,
        baseOffset,
        collector,
        bpFilter,
        objFilter,
        JPH::BodyFilter(),            // default (hits all bodies) - you can provide custom BodyFilter if needed
        JPH::ShapeFilter()            // default
    );

    return true;
}
