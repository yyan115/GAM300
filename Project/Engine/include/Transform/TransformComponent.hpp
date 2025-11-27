#pragma once
#include "Math/Matrix4x4.hpp"
#include "Quaternion.hpp"

struct Transform {
	REFL_SERIALIZABLE
	//Vector3D worldPosition = { 0, 0, 0 };
	//Vector3D worldScale = { 0, 0, 0 };
	//Vector3D worldRotation = { 0, 0, 0 };

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

	Transform() = default;
	~Transform() = default;
};

