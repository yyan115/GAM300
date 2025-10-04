#pragma once
#include "pch.h"
#include "Physics/JoltInclude.hpp"

enum class Motion { Static, Kinematic, Dynamic };

struct RigidBodyComponent {
	Motion motion{};
	JPH::BodyID id = JPH::BodyID();
	bool ccd = false; // continuous collision detection
	bool transform_dirty = false;     // set by gameplay when you edit Transform of kinematic/static
	bool motion_dirty = false;        // if you change Motion, flip this to trigger recreate
	uint32_t collider_seen_version = 0; // last applied Collider::version

	RigidBodyComponent() = default;
	~RigidBodyComponent() = default;
};