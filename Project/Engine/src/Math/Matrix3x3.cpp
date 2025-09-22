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

// ============================
// Constructors
// ============================
Matrix3x3::Matrix3x3() {
    *this = Identity();
}

Matrix3x3::Matrix3x3(float m00, float m01, float m02,
    float m10, float m11, float m12,
    float m20, float m21, float m22) {
    m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
    m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
    m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
}

// ============================
// Arithmetic operators
// ============================
Matrix3x3 Matrix3x3::operator+(const Matrix3x3& rhs) const {
    Matrix3x3 out;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out.m[i][j] = m[i][j] + rhs.m[i][j];
    return out;
}

Matrix3x3 Matrix3x3::operator-(const Matrix3x3& rhs) const {
    Matrix3x3 out;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out.m[i][j] = m[i][j] - rhs.m[i][j];
    return out;
}

Matrix3x3 Matrix3x3::operator*(const Matrix3x3& rhs) const {
    Matrix3x3 out;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out.m[i][j] = m[i][0] * rhs.m[0][j] +
                m[i][1] * rhs.m[1][j] +
                m[i][2] * rhs.m[2][j];
        }
    }
    return out;
}

Matrix3x3& Matrix3x3::operator*=(const Matrix3x3& rhs) {
    *this = *this * rhs;
    return *this;
}

Matrix3x3 Matrix3x3::operator*(float s) const {
    Matrix3x3 out;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out.m[i][j] = m[i][j] * s;
    return out;
}

Matrix3x3 Matrix3x3::operator/(float s) const {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this) * inv;
}

Matrix3x3& Matrix3x3::operator*=(float s) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            m[i][j] *= s;
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
        m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
        m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
        m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
    };
}

// ============================
// Equality
// ============================
bool Matrix3x3::operator==(const Matrix3x3& rhs) const {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (std::fabs(m[i][j] - rhs.m[i][j]) > 1e-6f)
                return false;
    return true;
}

// ============================
// Linear algebra
// ============================
float Matrix3x3::Determinant() const {
    return
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

Matrix3x3 Matrix3x3::Cofactor() const {
    Matrix3x3 c;
    c.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
    c.m[0][1] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]);
    c.m[0][2] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    c.m[1][0] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]);
    c.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]);
    c.m[1][2] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]);

    c.m[2][0] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
    c.m[2][1] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]);
    c.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]);
    return c;
}

Matrix3x3 Matrix3x3::Transposed() const {
    return {
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2]
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
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            os << mat.m[i][j];
            if (j < 2) os << ", ";
        }
        if (i < 2) os << '\n';
    }
    return os;
}