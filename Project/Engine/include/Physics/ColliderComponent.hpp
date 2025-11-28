#pragma once
#include "Physics/JoltInclude.hpp"
#include "pch.h"
#include "Math/Vector3D.hpp"
#include "Reflection/ReflectionBase.hpp"

enum class ColliderShapeType : int {
	Box = 0,
	Sphere,
	Capsule,
	Cylinder
};

struct ColliderComponent {
	REFL_SERIALIZABLE
	bool enabled = true;          // Component enabled state (can be toggled in inspector)
	int layerID = 0;
	uint32_t version = 0;         // bump when you swap shape/layer
	int shapeTypeID = 0;
	Vector3D boxHalfExtents = Vector3D(0.5f, 0.5f, 0.5f);  // For Box

	// Metadata for Inspector editing (non-serialized, reconstructed at runtime)
	JPH::ObjectLayer layer = 0;
	ColliderShapeType shapeType = ColliderShapeType::Box;
	float sphereRadius = 0.5f;                              // For Sphere
	float capsuleRadius = 0.5f;                             // For Capsule
	float capsuleHalfHeight = 0.5f;                         // For Capsule
	float cylinderRadius = 0.5f;                            // For Cylinder
	float cylinderHalfHeight = 0.5f;                        // For Cylinder
	JPH::RefConst<JPH::Shape> shape;

	ColliderComponent() = default;
	~ColliderComponent() = default;

	void SetEnabled(bool e) { enabled = e; }
	bool IsEnabled() const { return enabled; }
};