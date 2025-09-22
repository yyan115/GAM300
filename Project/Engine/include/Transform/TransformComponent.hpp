#pragma once
#include "Math/Matrix4x4.hpp"

struct Transform {
	//Vector3D worldPosition = { 0, 0, 0 };
	//Vector3D worldScale = { 0, 0, 0 };
	//Vector3D worldRotation = { 0, 0, 0 };

	Vector3D localPosition;
	Vector3D localScale;
	Vector3D localRotation;

	//Vector3D lastPosition = { 0, 0, 0 };
	//Vector3D lastScale = { 0, 0, 0 };
	//Vector3D lastRotation = { 0, 0, 0 };
	bool isDirty = true;
	
	Matrix4x4 worldMatrix{};

	Transform() = default;
	~Transform() = default;
};

//#pragma region Reflection
//REFL_REGISTER_START(Transform)
//	REFL_REGISTER_PROPERTY(position)
//	REFL_REGISTER_PROPERTY(scale)
//	REFL_REGISTER_PROPERTY(rotation)
//	REFL_REGISTER_PROPERTY(lastPosition)
//	REFL_REGISTER_PROPERTY(lastScale)
//	REFL_REGISTER_PROPERTY(lastRotation)
//	REFL_REGISTER_PROPERTY(model)
//REFL_REGISTER_END;
//#pragma endregion
