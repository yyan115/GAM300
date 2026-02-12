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
#include "Performance/PerformanceProfiler.hpp"

#include "Physics/PhysicsSystem.hpp"
#include "Physics/CollisionFilters.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "Transform/TransformComponent.hpp"
#include <cstdarg>

#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CollideShape.h>   // <-- gives CollideShapeSettings
#include <Jolt/Physics/Body/BodyFilter.h>          // BodyFilter
#include <Jolt/Physics/Collision/ShapeFilter.h>    // ShapeFilter
#include "Game AI/NavSystem.hpp"


#ifdef __ANDROID__
#include <android/log.h>
#endif
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
        settings.mNumVelocitySteps = 6;       // 6 is sufficient for action game (default 10 is for heavy sims)
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

    if (ecsManager.characterControllerSystem)
        ecsManager.characterControllerSystem->Shutdown();

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // Remove previously created bodies
    for (auto& [entity, bodyId] : entityBodyMap) {
        if (!bodyId.IsInvalid() && bi.IsAdded(bodyId)) {
            bi.RemoveBody(bodyId);
            bi.DestroyBody(bodyId);
        }
    }
    entityBodyMap.clear();
    bodyToEntityMap.clear();

    for (auto& e : entities) {
        if (!ecsManager.HasComponent<RigidBodyComponent>(e) ||
            !ecsManager.HasComponent<ColliderComponent>(e) ||
            !ecsManager.HasComponent<Transform>(e)) {
            continue;
        }

        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        rb.motion = static_cast<Motion>(rb.motionID);

        // =========================================================
        // FIX: MATCH SCENE PANEL GIZMO LOGIC
        // Instead of using tr.worldRotation/Scale (which might be stale),
        // we extract the authoritative data directly from the WorldMatrix.
        // =========================================================

        // 1. Extract Raw Axes (Columns) from the Matrix
        // This captures Rotation AND Scale AND Parent transforms exactly as the renderer sees them.
        JPH::Vec3 axisX(tr.worldMatrix.m.m00, tr.worldMatrix.m.m10, tr.worldMatrix.m.m20);
        JPH::Vec3 axisY(tr.worldMatrix.m.m01, tr.worldMatrix.m.m11, tr.worldMatrix.m.m21);
        JPH::Vec3 axisZ(tr.worldMatrix.m.m02, tr.worldMatrix.m.m12, tr.worldMatrix.m.m22);

        // 2. Extract Scale (Magnitude of the axes)
        float sx = axisX.Length();
        float sy = axisY.Length();
        float sz = axisZ.Length();

        // 3. Extract Rotation (Normalize the axes to get pure rotation)
        // We handle small scales to prevent divide-by-zero
        JPH::Vec3 normX = (sx > 0.0001f) ? axisX / sx : JPH::Vec3(1, 0, 0);
        JPH::Vec3 normY = (sy > 0.0001f) ? axisY / sy : JPH::Vec3(0, 1, 0);
        JPH::Vec3 normZ = (sz > 0.0001f) ? axisZ / sz : JPH::Vec3(0, 0, 1);

        JPH::Mat44 rotationMatrix = JPH::Mat44::sIdentity();
        rotationMatrix.SetColumn3(0, normX);
        rotationMatrix.SetColumn3(1, normY);
        rotationMatrix.SetColumn3(2, normZ);
        JPH::Quat rot = rotationMatrix.GetRotation().GetQuaternion();

        // 4. Calculate Center Position
        // Gizmo Logic: WorldMatrix * LocalCenter
        // Math: WorldPos + (AxisX * cx) + (AxisY * cy) + (AxisZ * cz)
        // Note: We use the RAW un-normalized axes here because they contain the scale.
        JPH::Vec3 centerOffset = (axisX * col.center.x) + (axisY * col.center.y) + (axisZ * col.center.z);

        // Final World Position for the Physics Body
        JPH::RVec3 updatedPos = JPH::RVec3(tr.worldPosition.x, tr.worldPosition.y, tr.worldPosition.z) + centerOffset;


        // ... Layer Logic ...
        int ecsLayerIndex = -1;
        if (ecsManager.HasComponent<LayerComponent>(e))
            ecsLayerIndex = ecsManager.GetComponent<LayerComponent>(e).layerIndex;

        const int groundIdx = LayerManager::GetInstance().GetLayerIndex("Ground");
        const int obstacleIdx = LayerManager::GetInstance().GetLayerIndex("Obstacle");

        if (ecsLayerIndex == groundIdx) col.layer = Layers::NAV_GROUND;
        else if (ecsLayerIndex == obstacleIdx) col.layer = Layers::NAV_OBSTACLE;
        else {
            if (rb.motion == Motion::Static) col.layer = Layers::NON_MOVING;
            else col.layer = Layers::MOVING;
        }

        {
            // Create Shape using the EXTRACTED Scale (sx, sy, sz)
            switch (col.shapeType) {
            case ColliderShapeType::Box:
            {
                // Apply extracted scale to half extents
                float hx = std::abs(col.boxHalfExtents.x) * sx;
                float hy = std::abs(col.boxHalfExtents.y) * sy;
                float hz = std::abs(col.boxHalfExtents.z) * sz;

                constexpr float kMinHalf = 0.05f;
                hx = std::max(hx, kMinHalf);
                hy = std::max(hy, kMinHalf);
                hz = std::max(hz, kMinHalf);

                JPH::BoxShapeSettings settings(JPH::Vec3(hx, hy, hz));
                settings.mConvexRadius = 0.0f;
                JPH::Shape::ShapeResult result = settings.Create();

                col.shape = result.IsValid() ? result.Get() : new JPH::BoxShape(JPH::Vec3(hx, hy, hz));
                break;
            }
            case ColliderShapeType::Sphere:
                col.shape = new JPH::SphereShape(col.sphereRadius * std::max({ sx, sy, sz }));
                break;
            case ColliderShapeType::Capsule:
                // Radius scales by X/Z, Height by Y
                col.shape = new JPH::CapsuleShape(col.capsuleHalfHeight * sy, col.capsuleRadius * std::max(sx, sz));
                break;
            case ColliderShapeType::Cylinder:
                col.shape = new JPH::CylinderShape(col.cylinderHalfHeight * sy, col.cylinderRadius * std::max(sx, sz));
                break;
            case ColliderShapeType::MeshShape:
            {
                // Get the model's mesh data
                if (ecsManager.HasComponent<ModelRenderComponent>(e)) {
                    auto& rc = ecsManager.GetComponent<ModelRenderComponent>(e);

                    if (rc.model && rc.model->meshes.size() > 0) {
                        JPH::TriangleList triangles;

                        // Extract triangles from all meshes in the model
                        for (const auto& mesh : rc.model->meshes) {
                            // Assuming your mesh has vertices with positions
                            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                                // Get the three vertices of the triangle
                                const auto& v0 = mesh.vertices[mesh.indices[i]];
                                const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
                                const auto& v2 = mesh.vertices[mesh.indices[i + 2]];

                                float sx = std::abs(tr.localScale.x);
                                float sy = std::abs(tr.localScale.y);
                                float sz = std::abs(tr.localScale.z);

                                // Apply local scale to vertices
                                JPH::Float3 p0(v0.position.x * sx,
                                    v0.position.y * sy,
                                    v0.position.z * sz);
                                JPH::Float3 p1(v1.position.x * sx,
                                    v1.position.y * sy,
                                    v1.position.z * sz);
                                JPH::Float3 p2(v2.position.x * sx,
                                    v2.position.y * sy,
                                    v2.position.z * sz);

                                triangles.push_back(JPH::Triangle(p0, p1, p2));
                            }
                        }

                        // Create the mesh shape
                        JPH::MeshShapeSettings meshSettings(triangles);
                        JPH::Shape::ShapeResult result = meshSettings.Create();

                        if (result.IsValid()) {
                            col.shape = result.Get();
                        }
                        else {
                            // Fallback to box if mesh creation fails
                            col.shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
                        }
                    }
                    else {
                        // Fallback to box if mesh creation fails
                        col.shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
                    }
                }
                else {
                    // Fallback to box if mesh creation fails
                    col.shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
                }
                break;
            }
            }
        }

        const auto motionType =
            rb.motion == Motion::Static ? JPH::EMotionType::Static :
            rb.motion == Motion::Kinematic ? JPH::EMotionType::Kinematic :
            JPH::EMotionType::Dynamic;

        JPH::BodyCreationSettings bcs(col.shape.GetPtr(), updatedPos, rot, motionType, col.layer);

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

        JPH::BodyID newBodyId = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);
        entityBodyMap[e] = newBodyId;
        bodyToEntityMap[newBodyId] = e;
        rb.id = newBodyId;
        rb.collider_seen_version = col.version;
        rb.transform_dirty = rb.motion_dirty = false;
        bi.SetMaxAngularVelocity(newBodyId, 2.0f);
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
#ifdef __ANDROID__
    static int updateCount = 0;
    if (updateCount++ % 60 == 0) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Update called, fixedDt=%f, entities=%zu", fixedDt, entities.size());
    }
