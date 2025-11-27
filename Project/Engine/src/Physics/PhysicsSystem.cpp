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
//#include "Physics/JoltInclude.hpp"
#include "Performance/PerformanceProfiler.hpp"

#include "Physics/PhysicsSystem.hpp"
#include "Physics/CollisionFilters.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include <cstdarg>

#ifdef __ANDROID__
#include <android/log.h>
#endif


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



bool PhysicsSystem::InitialiseJolt() {
    // Jolt one-time bootstrap
    static bool joltInitialized = false;
    if (!joltInitialized) {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Starting Jolt initialization...");
#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Registering default allocator...");
        JPH::RegisterDefaultAllocator();
#endif
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Setting trace and assert handlers...");
#else
        JPH::RegisterDefaultAllocator();
#endif
        JPH::Trace = JoltTrace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed; )

#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Creating Factory instance...");
#endif
        JPH::Factory::sInstance = new JPH::Factory();

#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Registering Jolt types...");
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_PROFILE_ENABLED=%d", JPH_PROFILE_ENABLED);
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_OBJECT_STREAM=%d", JPH_OBJECT_STREAM);
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=%d", JPH_FLOATING_POINT_EXCEPTIONS_ENABLED);
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_DISABLE_CUSTOM_ALLOCATOR=%d",
#ifdef JPH_DISABLE_CUSTOM_ALLOCATOR
            1
#else
            0
#endif
        );
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_OBJECT_
            
            
            
            _BITS=%d",
#ifdef JPH_OBJECT_LAYER_BITS
            JPH_OBJECT_LAYER_BITS
#else
            16  // default
#endif
        );
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_ENABLE_ASSERTS=%d",
#ifdef JPH_ENABLE_ASSERTS
            1
#else
            0
#endif
        );
#endif
        JPH::RegisterTypes();

#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Jolt initialization complete!");
#endif
        joltInitialized = true;
    }

    if (!temp) temp = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    if (!jobs) jobs = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::max(1u, std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() - 1 : 1u));

    physics.Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
        broadphase, objVsBP, objPair);
    physics.SetGravity(JPH::Vec3(0, -9.81f, 0));  // gravity set

    // Configure physics settings for better collision resolution
    JPH::PhysicsSettings settings;
    settings.mNumVelocitySteps = 10;      // Increase from default (10) to 15-20
    settings.mNumPositionSteps = 2;       // Increase from default (2) to 3-4
    settings.mBaumgarte = 0.2f;           // Penetration correction factor
    settings.mSpeculativeContactDistance = 0.02f;  // Predict contacts earlier
    settings.mPenetrationSlop = 0.02f;    // Allow small penetrations
    settings.mLinearCastThreshold = 0.75f; // CCD threshold
    physics.SetPhysicsSettings(settings);


#ifdef __ANDROID__
    JPH::Vec3 gravity = physics.GetGravity();
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Gravity set to: (%f, %f, %f)",
        gravity.GetX(), gravity.GetY(), gravity.GetZ());
#endif

    /*JPH::BodyInterface& bi = physics.GetBodyInterface();

    JPH::RefConst<JPH::Shape> groundShape = new JPH::BoxShape(JPH::Vec3(100, 1, 100));
    JPH::BodyCreationSettings gcs(
        groundShape.GetPtr(), JPH::RVec3(0, -1, 0), JPH::Quat::sIdentity(),
        JPH::EMotionType::Static, Layers::NON_MOVING
    );
    bi.CreateAndAddBody(gcs, JPH::EActivation::DontActivate);

    JPH::RefConst<JPH::Shape> boxShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    JPH::BodyCreationSettings bcs(
        boxShape.GetPtr(), JPH::RVec3(0, 3, 0), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, Layers::MOVING
    );
    bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);*/

    //auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    //for (const auto& entity : entities) {
    //    auto& 
    //  = ecs.GetComponent<ColliderComponent>(entity);
    //    switch (collider.shapeType)
    //    {
    //    case ColliderShapeType::Box:
    //        collider.shape = new JPH::BoxShape((JPH::Vec3(collider.boxHalfExtents.x, collider.boxHalfExtents.y, collider.boxHalfExtents.z)));
    //        break;
    //    default:
    //        break;
    //    }
    //}
