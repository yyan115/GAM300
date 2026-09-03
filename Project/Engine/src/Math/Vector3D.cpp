/*********************************************************************************
* @File			Vector3.cpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			3/9/2025
* @Brief		This is the Definition of Vector3 Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#include "pch.h"
#include "Math/Vector3D.hpp"

#pragma region Reflection
REFL_REGISTER_START(Vector3D)
	REFL_REGISTER_PROPERTY(x)
	REFL_REGISTER_PROPERTY(y)
	REFL_REGISTER_PROPERTY(z)
REFL_REGISTER_END
#pragma endregion

// Indexing
float& Vector3D::operator[](int i)
{
	if (i < 0 || i>2) throw std::out_of_range("Vector3 indexing out of range");
	return *(&x + i);
}

const float& Vector3D::operator[](int i) const
{
	if (i < 0 || i>2) throw std::out_of_range("Vector3 indexing out of range");
	return *(&x + i);
}

float Vector3D::Length() const { return std::sqrt(LengthSq()); }

Vector3D Vector3D::Normalized() const 
{
	float len = std::sqrt(LengthSq());
	return (len > 0.0f) ? (*this / len) : Vector3D::Zero();
}

Vector3D& Vector3D::Normalize() 
{
	float len = std::sqrt(LengthSq());
	if (len > 0.0f) 
	{ 
		x /= len; 
		y /= len; 
		z /= len; 
	}
	return *this;
}

Vector3D Vector3D::ProjectOnto(const Vector3D& n) const 
{
	float d = n.LengthSq();
	if (d <= 0.0f) return Vector3D::Zero();
	return n * (Dot(n) / d);
}

Vector3D Vector3D::Reflect(const Vector3D& n_normalized) const 
{
	return *this - n_normalized * (2.0f * this->Dot(n_normalized));
}

Vector3D Vector3D::Lerp(const Vector3D& a, const Vector3D& b, float t) 
{
	return a + (b - a) * t;
}


// ---- Stream output ----
std::ostream& operator<<(std::ostream& os, const Vector3D& v) {
	return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}