#endif
    if (entities.empty()) return;

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    // =========================================================================================
    // 1. MANAGE BODY ACTIVATION STATE (Add/Remove from World)
    //    This ensures that if an entity is disabled in the hierarchy OR the collider is disabled,
    //    it is removed from the physics simulation entirely.
    // =========================================================================================
    for (auto& e : entities) {
        auto bodyIt = entityBodyMap.find(e);
        if (bodyIt == entityBodyMap.end() || bodyIt->second.IsInvalid()) continue;
        JPH::BodyID bodyId = bodyIt->second;

        if (!ecsManager.HasComponent<ColliderComponent>(e)) continue;
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        // Determine if this body SHOULD be in the physics world
        bool shouldBeActive = ecsManager.IsEntityActiveInHierarchy(e) && col.enabled && rb.enabled;
        bool isCurrentlyAdded = bi.IsAdded(bodyId);

        if (shouldBeActive && !isCurrentlyAdded) {
            // Re-add to the world (Wake it up)
            bi.AddBody(bodyId, JPH::EActivation::Activate);
        }
        else if (!shouldBeActive && isCurrentlyAdded) {
            // Remove from the world (Stops all collisions and processing)
            bi.RemoveBody(bodyId);
        }
    }

    // =========================================================================================
    // 2. UPDATE KINEMATIC BODIES BEFORE PHYSICS STEP
    // =========================================================================================
    for (auto& e : entities) {
        if (!ecsManager.IsEntityActiveInHierarchy(e)) continue;

        auto bodyIt = entityBodyMap.find(e);
        if (bodyIt == entityBodyMap.end() || bodyIt->second.IsInvalid()) continue;
        JPH::BodyID bodyId = bodyIt->second;

        // Optimization: If we just removed it in Step 1, don't try to move it
        if (!bi.IsAdded(bodyId)) continue;

        if (!ecsManager.HasComponent<RigidBodyComponent>(e)) continue;
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        if (!rb.enabled) continue;

        auto& tr = ecsManager.GetComponent<Transform>(e);

        if (rb.motion == Motion::Kinematic) {
            auto& col = ecsManager.GetComponent<ColliderComponent>(e);

            // FIX: Use World Scale
            Vector3D scaledOffset = {
                col.center.x * tr.worldScale.x,
                col.center.y * tr.worldScale.y,
                col.center.z * tr.worldScale.z
            };

            // FIX: Use World Rotation
            JPH::Quat targetRot = JPH::Quat(tr.worldRotation.x, tr.worldRotation.y,
                tr.worldRotation.z, tr.worldRotation.w).Normalized();

            // Rotate offset to world space
            JPH::Vec3 offsetInWorld = targetRot * JPH::Vec3(scaledOffset.x, scaledOffset.y, scaledOffset.z);

            // FIX: Use World Position as base
            JPH::RVec3 basePos(tr.worldPosition.x, tr.worldPosition.y, tr.worldPosition.z);
            JPH::RVec3 targetPos = basePos + offsetInWorld;

            // Get current Jolt position (World Space)
            JPH::RVec3 currentPos = bi.GetPosition(bodyId);
            JPH::Quat currentRot = bi.GetRotation(bodyId);

            // Calculate velocities required to reach target
            JPH::Vec3 linearVel = (targetPos - currentPos) / fixedDt;

            // Calculate angular velocity
            JPH::Quat deltaRot = targetRot * currentRot.Conjugated();
            JPH::Vec3 axis;
            float angle;
            deltaRot.GetAxisAngle(axis, angle);
            JPH::Vec3 angularVel = axis * (angle / fixedDt);

            bi.SetLinearVelocity(bodyId, linearVel);
            bi.SetAngularVelocity(bodyId, angularVel);
            bi.MoveKinematic(bodyId, targetPos, targetRot, fixedDt);
            bi.ActivateBody(bodyId);

            rb.transform_dirty = false;
        }
    }

    // =========================================================================================
    // 3. SYNC ECS -> JOLT (for dynamic bodies)
    // =========================================================================================
    for (auto& e : entities) {
        if (!ecsManager.IsEntityActiveInHierarchy(e)) continue;

        auto bodyIt = entityBodyMap.find(e);
        if (bodyIt == entityBodyMap.end() || bodyIt->second.IsInvalid()) continue;
        JPH::BodyID bodyId = bodyIt->second;

        // Skip if not currently in physics world
        if (!bi.IsAdded(bodyId)) continue;

        if (!ecsManager.HasComponent<RigidBodyComponent>(e)) continue;
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);
        if (!rb.enabled) continue;

        bi.SetGravityFactor(bodyId, rb.gravityFactor);
        bi.SetIsSensor(bodyId, rb.isTrigger);

        if (rb.motion == Motion::Dynamic)
        {
            if (rb.linearVel.x != 0.0f || rb.linearVel.y != 0.0f || rb.linearVel.z != 0.0f) {
                bi.SetLinearVelocity(bodyId, ToJoltVec3(rb.linearVel));
                rb.linearVel = Vector3D(0, 0, 0);
            }
            if (rb.angularVel.x != 0.0f || rb.angularVel.y != 0.0f || rb.angularVel.z != 0.0f) {
                bi.SetAngularVelocity(bodyId, ToJoltVec3(rb.angularVel));
                rb.angularVel = Vector3D(0, 0, 0);
            }
            if (rb.forceApplied.x != 0.0f || rb.forceApplied.y != 0.0f || rb.forceApplied.z != 0.0f) {
                bi.AddForce(bodyId, ToJoltVec3(rb.forceApplied));
                rb.forceApplied = Vector3D(0.0f, 0.0f, 0.0f);
            }
            if (rb.torqueApplied.x != 0.0f || rb.torqueApplied.y != 0.0f || rb.torqueApplied.z != 0.0f) {
                bi.AddTorque(bodyId, ToJoltVec3(rb.torqueApplied));
                rb.torqueApplied = Vector3D(0.0f, 0.0f, 0.0f);
            }
            if (rb.impulseApplied.x != 0.0f || rb.impulseApplied.y != 0.0f || rb.impulseApplied.z != 0.0f) {
                bi.AddImpulse(bodyId, ToJoltVec3(rb.impulseApplied));
                rb.impulseApplied = Vector3D(0.0f, 0.0f, 0.0f);
            }
        }
    }

    // ========== RUN PHYSICS SIMULATION ==========
    physics.Update(fixedDt, /*collisionSteps=*/1, temp.get(), jobs.get());

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
    auto& bi = physics.GetBodyInterface();

#ifdef __ANDROID__
    static int syncCount = 0;
    if (syncCount++ % 60 == 0) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300",
            "[Physics] physicsSyncBack called, entities=%zu", entities.size());
    }
#endif

    for (auto& e : entities) {
        if (!ecsManager.IsEntityActiveInHierarchy(e)) continue;

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

        if (!rb.enabled) continue;

        // Only sync Dynamic bodies (physics controls them)
        // Kinematic/Static bodies are controlled by Transform, not physics
        if (rb.motion == Motion::Dynamic) {
            JPH::RVec3 p;
            JPH::Quat r;
            bi.GetPositionAndRotation(bodyId, p, r);

            // WRITE to ECS Transform (so renderer/other systems can see it)
            float offsetY = col.center.y * tr.localScale.y;     //in case meshes pivot start from the bottom instead of center

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

    // 2. Drop 
    //  shapes
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

        result.bodyId = hit.mBodyID;
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
