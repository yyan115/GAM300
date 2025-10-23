#pragma once
#include "Physics/JoltInclude.hpp"
#include "Reflection/ReflectionBase.hpp"

enum class Motion : int { 
	Static = 0, 
	Kinematic, 
	Dynamic 
};

struct RigidBodyComponent {
	REFL_SERIALIZABLE
	int motionID;
	bool ccd = false; // continuous collision detection
	float gravityFactor = 1.0f;

	Motion motion{};
	bool transform_dirty = false;     // set by gameplay when you edit Transform of kinematic/static
	bool motion_dirty = false;        // if you change Motion, flip this to trigger recreate
	uint32_t collider_seen_version = 0; // last applied Collider::version
	JPH::BodyID id = JPH::BodyID();

	RigidBodyComponent() = default;
	~RigidBodyComponent() = default;
};