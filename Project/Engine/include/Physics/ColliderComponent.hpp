#pragma once
#include "Physics/JoltInclude.hpp"
#include "pch.h"
struct ColliderComponent {
	JPH::RefConst<JPH::Shape> shape;
	JPH::ObjectLayer layer;
	uint32_t version = 0;         // bump when you swap shape/layer

	ColliderComponent() = default;
	~ColliderComponent() = default;
};