// Create contact listener with access to the same map
    contactListener = std::make_unique<MyContactListener>(bodyToEntityMap);
    physics.SetContactListener(contactListener.get());
    return true;
}

void PhysicsSystem::Initialise(ECSManager& ecsManager) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] physicsAuthoring called, entities=%zu", entities.size());
#endif

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    for (auto& e : entities) {
        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        //apply motiontype
        rb.motion = static_cast<Motion>(rb.motionID);

        JPH::RVec3Arg pos(tr.localPosition.x, tr.localPosition.y, tr.localPosition.z);
        JPH::QuatArg rot = JPH::Quat::sIdentity();
        JPH_ASSERT(rot.IsNormalized());

        // --- Set proper collision layer ---
        if (rb.motion == Motion::Static || rb.motion == Motion::Kinematic)
            col.layer = Layers::NON_MOVING;
        else
            col.layer = Layers::MOVING;

        // --- Create or recreate body ---
        if (rb.id.IsInvalid() || rb.motion_dirty || rb.collider_seen_version != col.version) {
            // Remove existing body if it exists
            if (!rb.id.IsInvalid()) {
                bi.RemoveBody(rb.id);
                bi.DestroyBody(rb.id);
                rb.id = JPH::BodyID();
            }

            // ALWAYS create shape when creating/recreating body
            switch (col.shapeType) {
            case ColliderShapeType::Box:
                // Apply local scale
                col.shape = new JPH::BoxShape(JPH::Vec3(
                    col.boxHalfExtents.x * tr.localScale.x,
                    col.boxHalfExtents.y * tr.localScale.y,
                    col.boxHalfExtents.z * tr.localScale.z
                ));
                break;

            case ColliderShapeType::Sphere:
                col.shape = new JPH::SphereShape(col.sphereRadius * std::max({ tr.localScale.x, tr.localScale.y, tr.localScale.z }));
                break;

            case ColliderShapeType::Capsule:
                col.shape = new JPH::CapsuleShape(col.capsuleHalfHeight * tr.localScale.y,
                    col.capsuleRadius * std::max(tr.localScale.x, tr.localScale.z));
                break;

            case ColliderShapeType::Cylinder:
                col.shape = new JPH::CylinderShape(col.cylinderHalfHeight * tr.localScale.y,
                    col.cylinderRadius * std::max(tr.localScale.x, tr.localScale.z));
                break;
            }

            // Map our Motion enum to JPH::EMotionType
            const auto motionType =
                rb.motion == Motion::Static ? JPH::EMotionType::Static :
                rb.motion == Motion::Kinematic ? JPH::EMotionType::Kinematic :
                JPH::EMotionType::Dynamic;

            // Create body creation settings
            JPH::BodyCreationSettings bcs(col.shape.GetPtr(), pos, rot, motionType, col.layer);

            // --- Apply CCD according to component ---
            if (motionType == JPH::EMotionType::Dynamic)
                bcs.mMotionQuality = rb.ccd ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;

            // --- Apply damping and restitution ---
            bcs.mRestitution = 0.2f;
            bcs.mFriction = 0.5f;
            bcs.mLinearDamping = rb.linearDamping;
            bcs.mAngularDamping = rb.angularDamping;

            // --- Apply gravity factor ---
            bcs.mGravityFactor = rb.gravityFactor;

            // Create and add the body to the physics system
            rb.id = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);
            bodyToEntityMap[rb.id] = e; // Map Jolt body ID to ECS entity

            // Update bookkeeping
            rb.collider_seen_version = col.version;
            rb.transform_dirty = rb.motion_dirty = false;

            // Limit angular velocity to avoid instability
            bi.SetMaxAngularVelocity(rb.id, 2.0f);

#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "GAM300",
                "[Physics] Created body id=%u, motion=%d, CCD=%d, pos=(%f,%f,%f)",
                rb.id.GetIndex(), static_cast<int>(motionType), rb.ccd,   
                pos.GetX(), pos.GetY(), pos.GetZ());
