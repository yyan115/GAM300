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
    // Jolt one-time bootstrap (no RegisterTypes needed for this smoke test)
    if (JPH::Factory::sInstance == nullptr) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = JoltTrace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed; )
            JPH::Factory::sInstance = new JPH::Factory();
    }

    if (!temp) temp = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    if (!jobs) jobs = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::max(1u, std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() - 1 : 1u));

    // IMPORTANT: pass your persistent members (not locals) to Init
    physics.Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
        broadphase, objVsBP, objPair);  
    physics.SetGravity(JPH::Vec3(0, -9.81f, 0));  // gravity set

    JPH::BodyInterface& bi = physics.GetBodyInterface();

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
    bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);

    return true;
}



void PhysicsSystem::Update(float dt) {
	physics.Update(dt, /*collisionSteps=*/1, temp.get(), jobs.get());


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

    JPH::BodyInterface& bi = physics.GetBodyInterface();

    for (auto &e : entities) {
        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

		JPH::RVec3Arg pos = JPH::RVec3(tr.position.x, tr.position.y, tr.position.z);
		JPH::QuatArg rot = JPH::Quat(tr.rotation.x, tr.rotation.y, tr.rotation.z, 0);
        // 1) Create if not created yet
        if (rb.id.IsInvalid()) {
            const auto motion =
                rb.motion == Motion::Static ? JPH::EMotionType::Static :
                rb.motion == Motion::Kinematic ? JPH::EMotionType::Kinematic :
                JPH::EMotionType::Dynamic;

            JPH::BodyCreationSettings bcs(col.shape.GetPtr(), pos, rot, motion, col.layer);
            if (rb.ccd) bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;

            rb.id = bi.CreateAndAddBody(bcs, JPH::EActivation::Activate);
            rb.collider_seen_version = col.version;
            rb.transform_dirty = rb.motion_dirty = false;
            continue; // done for this entity
        }

        // 2) Recreate if collider or motion changed (cheap guard)
        if (rb.motion_dirty || rb.collider_seen_version != col.version) {
            // remove, destroy, recreate (schemas usually don’t change frequently)
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

    for (auto &e : entities) {
        auto& tr = ecsManager.GetComponent<Transform>(e);
        auto& col = ecsManager.GetComponent<ColliderComponent>(e);
        auto& rb = ecsManager.GetComponent<RigidBodyComponent>(e);

        JPH::RVec3 pos = JPH::RVec3(tr.position.x, tr.position.y, tr.position.z);
        JPH::Quat rot = JPH::Quat(tr.rotation.x, tr.rotation.y, tr.rotation.z, 0);

        if (rb.id.IsInvalid()) continue;
        if (rb.motion == Motion::Dynamic) {
            JPH::RVec3 p; JPH::Quat r;
            bi.GetPositionAndRotation(rb.id, p, r);
			tr.position = Vector3D(p.GetX(), p.GetY(), p.GetZ());
			tr.rotation = Vector3D(r.GetX(), r.GetY(), r.GetZ());
        }
    }
}

void PhysicsSystem::Shutdown() {
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}
