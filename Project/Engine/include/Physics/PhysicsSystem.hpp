
class ECSManager;

#include "pch.h"
#include "Physics/JoltInclude.hpp"
#include "ECS/System.hpp"
#include "Physics/CollisionFilters.hpp"
#include "../Engine.h"  // For ENGINE_API macro

class PhysicsSystem : public System {
public:
	PhysicsSystem() = default;
	~PhysicsSystem() = default;

	bool Initialise();

	void Update(float dt);

	void physicsAuthoring(ECSManager& ecsManager);
	void physicsSyncBack(ECSManager& ecsManager);
	void Shutdown();

	MyBroadPhaseLayerInterface broadphase;
	MyObjectVsBroadPhaseLayerFilter objVsBP;
	MyObjectLayerPairFilter objPair;

private:
	JPH::PhysicsSystem          physics;
	std::unique_ptr<JPH::JobSystem> jobs;           // e.g., JobSystemThreadPool
	std::unique_ptr<JPH::TempAllocator> temp;       // e.g., TempAllocatorImpl
	

};