
class ECSManager;

#include "pch.h"
#include "Physics/JoltInclude.hpp"
#include "ECS/System.hpp"
#include "Physics/CollisionFilters.hpp"
#include "Math/Vector3D.hpp"
#include "Physics/PhysicsContactListener.hpp"

#include "../Engine.h"  // For ENGINE_API macro

class PhysicsSystem : public System {
public:
	PhysicsSystem() = default;
	~PhysicsSystem() = default;

	bool InitialiseJolt();
	void Initialise(ECSManager& ecsManager);

	void Update(float fixedDt, ECSManager& ecsManager);	//SIMULATE PHYSICS e.g APPLY FORCES e.t.c
	//void SyncDirtyComponents(ECSManager& ecsManager);	//APPLY INSPECTOR CHANGES TO JOLT
	void PhysicsSyncBack(ECSManager& ecsManager);	//JOLT -> ECS
	void Shutdown();


	MyBroadPhaseLayerInterface broadphase;
	MyObjectVsBroadPhaseLayerFilter objVsBP;
	MyObjectLayerPairFilter objPair;

private:
	JPH::PhysicsSystem          physics;
	std::unordered_map<JPH::BodyID, int> bodyToEntityMap;	//to reference physics id -> entity id (for logging)
	std::unique_ptr<MyContactListener> contactListener;
	std::unique_ptr<JPH::JobSystem> jobs;           // e.g., JobSystemThreadPool
	std::unique_ptr<JPH::TempAllocator> temp;       // e.g., TempAllocatorImpl

};