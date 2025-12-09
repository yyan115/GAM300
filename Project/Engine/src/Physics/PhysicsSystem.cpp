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
    // Jolt one-time bootstrap (types/allocator) - shared across all instances
    static bool joltTypesInitialized = false;

    if (!joltTypesInitialized) {
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



#ifdef JPH_OBJECT_LAYER_BITS
        //JPH_OBJECT_LAYER_BITS
#else
        16  // default
#endif
        //);
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_ENABLE_ASSERTS=%d",
#ifdef JPH_ENABLE_ASSERTS
            //1
#else
            0
#endif
            //);
#endif
            JPH::RegisterTypes();

#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] Jolt initialization complete!");
#endif
        joltTypesInitialized = true;
    }

    // Only initialize THIS instance's physics system once (calling physics.Init() again would wipe all bodies!)
    if (!m_joltInitialized) {
        std::cout << "[Physics] InitialiseJolt: Creating physics world for this instance..." << std::endl;

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

        // Create contact listener with access to the same map
        contactListener = std::make_unique<MyContactListener>(bodyToEntityMap);
        physics.SetContactListener(contactListener.get());

        m_joltInitialized = true;
    } else {
        std::cout << "[Physics] InitialiseJolt: This instance already initialized, skipping Init()" << std::endl;
    }

    return true;
}


void PhysicsSystem::Initialise(ECSManager& ecsManager) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] physicsAuthoring called, entities=%zu", entities.size());
#endif

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // Remove any previously created bodies from last play session
    for (auto& [entity, bodyId] : entityBodyMap) {
        if (!bodyId.IsInvalid() && bi.IsAdded(bodyId)) {
            bi.RemoveBody(bodyId);
            bi.DestroyBody(bodyId);
        }
    }
    entityBodyMap.clear();
    bodyToEntityMap.clear();

    for (auto& e : entities) {
        // Skip entities that don't actually have the required components
        if (!ecsManager.HasComponent<RigidBodyComponent>(e) ||
            !ecsManager.HasComponent<ColliderComponent>(e) ||
            !ecsManager.HasComponent<Transform>(e)) {
            continue;
        }

        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        //apply motiontype
        rb.motion = static_cast<Motion>(rb.motionID);

        JPH::RVec3Arg pos(tr.localPosition.x, tr.localPosition.y, tr.localPosition.z);
        // Convert rotation from ECS to Jolt
        JPH::Quat rot = JPH::Quat(tr.localRotation.x, tr.localRotation.y,
            tr.localRotation.z, tr.localRotation.w);
        rot = rot.Normalized();  // Safety normalization
        JPH_ASSERT(rot.IsNormalized());

        // --- Set proper collision layer ---
        if (rb.motion == Motion::Static || rb.motion == Motion::Kinematic)
            col.layer = Layers::NON_MOVING;
        else
            col.layer = Layers::MOVING;

        // --- Always create body for each entity (use entityBodyMap for tracking) ---
        {

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

            //// IMPORTANT: Also enable CCD for kinematic bodies if they move fast
            //if (motionType == JPH::EMotionType::Kinematic)
            //    bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;

            
            if (motionType == JPH::EMotionType::Kinematic)
            {
                bcs.mCollideKinematicVsNonDynamic = true;
                bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
            }


            // --- Apply damping and restitution ---
            bcs.mRestitution = 0.2f;
            bcs.mFriction = 0.5f;
            bcs.mLinearDamping = rb.linearDamping;
            bcs.mAngularDamping = rb.angularDamping;

            // --- Apply gravity factor ---
            bcs.mGravityFactor = rb.gravityFactor;

            // Create and add the body to the physics system
            JPH::BodyID newBodyId = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);

            // Store body ID in our per-entity map (avoids shared component bug)
            entityBodyMap[e] = newBodyId;
            bodyToEntityMap[newBodyId] = e;

            // Also store in component for Update/SyncBack compatibility
            rb.id = newBodyId;

            // Update bookkeeping
            rb.collider_seen_version = col.version;
            rb.transform_dirty = rb.motion_dirty = false;

            // Limit angular velocity to avoid instability
            bi.SetMaxAngularVelocity(newBodyId, 2.0f);

#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "GAM300",
                "[Physics] Created body id=%u, motion=%d, layer=%d, CCD=%d, pos=(%f,%f,%f)",
                rb.id.GetIndex(), static_cast<int>(motionType), col.layer, rb.ccd,
                pos.GetX(), pos.GetY(), pos.GetZ());