#endif
        }

        // --- Update kinematic / static transforms ---
        if ((rb.motion == Motion::Kinematic || rb.motion == Motion::Static) && rb.transform_dirty) {
            if (rb.motion == Motion::Kinematic)
                bi.MoveKinematic(rb.id, pos, rot, 0.0f);
            else
                bi.SetPositionAndRotation(rb.id, pos, rot, JPH::EActivation::DontActivate);

            rb.transform_dirty = false;
        }
    }
}


void PhysicsSystem::Update(float fixedDt, ECSManager& ecsManager) {
    PROFILE_FUNCTION();
#ifdef __ANDROID__
    static int updateCount = 0;
    if (updateCount++ % 60 == 0) { // Log every 60 frames
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Update called, fixedDt=%f, entities=%zu", fixedDt, entities.size());
    }
#endif
    if (entities.empty()) return;


    // Sync ECS -> Jolt (before physics step)
    JPH::BodyInterface& bi = physics.GetBodyInterface();
    for (auto& e : entities) {
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        bi.SetGravityFactor(rb.id, rb.gravityFactor);
        bi.SetIsSensor(rb.id, rb.isTrigger);
        rb.ccd ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;

        // Read back velocities from physics engine
        rb.angularVel = FromJoltVec3(bi.GetAngularVelocity(rb.id));
        rb.linearVel = FromJoltVec3(bi.GetLinearVelocity(rb.id));
    }

    // Run physics with fixed timestep (dt is already fixed from TimeManager)
    physics.Update(fixedDt, /*collisionSteps=*/4, temp.get(), jobs.get());
    
    // Sync Jolt -> ECS (after physics step)
    PhysicsSyncBack(ecsManager);
}

void PhysicsSystem::PhysicsSyncBack(ECSManager& ecsManager) {
    auto& bi = physics.GetBodyInterface();

#ifdef __ANDROID__
    static int syncCount = 0;
    if (syncCount++ % 60 == 0) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300",
            "[Physics] physicsSyncBack called, entities=%zu", entities.size());
}
#endif

    for (auto& e : entities) {
        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        if (rb.id.IsInvalid()) continue;

        // Only sync Dynamic bodies (physics controls them)
        // Kinematic/Static bodies are controlled by Transform, not physics
        if (rb.motion == Motion::Dynamic) {
            JPH::RVec3 p;
            JPH::Quat r;
            bi.GetPositionAndRotation(rb.id, p, r);

            // WRITE to ECS Transform (so renderer/other systems can see it)
            tr.localPosition = Vector3D(p.GetX(), p.GetY(), p.GetZ());
            tr.localRotation = Quaternion(r.GetX(), r.GetY(), r.GetZ(), r.GetW());
            tr.isDirty = true;

#ifdef __ANDROID__
            if (syncCount % 60 == 0) {
                __android_log_print(ANDROID_LOG_INFO, "GAM300",
                    "[Physics] Dynamic body pos: (%f, %f, %f)",
                    p.GetX(), p.GetY(), p.GetZ());
        }
#endif
    }
    }
}


void PhysicsSystem::Shutdown() {
    // 1. Remove and destroy all bodies

    //entities.clear();
    auto& bi = physics.GetBodyInterface();
    for (auto e : entities) {
        auto& rb = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<RigidBodyComponent>(e);
        if (!rb.id.IsInvalid()) {
            bi.RemoveBody(rb.id);
            bi.DestroyBody(rb.id);
            rb.id = JPH::BodyID();
        }
    }

    // 2. Drop collider shapes
    for (auto e : entities) {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        auto& col = ecs.GetComponent<ColliderComponent>(e);
        col.shape = nullptr;
    }
    // 3. Destroy PhysicsSystem *before* releasing job/temp allocators
    //physics.~PhysicsSystem();   // or wrap in unique_ptr and reset()

    // 4. Now release allocators
    //jobs.reset();
    //temp.reset();

    // 5. Finally unregister types if you registered them
    // JPH::UnregisterTypes();
}
