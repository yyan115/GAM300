#pragma once
#include "Math/Matrix4x4.hpp"

struct Transform {
	Vector3D position = { 0, 0, 0 };
	Vector3D scale = { 1, 1, 1 };
	Vector3D rotation = { 0, 0, 0 };

	Vector3D lastPosition = { 0, 0, 0 };
	Vector3D lastScale = { 0, 0, 0 };
	Vector3D lastRotation = { 0, 0, 0 };

	Matrix4x4 model{};

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
