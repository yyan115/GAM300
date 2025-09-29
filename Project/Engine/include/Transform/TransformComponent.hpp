#pragma once
#include "Math/Matrix4x4.hpp"
#include "Quaternion.hpp"

struct Transform {
	REFL_SERIALIZABLE
	//Vector3D worldPosition = { 0, 0, 0 };
	//Vector3D worldScale = { 0, 0, 0 };
	//Vector3D worldRotation = { 0, 0, 0 };

	Vector3D localPosition;
	Vector3D localScale;
	//Vector3D editorEuler;
	Quaternion localRotation;

	//Vector3D lastPosition = { 0, 0, 0 };
	//Vector3D lastScale = { 0, 0, 0 };
	//Vector3D lastRotation = { 0, 0, 0 };
	bool isDirty = true;
	
	Matrix4x4 worldMatrix{};

	Transform() = default;
	~Transform() = default;
};

