#pragma once
#include "Physics/JoltInclude.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "Math/Vector3D.hpp"

enum class Motion : int { 
	Static = 0, 
	Kinematic, 
	Dynamic 
};

struct PhysicsMaterial {
	std::string name = "Default";
	float friction = 0.6f;
};



struct RigidBodyComponent {
	REFL_SERIALIZABLE
	bool enabled = true;          // Component enabled state (can be toggled in inspector)
	int motionID;

	bool ccd		= false; // continuous collision detection
	bool isTrigger	= false;

	float gravityFactor = 1.0f;

	Vector3D angularVel = { 0.0f,0.0f,0.0f };
	Vector3D linearVel = { 0.0f, 0.0f,0.0f };

	float linearDamping = 0.0f;
	float angularDamping = 0.95f;



	Motion motion{};
	bool transform_dirty = false;     // set by gameplay when you edit Transform of kinematic/static
	bool motion_dirty = false;        // if you change Motion, flip this to trigger recreate
	uint32_t collider_seen_version = 0; // last applied Collider::version
	JPH::BodyID id = JPH::BodyID();

	RigidBodyComponent() = default;
	~RigidBodyComponent() = default;
};