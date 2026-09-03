#pragma once
#include "Math/Matrix4x4.hpp"
#include "Quaternion.hpp"
#include <cstdint>

struct Transform {
	REFL_SERIALIZABLE
	Vector3D worldPosition = { 0, 0, 0 };
	Vector3D worldScale = { 0, 0, 0 };
	Quaternion worldRotation = {};

	bool overrideFromPrefab = false;
	Vector3D localPosition = {0.0f, 0.0f, 0.0f};
	Vector3D localScale = {1.0f, 1.0f, 1.0f};
	//Vector3D editorEuler;
	Quaternion localRotation = {};

	//Vector3D lastPosition = { 0, 0, 0 };
	//Vector3D lastScale = { 0, 0, 0 };
	//Vector3D lastRotation = { 0, 0, 0 };
	bool isDirty = true;
	
	Matrix4x4 worldMatrix{};
	// Runtime-only generation for systems that cache world-space derived data.
	std::uint64_t worldRevision = 0;

	// Runtime-only local-matrix cache. Hierarchy propagation frequently updates
	// a world matrix because an ancestor moved even though this entity's local
	// TRS did not. Animation can also provide the matrix it already composed.
	// The source values make the cache self-validating even when editor or
	// reflection code writes the public local fields directly.
	Matrix4x4 cachedLocalMatrix{};
	Vector3D cachedLocalPosition{};
	Vector3D cachedLocalScale{};
	Quaternion cachedLocalRotation{};
	bool cachedLocalMatrixValid = false;

	Transform() = default;
	~Transform() = default;
};
