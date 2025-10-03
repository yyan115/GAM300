#pragma once
#include "pch.h"
#include "ECS/System.hpp"
#include "ECS/ECSRegistry.hpp"
//#include "Physics/JoltInclude.hpp"

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

static bool JoltAssertFailed(const char* expr, const char* msg, const char* file, JPH::uint line)
{
    fprintf(stderr, "[Jolt Assert] %s : %s (%s:%u)\n", expr, msg ? msg : "", file, (unsigned)line);
    return false;
}



bool PhysicsSystem::Initialise() {
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
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Jolt] JPH_OBJECT_LAYER_BITS=%d",
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

    // IMPORTANT: pass your persistent members (not locals) to Init
    physics.Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
        broadphase, objVsBP, objPair);
    physics.SetGravity(JPH::Vec3(0, -9.81f, 0));  // gravity set

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

    return true;
}



void PhysicsSystem::Update(float dt) {
#ifdef __ANDROID__
	static int updateCount = 0;
	if (updateCount++ % 60 == 0) { // Log every 60 frames
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Update called, dt=%f, entities=%zu", dt, entities.size());
	}
#endif

	physics.Update(dt, /*collisionSteps=*/4, temp.get(), jobs.get()); // Increased collision steps for better response


	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	//GraphicsManager& gfxManager = GraphicsManager::GetInstance();

	// Submit all related physics components in
	for (const auto& entity : entities)
	{
		auto& rigidBodyComponent = ecsManager.GetComponent<RigidBodyComponent>(entity);
		auto& transformComponent = ecsManager.GetComponent<Transform>(entity);
		auto& colliderComponent = ecsManager.GetComponent<ColliderComponent>(entity);


	}
}

void PhysicsSystem::physicsAuthoring(ECSManager& ecsManager) {
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] physicsAuthoring called, entities=%zu", entities.size());
#endif

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    for (auto &e : entities) {
        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

		JPH::RVec3Arg pos = JPH::RVec3(tr.localPosition.x, tr.localPosition.y, tr.localPosition.z);
        JPH::QuatArg rot = JPH::Quat::sIdentity();
        JPH_ASSERT(rot.IsNormalized());  // will catch accidents early

        // 1) Create if not created yet
        if (rb.id.IsInvalid()) {
            const auto motion =
                rb.motion == Motion::Static ? JPH::EMotionType::Static :
                rb.motion == Motion::Kinematic ? JPH::EMotionType::Kinematic :
                JPH::EMotionType::Dynamic;
            JPH_ASSERT(col.shape != nullptr);                     //  catch missing shapes early

            JPH::BodyCreationSettings bcs(col.shape.GetPtr(), pos, rot, motion, col.layer);
            if (rb.ccd) bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
            
            // Set physics material properties for bouncing
            bcs.mRestitution = 0.8f;        // High bounciness
            bcs.mFriction = 0.3f;           // Moderate friction
            bcs.mLinearDamping = 0.01f;     // Very low linear damping
            bcs.mAngularDamping = 0.01f;    // Very low angular damping

            rb.id = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);
            rb.collider_seen_version = col.version;
            rb.transform_dirty = rb.motion_dirty = false;

#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Created body: id=%u, motion=%d, pos=(%f,%f,%f)",
                rb.id.GetIndex(), (int)motion, pos.GetX(), pos.GetY(), pos.GetZ());
            // Verify body is actually active
            if (motion == JPH::EMotionType::Dynamic) {
                bool isActive = bi.IsActive(rb.id);
                __android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Dynamic body active: %d", isActive);
            }
#endif
            continue; // done for this entity
        }

        // 2) Recreate if collider or motion changed (cheap guard)
        if (rb.motion_dirty || rb.collider_seen_version != col.version) {
            // remove, destroy, recreate (schemas usually don�t change frequently)
            bi.RemoveBody(rb.id);
            bi.DestroyBody(rb.id);
            rb.id = JPH::BodyID(); // mark invalid; next loop creates fresh
            continue;
        }

        // 3) Push transforms for kinematics / edited statics
        if (rb.motion == Motion::Kinematic && rb.transform_dirty) {
            bi.MoveKinematic(rb.id, pos, rot, /*dt*/ 0.0f /* or pass dt if you have it here */);
            rb.transform_dirty = false;
        }
        else if (rb.motion == Motion::Static && rb.transform_dirty) {
            bi.SetPositionAndRotation(rb.id, pos, rot, JPH::EActivation::DontActivate);
            rb.transform_dirty = false;
        }
    }
}

void PhysicsSystem::physicsSyncBack(ECSManager& ecsManager) {
	auto& bi = physics.GetBodyInterface();

#ifdef __ANDROID__
	static int syncCount = 0;
	if (syncCount++ % 60 == 0) { // Log every 60 frames
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] physicsSyncBack called, entities=%zu", entities.size());
	}
#endif

    for (auto &e : entities) {
        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        JPH::RVec3 pos = JPH::RVec3(tr.localPosition.x, tr.localPosition.y, tr.localPosition.z);
        JPH::QuatArg rot = JPH::Quat::sIdentity();
        JPH_ASSERT(rot.IsNormalized());  // will catch accidents early

        if (rb.id.IsInvalid()) continue;
        if (rb.motion == Motion::Dynamic) {
            JPH::RVec3 p; JPH::Quat r;
            bi.GetPositionAndRotation(rb.id, p, r);
			tr.localPosition = Vector3D(p.GetX(), p.GetY(), p.GetZ());
			tr.localRotation = Quaternion(r.GetX(), r.GetY(), r.GetZ(), r.GetW());
            tr.isDirty = true;

#ifdef __ANDROID__
			// Debug: Log position for one dynamic body every 60 frames
			if (syncCount % 60 == 0) {
				__android_log_print(ANDROID_LOG_INFO, "GAM300", "[Physics] Dynamic body pos: (%f, %f, %f)",
					p.GetX(), p.GetY(), p.GetZ());
			}
#endif
        }
    }
}

void PhysicsSystem::Shutdown() {
	//JPH::UnregisterTypes();
    JPH::BodyInterface& bi = physics.GetBodyInterface();
	for (auto& e : entities) {
		auto& rb = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<RigidBodyComponent>(e);
		if (!rb.id.IsInvalid()) {
			bi.RemoveBody(rb.id);
			bi.DestroyBody(rb.id);
			rb.id = JPH::BodyID();
		}
	}
    for (auto& e : entities) {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        auto& col = ecs.GetComponent<ColliderComponent>(e);
        col.shape = nullptr;    // drop RefConst<Shape>
    }
    entities.clear();

    jobs.reset();
    temp.reset();
    /*delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;*/
}