#endif
        }
    }
}


//KINEMATIC: NOT AFFECTED BY GRAVITY, FORCES, IMPULSES, OTHER BODIES MOVING IT.
//MOVE MANUALLY VIA POS, ROTATION E.T.C


//DYNAMIC: USE PHYSICS SIMULATION. IF ANY CHANGES TO BE MADE, ADJUST VIA FORCES, NOT POS


void PhysicsSystem::Update(float fixedDt, ECSManager& ecsManager) {
    PROFILE_FUNCTION();
#ifdef __ANDROID__
    static int updateCount = 0;
    if (updateCount++ % 60 == 0) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Update called, fixedDt=%f, entities=%zu", fixedDt, entities.size());
    }
#endif
    if (entities.empty()) return;

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // ========== UPDATE KINEMATIC BODIES BEFORE PHYSICS STEP ==========
    for (auto& e : entities) {
        // Skip entities without a body in our map
        auto bodyIt = entityBodyMap.find(e);
        if (bodyIt == entityBodyMap.end() || bodyIt->second.IsInvalid()) continue;
        JPH::BodyID bodyId = bodyIt->second;

        if (!ecsManager.HasComponent<RigidBodyComponent>(e)) continue;
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        auto& tr = ecsManager.GetComponent<Transform>(e);
        

        if (rb.motion == Motion::Kinematic) {
            JPH::RVec3 targetPos(tr.localPosition.x, tr.localPosition.y, tr.localPosition.z);
            JPH::Quat targetRot = JPH::Quat(tr.localRotation.x, tr.localRotation.y,
                tr.localRotation.z, tr.localRotation.w).Normalized();

            // Get current position and rotation
            JPH::RVec3 currentPos = bi.GetPosition(bodyId);
            JPH::Quat currentRot = bi.GetRotation(bodyId);

            // Calculate linear velocity from position delta (CRITICAL for collision detection!)
            JPH::Vec3 linearVel = (targetPos - currentPos) / fixedDt;

            // Calculate angular velocity from rotation delta
            JPH::Quat deltaRot = targetRot * currentRot.Conjugated();

            // Extract axis and angle using GetAxisAngle
            JPH::Vec3 axis;
            float angle;
            deltaRot.GetAxisAngle(axis, angle);

            // Angular velocity = axis * (angle / deltaTime)
            JPH::Vec3 angularVel = axis * (angle / fixedDt);

            // Set velocities BEFORE moving (Jolt uses this for collision detection)
            bi.SetLinearVelocity(bodyId, linearVel);
            bi.SetAngularVelocity(bodyId, angularVel);

            // Now move the kinematic body
            bi.MoveKinematic(bodyId, targetPos, targetRot, fixedDt);

            // Ensure body stays active for collision detection
            bi.ActivateBody(bodyId);

            rb.transform_dirty = false;

                

#ifdef __ANDROID__
            if (updateCount % 60 == 0) {
                __android_log_print(ANDROID_LOG_INFO, "GAM300",
                    "[Physics] MoveKinematic body %u to (%f, %f, %f) with dt=%f",
                    rb.id.GetIndex(), pos.GetX(), pos.GetY(), pos.GetZ(), fixedDt);
            }
#endif
        }
    }

    // ========== SYNC ECS -> JOLT (for dynamic bodies) ==========
    for (auto& e : entities) {
        // Skip entities without a body in our map
        auto bodyIt = entityBodyMap.find(e);
        if (bodyIt == entityBodyMap.end() || bodyIt->second.IsInvalid()) continue;
        JPH::BodyID bodyId = bodyIt->second;

        if (!ecsManager.HasComponent<RigidBodyComponent>(e)) continue;
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        bi.SetGravityFactor(bodyId, rb.gravityFactor);
        bi.SetIsSensor(bodyId, rb.isTrigger);

        //// Read back velocities from physics engine
        //rb.angularVel = FromJoltVec3(bi.GetAngularVelocity(body Id));
        //rb.linearVel = FromJoltVec3(bi.GetLinearVelocity(bodyId));

        if (rb.motion == Motion::Dynamic)
        {
            // Only apply velocity if it's non-zero, but should never touch this directly in script.
            if (rb.linearVel.x != 0.0f || rb.linearVel.y != 0.0f || rb.linearVel.z != 0.0f) {
                bi.SetLinearVelocity(bodyId, ToJoltVec3(rb.linearVel));
                rb.linearVel = Vector3D(0, 0, 0); // Reset after applying
            }

            if (rb.angularVel.x != 0.0f || rb.angularVel.y != 0.0f || rb.angularVel.z != 0.0f) {
                bi.SetAngularVelocity(bodyId, ToJoltVec3(rb.angularVel));
                rb.angularVel = Vector3D(0, 0, 0); // Reset after applying
            }

            if (rb.forceApplied.x != 0.0f || rb.forceApplied.y != 0.0f || rb.forceApplied.z != 0.0f)
            {
                bi.AddForce(bodyId, ToJoltVec3(rb.forceApplied));
                rb.forceApplied = Vector3D(0.0f, 0.0f, 0.0f);   //reset back to 0
            }
            if (rb.torqueApplied.x != 0.0f || rb.torqueApplied.y != 0.0f || rb.torqueApplied.z != 0.0f)
            {
                bi.AddTorque(bodyId, ToJoltVec3(rb.torqueApplied));
                rb.torqueApplied = Vector3D(0.0f, 0.0f, 0.0f);
            }
            
            if (rb.impulseApplied.x != 0.0f || rb.impulseApplied.y != 0.0f || rb.impulseApplied.z != 0.0f)
            {
                bi.AddImpulse(bodyId, ToJoltVec3(rb.impulseApplied));
                rb.impulseApplied = Vector3D(0.0f, 0.0f, 0.0f);
            }

        }
    }

    // ========== RUN PHYSICS SIMULATION ==========
    physics.Update(fixedDt, /*collisionSteps=*/4, temp.get(), jobs.get());

    // ========== SYNC JOLT -> ECS (after physics step) ==========
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
        // Skip entities without a body in our map
        auto bodyIt = entityBodyMap.find(e);
        if (bodyIt == entityBodyMap.end() || bodyIt->second.IsInvalid()) continue;
        JPH::BodyID bodyId = bodyIt->second;

        if (!ecsManager.HasComponent<RigidBodyComponent>(e) ||
            !ecsManager.HasComponent<ColliderComponent>(e) ||
            !ecsManager.HasComponent<Transform>(e)) continue;

        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);

        // Only sync Dynamic bodies (physics controls them)
        // Kinematic/Static bodies are controlled by Transform, not physics
        if (rb.motion == Motion::Dynamic) {
            JPH::RVec3 p;
            JPH::Quat r;
            bi.GetPositionAndRotation(bodyId, p, r);

            // WRITE to ECS Transform (so renderer/other systems can see it)
            float offsetY = col.center.y * tr.localScale.y;

            tr.localPosition = Vector3D(p.GetX(), p.GetY() - offsetY, p.GetZ());
            tr.localRotation = Quaternion(r.GetW(), r.GetX(), r.GetY(), r.GetZ());

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
    // 1. Remove and destroy all bodies using entityBodyMap
    auto& bi = physics.GetBodyInterface();
    for (auto& [entity, bodyId] : entityBodyMap) {
        if (!bodyId.IsInvalid() && bi.IsAdded(bodyId)) {
            bi.RemoveBody(bodyId);
            bi.DestroyBody(bodyId);
        }
    }
    entityBodyMap.clear();
    bodyToEntityMap.clear();

    // 2. Drop collider shapes
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    for (auto e : entities) {
        if (!ecs.HasComponent<ColliderComponent>(e)) continue;
        auto& col = ecs.GetComponent<ColliderComponent>(e);
        col.shape = nullptr;
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
        // Ignore sensors and character - only hit solid geometry
        return inLayer != Layers::SENSOR && inLayer != Layers::CHARACTER;
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

    // Use default filters (hit everything) for debugging
    JPH::RayCastResult hit;
    if (query.CastRay(ray, hit)) {
        result.hit = true;
        result.distance = hit.mFraction * maxDistance;

        // Calculate hit point
        JPH::RVec3 hitPos = start + dir * result.distance;
        result.hitPoint = Vector3D(static_cast<float>(hitPos.GetX()),
                                   static_cast<float>(hitPos.GetY()),
                                   static_cast<float>(hitPos.GetZ()));
    }

    return result;
}
