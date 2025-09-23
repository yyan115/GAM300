/*********************************************************************************
* @File			Matrix4x4.cpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			3/9/2025
* @Brief		This is the Defination of 4x4 Matrix Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#include "pch.h"
#include "Math/Matrix4x4.hpp"

#pragma region Reflection
//TODO: Change to actual values and not in an array format
//REFL_REGISTER_START(Matrix4x4)
//    REFL_REGISTER_PROPERTY(m[0][0])
//    REFL_REGISTER_PROPERTY(m[0][1])
//    REFL_REGISTER_PROPERTY(m[0][2])
//    REFL_REGISTER_PROPERTY(m[0][3])
//    REFL_REGISTER_PROPERTY(m[1][0])
//    REFL_REGISTER_PROPERTY(m[1][1])
//    REFL_REGISTER_PROPERTY(m[1][2])
//    REFL_REGISTER_PROPERTY(m[1][3])
//    REFL_REGISTER_PROPERTY(m[2][0])
//    REFL_REGISTER_PROPERTY(m[2][1])
//    REFL_REGISTER_PROPERTY(m[2][2])
//    REFL_REGISTER_PROPERTY(m[2][3])
//    REFL_REGISTER_PROPERTY(m[3][0])
//    REFL_REGISTER_PROPERTY(m[3][1])
//    REFL_REGISTER_PROPERTY(m[3][2])
//    REFL_REGISTER_PROPERTY(m[3][3])
//REFL_REGISTER_END;

#pragma endregion

// For asserts
static inline void assert_rc4(int r, int c) {
    assert(r >= 0 && r < 4 && c >= 0 && c < 4);
}

// ============================
// Constructors
// ============================
Matrix4x4::Matrix4x4() {
    *this = Identity();
}

Matrix4x4::Matrix4x4(float m00, float m01, float m02, float m03,
    float m10, float m11, float m12, float m13,
    float m20, float m21, float m22, float m23,
    float m30, float m31, float m32, float m33) {
	m ={m00, m01, m02, m03,
        m10, m11, m12, m13,
        m20, m21, m22, m23,
		m30, m31, m32, m33 };
}

// ============================
// Element access
// ============================
float& Matrix4x4::operator()(int r, int c)
{
    assert_rc4(r, c);
    if (r == 0)      return c == 0 ? m.m00 : (c == 1 ? m.m01 : (c == 2 ? m.m02 : m.m03));
    else if (r == 1) return c == 0 ? m.m10 : (c == 1 ? m.m11 : (c == 2 ? m.m12 : m.m13));
    else if (r == 2) return c == 0 ? m.m20 : (c == 1 ? m.m21 : (c == 2 ? m.m22 : m.m23));
    else           return c == 0 ? m.m30 : (c == 1 ? m.m31 : (c == 2 ? m.m32 : m.m33));
}
const float& Matrix4x4::operator()(int r, int c) const 
{
    assert_rc4(r, c);
    if (r == 0)      return c == 0 ? m.m00 : (c == 1 ? m.m01 : (c == 2 ? m.m02 : m.m03));
    else if (r == 1) return c == 0 ? m.m10 : (c == 1 ? m.m11 : (c == 2 ? m.m12 : m.m13));
    else if (r == 2) return c == 0 ? m.m20 : (c == 1 ? m.m21 : (c == 2 ? m.m22 : m.m23));
    else           return c == 0 ? m.m30 : (c == 1 ? m.m31 : (c == 2 ? m.m32 : m.m33));
}

// ============================
// Arithmetic
// ============================
Matrix4x4 Matrix4x4::operator+(const Matrix4x4& rhs) const {
    return {
        m.m00 + rhs.m.m00, m.m01 + rhs.m.m01, m.m02 + rhs.m.m02, m.m03 + rhs.m.m03,
        m.m10 + rhs.m.m10, m.m11 + rhs.m.m11, m.m12 + rhs.m.m12, m.m13 + rhs.m.m13,
        m.m20 + rhs.m.m20, m.m21 + rhs.m.m21, m.m22 + rhs.m.m22, m.m23 + rhs.m.m23,
        m.m30 + rhs.m.m30, m.m31 + rhs.m.m31, m.m32 + rhs.m.m32, m.m33 + rhs.m.m33
    };
}

Matrix4x4 Matrix4x4::operator-(const Matrix4x4& rhs) const {
    return {
        m.m00 - rhs.m.m00, m.m01 - rhs.m.m01, m.m02 - rhs.m.m02, m.m03 - rhs.m.m03,
        m.m10 - rhs.m.m10, m.m11 - rhs.m.m11, m.m12 - rhs.m.m12, m.m13 - rhs.m.m13,
        m.m20 - rhs.m.m20, m.m21 - rhs.m.m21, m.m22 - rhs.m.m22, m.m23 - rhs.m.m23,
        m.m30 - rhs.m.m30, m.m31 - rhs.m.m31, m.m32 - rhs.m.m32, m.m33 - rhs.m.m33
    };
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const {
    Matrix4x4 o;
    // Row 0
    o.m.m00 = m.m00 * rhs.m.m00 + m.m01 * rhs.m.m10 + m.m02 * rhs.m.m20 + m.m03 * rhs.m.m30;
    o.m.m01 = m.m00 * rhs.m.m01 + m.m01 * rhs.m.m11 + m.m02 * rhs.m.m21 + m.m03 * rhs.m.m31;
    o.m.m02 = m.m00 * rhs.m.m02 + m.m01 * rhs.m.m12 + m.m02 * rhs.m.m22 + m.m03 * rhs.m.m32;
    o.m.m03 = m.m00 * rhs.m.m03 + m.m01 * rhs.m.m13 + m.m02 * rhs.m.m23 + m.m03 * rhs.m.m33;
    // Row 1
    o.m.m10 = m.m10 * rhs.m.m00 + m.m11 * rhs.m.m10 + m.m12 * rhs.m.m20 + m.m13 * rhs.m.m30;
    o.m.m11 = m.m10 * rhs.m.m01 + m.m11 * rhs.m.m11 + m.m12 * rhs.m.m21 + m.m13 * rhs.m.m31;
    o.m.m12 = m.m10 * rhs.m.m02 + m.m11 * rhs.m.m12 + m.m12 * rhs.m.m22 + m.m13 * rhs.m.m32;
    o.m.m13 = m.m10 * rhs.m.m03 + m.m11 * rhs.m.m13 + m.m12 * rhs.m.m23 + m.m13 * rhs.m.m33;
    // Row 2
    o.m.m20 = m.m20 * rhs.m.m00 + m.m21 * rhs.m.m10 + m.m22 * rhs.m.m20 + m.m23 * rhs.m.m30;
    o.m.m21 = m.m20 * rhs.m.m01 + m.m21 * rhs.m.m11 + m.m22 * rhs.m.m21 + m.m23 * rhs.m.m31;
    o.m.m22 = m.m20 * rhs.m.m02 + m.m21 * rhs.m.m12 + m.m22 * rhs.m.m22 + m.m23 * rhs.m.m32;
    o.m.m23 = m.m20 * rhs.m.m03 + m.m21 * rhs.m.m13 + m.m22 * rhs.m.m23 + m.m23 * rhs.m.m33;
    // Row 3
    o.m.m30 = m.m30 * rhs.m.m00 + m.m31 * rhs.m.m10 + m.m32 * rhs.m.m20 + m.m33 * rhs.m.m30;
    o.m.m31 = m.m30 * rhs.m.m01 + m.m31 * rhs.m.m11 + m.m32 * rhs.m.m21 + m.m33 * rhs.m.m31;
    o.m.m32 = m.m30 * rhs.m.m02 + m.m31 * rhs.m.m12 + m.m32 * rhs.m.m22 + m.m33 * rhs.m.m32;
    o.m.m33 = m.m30 * rhs.m.m03 + m.m31 * rhs.m.m13 + m.m32 * rhs.m.m23 + m.m33 * rhs.m.m33;
    return o;
}

Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& rhs) {
    *this = *this * rhs;
    return *this;
}

Matrix4x4 Matrix4x4::operator*(float s) const {
    return {
        m.m00 * s, m.m01 * s, m.m02 * s, m.m03 * s,
        m.m10 * s, m.m11 * s, m.m12 * s, m.m13 * s,
        m.m20 * s, m.m21 * s, m.m22 * s, m.m23 * s,
        m.m30 * s, m.m31 * s, m.m32 * s, m.m33 * s
    };
}

Matrix4x4 Matrix4x4::operator/(float s) const {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this) * inv;
}

Matrix4x4& Matrix4x4::operator*=(float s) {
    m.m00 *= s; m.m01 *= s; m.m02 *= s; m.m03 *= s;
    m.m10 *= s; m.m11 *= s; m.m12 *= s; m.m13 *= s;
    m.m20 *= s; m.m21 *= s; m.m22 *= s; m.m23 *= s;
    m.m30 *= s; m.m31 *= s; m.m32 *= s; m.m33 *= s;
    return *this;
}

Matrix4x4& Matrix4x4::operator/=(float s) {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this *= inv);
}

bool Matrix4x4::operator==(const Matrix4x4& rhs) const {
    const float e = 1e-6f;
    return
        std::fabs(m.m00 - rhs.m.m00) < e && std::fabs(m.m01 - rhs.m.m01) < e &&
        std::fabs(m.m02 - rhs.m.m02) < e && std::fabs(m.m03 - rhs.m.m03) < e &&
                                            
        std::fabs(m.m10 - rhs.m.m10) < e && std::fabs(m.m11 - rhs.m.m11) < e &&
        std::fabs(m.m12 - rhs.m.m12) < e && std::fabs(m.m13 - rhs.m.m13) < e &&
                                            
        std::fabs(m.m20 - rhs.m.m20) < e && std::fabs(m.m21 - rhs.m.m21) < e &&
        std::fabs(m.m22 - rhs.m.m22) < e && std::fabs(m.m23 - rhs.m.m23) < e &&

        std::fabs(m.m30 - rhs.m.m30) < e && std::fabs(m.m31 - rhs.m.m31) < e &&
        std::fabs(m.m32 - rhs.m.m32) < e && std::fabs(m.m33 - rhs.m.m33) < e;
}

// ============================
// Vector transforms
// ============================
Vector3D Matrix4x4::TransformPoint(const Vector3D& v) const 
{
    float x = m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03;
    float y = m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13;
    float z = m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23;
    float w = m.m30 * v.x + m.m31 * v.y + m.m32 * v.z + m.m33;
    if (std::fabs(w) > 1e-8f) {
        float invw = 1.0f / w;
        return { x * invw, y * invw, z * invw };
    }
    // If w==0 (shouldn't happen for points), just return xyz
    return { x, y, z };
}

Vector3D Matrix4x4::TransformVector(const Vector3D& v) const 
{
    // w=0 => translation ignored
    return {
        m.m00 * v.x + m.m01 * v.y + m.m02 * v.z,
        m.m10 * v.x + m.m11 * v.y + m.m12 * v.z,
        m.m20 * v.x + m.m21 * v.y + m.m22 * v.z
    };
}

// ============================
// Linear algebra
// ============================
Matrix4x4 Matrix4x4::Transposed() const {
    return {
		m.m00, m.m10, m.m20, m.m30,
		m.m01, m.m11, m.m21, m.m31,
		m.m02, m.m12, m.m22, m.m32,
		m.m03, m.m13, m.m23, m.m33
    };
}

static inline float Det3(   float a00, float a01, float a02,
                            float a10, float a11, float a12,
                            float a20, float a21, float a22) {
    return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
}

float Matrix4x4::Determinant() const {
    float c0 =  Det3(m.m11,m.m12,m.m13, m.m21,m.m22,m.m23, m.m31,m.m32,m.m33);
    float c1 = -Det3(m.m10,m.m12,m.m13, m.m20,m.m22,m.m23, m.m30,m.m32,m.m33);
    float c2 =  Det3(m.m10,m.m11,m.m13, m.m20,m.m21,m.m23, m.m30,m.m31,m.m33);
    float c3 = -Det3(m.m10,m.m11,m.m12, m.m20,m.m21,m.m22, m.m30,m.m31,m.m32);
    return m.m00*c0 + m.m01*c1 + m.m02*c2 + m.m03*c3;
}

// Robust Gauss-Jordan inverse
bool Matrix4x4::TryInverse(Matrix4x4& out) const {
    float a[4][8] = {
        {m.m00,m.m01,m.m02,m.m03, 1,0,0,0},
        {m.m10,m.m11,m.m12,m.m13, 0,1,0,0},
        {m.m20,m.m21,m.m22,m.m23, 0,0,1,0},
        {m.m30,m.m31,m.m32,m.m33, 0,0,0,1}
    };

    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        float maxAbs = std::fabs(a[pivot][col]);
        for (int r = col + 1; r < 4; ++r) {
            float v = std::fabs(a[r][col]);
            if (v > maxAbs) { maxAbs = v; pivot = r; }
        }
        if (maxAbs < 1e-10f) return false;

        if (pivot != col) { for (int j = 0; j < 8; ++j) std::swap(a[pivot][j], a[col][j]); }

        float invP = 1.0f / a[col][col];
        for (int j = 0; j < 8; ++j) a[col][j] *= invP;

        for (int r = 0; r < 4; ++r) if (r != col) {
            float f = a[r][col];
            if (std::fabs(f) < 1e-20f) continue;
            for (int j = 0; j < 8; ++j) a[r][j] -= f * a[col][j];
        }
    }
    out.m.m00 = a[0][4]; out.m.m01 = a[0][5]; out.m.m02 = a[0][6]; out.m.m03 = a[0][7];
    out.m.m10 = a[1][4]; out.m.m11 = a[1][5]; out.m.m12 = a[1][6]; out.m.m13 = a[1][7];
    out.m.m20 = a[2][4]; out.m.m21 = a[2][5]; out.m.m22 = a[2][6]; out.m.m23 = a[2][7];
    out.m.m30 = a[3][4]; out.m.m31 = a[3][5]; out.m.m32 = a[3][6]; out.m.m33 = a[3][7];
    return true;
}

Matrix4x4 Matrix4x4::Inversed() const {
    Matrix4x4 inv;
    bool ok = TryInverse(inv);
    assert(ok && "Matrix4x4 is singular");
    return inv;
}

// ============================
// Factories
// ============================
Matrix4x4 Matrix4x4::Identity() {
    return { 1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1 };
}

Matrix4x4 Matrix4x4::Zero() {
    return {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0};
}

Matrix4x4 Matrix4x4::Translate(float tx, float ty, float tz) {
    return {
        1,0,0,tx,
        0,1,0,ty,
        0,0,1,tz,
        0,0,0,1
    };
}

Matrix4x4 Matrix4x4::Scale(float sx, float sy, float sz) {
    return {
        sx,0, 0, 0,
        0, sy,0, 0,
        0, 0, sz,0,
        0, 0, 0, 1
    };
}

Matrix4x4 Matrix4x4::RotationX(float a) {
    float c = std::cos(a), s = std::sin(a);
    return {
        1, 0, 0, 0,
        0, c,-s, 0,
        0, s, c, 0,
        0, 0, 0, 1
    };
}

Matrix4x4 Matrix4x4::RotationY(float a) {
    float c = std::cos(a), s = std::sin(a);
    return {
         c, 0, s, 0,
         0, 1, 0, 0,
        -s, 0, c, 0,
         0, 0, 0, 1
    };
}

Matrix4x4 Matrix4x4::RotationZ(float a) {
    float c = std::cos(a), s = std::sin(a);
    return {
        c,-s, 0, 0,
        s, c, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
}

Matrix4x4 Matrix4x4::RotationAxisAngle(const Vector3D& axis_unit, float a) {
    float x = axis_unit.x, y = axis_unit.y, z = axis_unit.z;
    float c = std::cos(a), s = std::sin(a), t = 1.0f - c;
    // Upper-left 3x3 is the rotation; last row/col make it affine
    return {
        t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,  0,
        t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,  0,
        t * x * z - s * y,  t * y * z + s * x,  t * z * z + c,      0,
        0,                  0,                  0,                  1
    };
}

// Compose T * R * S  (applies S then R then T to column vectors)
Matrix4x4 Matrix4x4::TRS(const Vector3D& t, const Matrix4x4& R, const Vector3D& s) {
    Matrix4x4 S = Scale(s.x, s.y, s.z);
    Matrix4x4 T = Translate(t.x, t.y, t.z);
    return T * R * S;
}

// ============================
// Camera / Projection (RH)
// ============================

static inline Vector3D Normalize(const Vector3D& v) {
    float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
    float inv = 1.0f / std::sqrt(len2);
    return { v.x * inv, v.y * inv, v.z * inv };
}
static inline Vector3D Cross(const Vector3D& a, const Vector3D& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
static inline float Dot(const Vector3D& a, const Vector3D& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Right-handed look-at (OpenGL-style, -Z forward after view transform)
Matrix4x4 Matrix4x4::LookAtRH(const Vector3D& eye, const Vector3D& target, const Vector3D& up) {
    Vector3D f = Normalize({ eye.x - target.x, eye.y - target.y, eye.z - target.z }); // forward
    Vector3D r = Normalize(Cross(up, f));   // right
    Vector3D u = Cross(f, r);               // up (already normalized)

    // Row-major, column-vector: view = [ R | t; 0 0 0 1 ] with t = -R*eye
    return {
        r.x, r.y, r.z, -Dot(r, eye),
        u.x, u.y, u.z, -Dot(u, eye),
        f.x, f.y, f.z, -Dot(f, eye),
        0,   0,   0,    1
    };
}

// Right-handed perspective (clip-space z in [-1,1] if using OpenGL)
Matrix4x4 Matrix4x4::PerspectiveFovRH(float fovY, float aspect, float zNear, float zFar) {
    assert(aspect > 0.0f && zNear > 0.0f && zFar > zNear);
    float f = 1.0f / std::tan(fovY * 0.5f);
    float A = (zFar + zNear) / (zNear - zFar);
    float B = (2.0f * zFar * zNear) / (zNear - zFar);
    return {
        f / aspect, 0, 0,  0,
        0,        f, 0,  0,
        0,        0, A,  B,
        0,        0,-1,  0
    };
}

Matrix4x4 Matrix4x4::OrthoRH(float l, float r, float b, float t, float n, float fz) {
    assert(r != l && t != b && fz != n);
    float sx = 2.0f / (r - l);
    float sy = 2.0f / (t - b);
    float sz = -2.0f / (fz - n);
    float tx = -(r + l) / (r - l);
    float ty = -(t + b) / (t - b);
    float tz = -(fz + n) / (fz - n);
    return {
        sx, 0,  0,  tx,
        0,  sy, 0,  ty,
        0,  0,  sz, tz,
        0,  0,  0,  1
    };
}

std::ostream& operator<<(std::ostream& os, const Matrix4x4& mat) {
    os << mat.m.m00 << ", " << mat.m.m01 << ", " << mat.m.m02 << ", " << mat.m.m03 << ";\n "
        << mat.m.m10 << ", " << mat.m.m11 << ", " << mat.m.m12 << ", " << mat.m.m13 << ";\n "
        << mat.m.m20 << ", " << mat.m.m21 << ", " << mat.m.m22 << ", " << mat.m.m23 << ";\n "
        << mat.m.m30 << ", " << mat.m.m31 << ", " << mat.m.m32 << ", " << mat.m.m33 << "]\n";
    return os;
}