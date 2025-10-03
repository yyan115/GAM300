#pragma once
#include "Physics/JoltInclude.hpp"
#include "pch.h"
#include "Math/Vector3D.hpp"

enum class ColliderShapeType {
	Box,
	Sphere,
	Capsule,
	Cylinder
};

struct ColliderComponent {
	JPH::RefConst<JPH::Shape> shape;
	JPH::ObjectLayer layer = 0;
	uint32_t version = 0;         // bump when you swap shape/layer

	// Metadata for Inspector editing (non-serialized, reconstructed at runtime)
	ColliderShapeType shapeType = ColliderShapeType::Box;
	Vector3D boxHalfExtents = Vector3D(0.5f, 0.5f, 0.5f);  // For Box
	float sphereRadius = 0.5f;                              // For Sphere
	float capsuleRadius = 0.5f;                             // For Capsule
	float capsuleHalfHeight = 0.5f;                         // For Capsule
	float cylinderRadius = 0.5f;                            // For Cylinder
	float cylinderHalfHeight = 0.5f;                        // For Cylinder

	ColliderComponent() = default;
	~ColliderComponent() = default;
};