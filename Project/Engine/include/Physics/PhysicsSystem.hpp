/*********************************************************************************
* @File			PhysicsSystem.hpp
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
class ECSManager;

#include "pch.h"
#include "Physics/JoltInclude.hpp"
#include "ECS/System.hpp"
#include "Physics/CollisionFilters.hpp"
#include "Math/Vector3D.hpp"
#include "Physics/PhysicsContactListener.hpp"
#include "ECS/LayerComponent.hpp"
#include "Physics/ColliderComponent.hpp"

#include "../Engine.h"  // For ENGINE_API macro

class PhysicsSystem : public System {
public:
	PhysicsSystem() : m_joltInitialized(false) {}
	~PhysicsSystem() = default;

	bool InitialiseJolt();
	void Initialise(ECSManager& ecsManager);
	void PostInitialize(ECSManager& ecsManager);

	void Update(float fixedDt, ECSManager& ecsManager);	//SIMULATE PHYSICS e.g APPLY FORCES e.t.c
	void EditorUpdate(ECSManager& ecs); // CALLED ONLY WHEN THE EDITOR IS IN STOP MODE.
	//void SyncDirtyComponents(ECSManager& ecsManager);	//APPLY INSPECTOR CHANGES TO JOLT
	void PhysicsSyncBack(ECSManager& ecsManager);	//JOLT -> ECS
	void Shutdown();

	JPH::PhysicsSystem& GetJoltSystem() { return physics; }

	// Raycasting for camera collision, line-of-sight checks, etc.
	// Returns: hit distance (negative if no hit), and optionally fills hitPoint
	struct RaycastResult {
		bool hit = false;
		float distance = -1.0f;
		Vector3D hitPoint = Vector3D(0, 0, 0);
		Vector3D hitNormal = Vector3D(0, 0, 0);

		JPH::BodyID bodyId = JPH::BodyID();
	};
	RaycastResult Raycast(const Vector3D& origin, const Vector3D& direction, float maxDistance);
	/*RaycastResult RaycastGroundOnly(const Vector3D& origin, float maxDistance);
	RaycastResult RaycastGroundIgnoreObstacles(const Vector3D& origin,
		const Vector3D& direction,
		float maxDistance);*/
	RaycastResult RaycastGround(
		const Vector3D& origin,
		const Vector3D& dir,
		float dist,
		ECSManager& ecs,
		int groundIdx,
		int obstacleIdx,
		bool acceptObstacleAsHit,
		bool debugLog = false
	);

	MyBroadPhaseLayerInterface broadphase;
	MyObjectVsBroadPhaseLayerFilter objVsBP;
	MyObjectLayerPairFilter objPair;

	struct OverlapResult {
		bool hit = false;
	};

	// test if a capsule overlaps solid world at the given position
	OverlapResult OverlapCapsule(
		const Vector3D& center,
		float halfHeight,
		float radius
	);

	bool BodyIsInECSLayer(const JPH::BodyID& body, int layerIndex) const;

	bool BodyIsObstacle(const JPH::BodyID& body) const;

	bool OverlapCapsuleObstacleLayer(
		const Vector3D& center,
		float halfHeight,
		float radius,
		ECSManager& ecsManager,
		int obstacleLayerIndex
	);

	Entity GetEntityFromBody(const JPH::BodyID& id) const;

	bool IsJoltInitialized() const { return m_joltInitialized; }

	bool GetBodyWorldAABB(Entity e, JPH::AABox& outAABB) const;

	bool GetOverlappingEntities(Entity entity, std::vector<Entity>& out);
private:
	JPH::PhysicsSystem          physics;
	std::unordered_map<JPH::BodyID, int> bodyToEntityMap;	//to reference physics id -> entity id (for logging)
	std::unordered_map<int, JPH::BodyID> entityBodyMap;     // Track body ID per entity (workaround for shared component bug)
	std::unique_ptr<MyContactListener> contactListener;
	std::unique_ptr<JPH::JobSystem> jobs;           // e.g., JobSystemThreadPool
	std::unique_ptr<JPH::TempAllocator> temp;       // e.g., TempAllocatorImpl
	bool m_joltInitialized;  // Track if THIS instance's physics has been initialized

	void SyncPhysicsBodyToTransform(Entity entity, ECSManager& ecs);
	void UpdateColliderShapeScale(ColliderComponent& col, Vector3D worldScale);
};