/*********************************************************************************
* @File			Matrix3x3.cpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			3/9/2025
* @Brief		This is the Definition of 3x3 Matrix Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#include "pch.h"
#include "Math/Matrix3x3.hpp"

#pragma region Reflection

//TODO: Change to actual values and not in an array format
//REFL_REGISTER_START(Matrix3x3)
//    REFL_REGISTER_PROPERTY(m[0][0])
//    REFL_REGISTER_PROPERTY(m[0][1])
//    REFL_REGISTER_PROPERTY(m[0][2])
//    REFL_REGISTER_PROPERTY(m[1][0])
//    REFL_REGISTER_PROPERTY(m[1][1])
//    REFL_REGISTER_PROPERTY(m[1][2])
//    REFL_REGISTER_PROPERTY(m[2][0])
//    REFL_REGISTER_PROPERTY(m[2][1])
//    REFL_REGISTER_PROPERTY(m[2][2])
//REFL_REGISTER_END;

#pragma endregion

// helper macro to convert (row,col) to index
static inline void assert_rc(int r, int c) {
    assert(r >= 0 && r < 3 && c >= 0 && c < 3);
}

// ============================
// Constructors
// ============================
Matrix3x3::Matrix3x3() {
    *this = Identity();
}

Matrix3x3::Matrix3x3(float m00, float m01, float m02,
    float m10, float m11, float m12,
    float m20, float m21, float m22) {
    m = { m00,m01,m02, m10,m11,m12, m20,m21,m22 };
}


// ============================
// Element access
// ============================
float& Matrix3x3::operator()(int r, int c) {
    assert_rc(r, c);
    if (r == 0)      return c == 0 ? m.m00 : (c == 1 ? m.m01 : m.m02);
    else if (r == 1) return c == 0 ? m.m10 : (c == 1 ? m.m11 : m.m12);
    else           return c == 0 ? m.m20 : (c == 1 ? m.m21 : m.m22);
}
const float& Matrix3x3::operator()(int r, int c) const {
    assert_rc(r, c);
    if (r == 0)      return c == 0 ? m.m00 : (c == 1 ? m.m01 : m.m02);
    else if (r == 1) return c == 0 ? m.m10 : (c == 1 ? m.m11 : m.m12);
    else           return c == 0 ? m.m20 : (c == 1 ? m.m21 : m.m22);
}


// ============================
// Arithmetic operators
// ============================
Matrix3x3 Matrix3x3::operator+(const Matrix3x3& r) const {
    return {
        m.m00 + r.m.m00, m.m01 + r.m.m01, m.m02 + r.m.m02,
        m.m10 + r.m.m10, m.m11 + r.m.m11, m.m12 + r.m.m12,
        m.m20 + r.m.m20, m.m21 + r.m.m21, m.m22 + r.m.m22
    };
}

Matrix3x3 Matrix3x3::operator-(const Matrix3x3& r) const {
    return {
        m.m00 - r.m.m00, m.m01 - r.m.m01, m.m02 - r.m.m02,
        m.m10 - r.m.m10, m.m11 - r.m.m11, m.m12 - r.m.m12,
        m.m20 - r.m.m20, m.m21 - r.m.m21, m.m22 - r.m.m22
    };
}

Matrix3x3 Matrix3x3::operator*(const Matrix3x3& rhs) const {
    Matrix3x3 o;
    o.m.m00 = m.m00 * rhs.m.m00 + m.m01 * rhs.m.m10 + m.m02 * rhs.m.m20;
    o.m.m01 = m.m00 * rhs.m.m01 + m.m01 * rhs.m.m11 + m.m02 * rhs.m.m21;
    o.m.m02 = m.m00 * rhs.m.m02 + m.m01 * rhs.m.m12 + m.m02 * rhs.m.m22;

    o.m.m10 = m.m10 * rhs.m.m00 + m.m11 * rhs.m.m10 + m.m12 * rhs.m.m20;
    o.m.m11 = m.m10 * rhs.m.m01 + m.m11 * rhs.m.m11 + m.m12 * rhs.m.m21;
    o.m.m12 = m.m10 * rhs.m.m02 + m.m11 * rhs.m.m12 + m.m12 * rhs.m.m22;

    o.m.m20 = m.m20 * rhs.m.m00 + m.m21 * rhs.m.m10 + m.m22 * rhs.m.m20;
    o.m.m21 = m.m20 * rhs.m.m01 + m.m21 * rhs.m.m11 + m.m22 * rhs.m.m21;
    o.m.m22 = m.m20 * rhs.m.m02 + m.m21 * rhs.m.m12 + m.m22 * rhs.m.m22;
    return o;
}

Matrix3x3& Matrix3x3::operator*=(const Matrix3x3& rhs) {
    *this = *this * rhs;
    return *this;
}

Matrix3x3 Matrix3x3::operator*(float s) const {
    return { m.m00 * s, m.m01 * s, m.m02 * s,
             m.m10 * s, m.m11 * s, m.m12 * s,
             m.m20 * s, m.m21 * s, m.m22 * s };
}

Matrix3x3 Matrix3x3::operator/(float s) const {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this) * inv;
}

Matrix3x3& Matrix3x3::operator*=(float s) {
	m.m00 *= s; m.m01 *= s; m.m02 *= s;
	m.m10 *= s; m.m11 *= s; m.m12 *= s;
	m.m20 *= s; m.m21 *= s; m.m22 *= s;
    return *this;
}

Matrix3x3& Matrix3x3::operator/=(float s) {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this *= inv);
}

// ============================
// Vector multiply
// ============================
Vector3D Matrix3x3::operator*(const Vector3D& v) const {
    return {
        m.m00 * v.x + m.m01 * v.y + m.m02 * v.z,
        m.m10 * v.x + m.m11 * v.y + m.m12 * v.z,
		m.m20* v.x + m.m21 * v.y + m.m22 * v.z
    };
}

// ============================
// Equality
// ============================
bool Matrix3x3::operator==(const Matrix3x3& r) const {
    const float eps = 1e-6f;
    return
        std::fabs(m.m00 - r.m.m00) < eps && std::fabs(m.m01 - r.m.m01) < eps && std::fabs(m.m02 - r.m.m02) < eps &&
        std::fabs(m.m10 - r.m.m10) < eps && std::fabs(m.m11 - r.m.m11) < eps && std::fabs(m.m12 - r.m.m12) < eps &&
        std::fabs(m.m20 - r.m.m20) < eps && std::fabs(m.m21 - r.m.m21) < eps && std::fabs(m.m22 - r.m.m22) < eps;

}

// ============================
// Linear algebra
// ============================
float Matrix3x3::Determinant() const {
    return
		m.m00 * (m.m11 * m.m22 - m.m12 * m.m21) -
		m.m01 * (m.m10 * m.m22 - m.m12 * m.m20) +
		m.m02 * (m.m10 * m.m21 - m.m11 * m.m20);
}

Matrix3x3 Matrix3x3::Cofactor() const {
    Matrix3x3 c;
	c.m.m00 = (m.m11 * m.m22 - m.m12 * m.m21);
	c.m.m01 = -(m.m10 * m.m22 - m.m12 * m.m20);
	c.m.m02 = (m.m10 * m.m21 - m.m11 * m.m20);

	c.m.m10 = -(m.m01 * m.m22 - m.m02 * m.m21);
	c.m.m11 = (m.m00 * m.m22 - m.m02 * m.m20);
	c.m.m12 = -(m.m00 * m.m21 - m.m01 * m.m20);

	c.m.m20 = (m.m01 * m.m12 - m.m02 * m.m11);
	c.m.m21 = -(m.m00 * m.m12 - m.m02 * m.m10);
	c.m.m22 = (m.m00 * m.m11 - m.m01 * m.m10);
    return c;
}

Matrix3x3 Matrix3x3::Transposed() const {
    return {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m02, m.m12, m.m22
    };
}

bool Matrix3x3::TryInverse(Matrix3x3& out) const {
    float det = Determinant();
    if (std::fabs(det) < 1e-8f) return false;
    Matrix3x3 adj = Cofactor().Transposed();
    out = adj / det;
    return true;
}

Matrix3x3 Matrix3x3::Inversed() const {
    Matrix3x3 out;
    bool ok = TryInverse(out);
    assert(ok && "Matrix3x3 is singular");
    return out;
}

// ============================
// Factories
// ============================
Matrix3x3 Matrix3x3::Identity() {
    return { 1,0,0, 0,1,0, 0,0,1 };
}

Matrix3x3 Matrix3x3::Zero() {
    return { 0,0,0, 0,0,0, 0,0,0 };
}

Matrix3x3 Matrix3x3::Scale(float sx, float sy, float sz) {
    return { sx,0,0, 0,sy,0, 0,0,sz };
}

Matrix3x3 Matrix3x3::RotationX(float a) {
    float c = std::cos(a), s = std::sin(a);
    return { 1,0,0, 0,c,-s, 0,s,c };
}

Matrix3x3 Matrix3x3::RotationY(float a) {
    float c = std::cos(a), s = std::sin(a);
    return { c,0,s, 0,1,0, -s,0,c };
}

Matrix3x3 Matrix3x3::RotationZ(float a) {
    float c = std::cos(a), s = std::sin(a);
    return { c,-s,0, s,c,0, 0,0,1 };
}

Matrix3x3 Matrix3x3::RotationAxisAngle(const Vector3D& u, float a) {
    // assumes u is unit length
    float x = u.x, y = u.y, z = u.z;
    float c = std::cos(a), s = std::sin(a), t = 1 - c;
    return {
        t * x * x + c,     t * x * y - s * z, t * x * z + s * y,
        t * x * y + s * z,   t * y * y + c,   t * y * z - s * x,
        t * x * z - s * y,   t * y * z + s * x, t * z * z + c
    };
}

std::ostream& operator<<(std::ostream& os, const Matrix3x3& mat) {
    os << "[ " << mat.m.m00 << ", " << mat.m.m01 << ", " << mat.m.m02 << " ]\n"
       << "[ " << mat.m.m10 << ", " << mat.m.m11 << ", " << mat.m.m12 << " ]\n"
		<< "[ " << mat.m.m20 << ", " << mat.m.m21 << ", " << mat.m.m22 << " ]";
    return os;
}