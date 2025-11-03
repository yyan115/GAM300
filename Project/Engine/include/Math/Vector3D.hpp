/*********************************************************************************
* @File			Vector3.hpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			3/9/2025
* @Brief		This is the Declaration of Vector3 Class
* 
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen 
* Institute of Technology is prohibited. 
*********************************************************************************/
#pragma once

#include "pch.h"
#include "Reflection/ReflectionBase.hpp"
#include "glm/vec3.hpp"
#include "Physics/JoltInclude.hpp"

#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
// Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

struct ENGINE_API Vector3D
{
	REFL_SERIALIZABLE

	float x, y, z;

	// Constructs
	constexpr Vector3D() : x(0.f), y(0.f), z(0.f) {}
	constexpr Vector3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

	//  Special helpers
	static constexpr Vector3D Zero() { return { 0.f,0.f,0.f }; }
	static constexpr Vector3D XAxis() { return { 1.f,0.f,0.f }; }
	static constexpr Vector3D YAxis() { return { 0.f,1.f,0.f }; }
	static constexpr Vector3D ZAxis() { return { 0.f,0.f,1.f }; }
	static constexpr Vector3D Ones() { return { 1.f,1.f,1.f }; }

	// Copy Constructor
	Vector3D(const Vector3D&);

	Vector3D& operator=(const Vector3D&);

	// Indexing
	float& operator[](int i);

	const float& operator[](int i) const;

	// Assignment Operator
	constexpr Vector3D operator-() const noexcept { return { -x,-y,-z }; }

	Vector3D operator+(const Vector3D&) const;
	Vector3D operator-(const Vector3D&) const;
	Vector3D operator*(const Vector3D&) const;
	Vector3D operator/(const Vector3D&) const;

	Vector3D operator*(float) const;
	Vector3D operator/(float) const;

	Vector3D& operator+=(const Vector3D&);
	Vector3D& operator-=(const Vector3D&);
	Vector3D& operator*=(const Vector3D&);
	Vector3D& operator/=(const Vector3D&);

	Vector3D& operator*=(float);
	Vector3D& operator/=(float);

	// Comparison
	bool operator==(const Vector3D&) const;
	bool operator!=(const Vector3D&) const;

	// Math functions
	float Dot(const Vector3D&) const;
	Vector3D Cross(const Vector3D&) const;

	float LengthSq() const;
	float Length() const;

	Vector3D Normalized() const;
	Vector3D& Normalize();

	Vector3D ProjectOnto(const Vector3D&) const;
	Vector3D Reflect(const Vector3D& Normalized) const;

	static Vector3D Lerp(const Vector3D& a, const Vector3D& b, float t);

	inline glm::vec3 ConvertToGLM() const {
		return glm::vec3(x, y, z);
	}

	static inline Vector3D ConvertGLMToVector3D(const glm::vec3& v) {
		return Vector3D(v.x, v.y, v.z);
	}
};

typedef Vector3D Vec3;

// Left scalar
Vector3D operator*(float scalar, Vector3D& v);

// Display Vector
std::ostream& operator<<(std::ostream& os, const Vector3D& v);

inline JPH::Vec3 ToJoltVec3(const Vector3D& v)
{
	return JPH::Vec3(v.x, v.y, v.z);
}

// Converts Jolt Vec3 to your Vector3D
inline Vector3D FromJoltVec3(const JPH::Vec3& v)
{
	return Vector3D(v.GetX(), v.GetY(), v.GetZ());
}