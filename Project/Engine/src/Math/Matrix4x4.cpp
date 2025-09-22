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
    m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
    m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
    m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
    m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
}

// ============================
// Arithmetic
// ============================
Matrix4x4 Matrix4x4::operator+(const Matrix4x4& rhs) const {
    Matrix4x4 out;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out.m[i][j] = m[i][j] + rhs.m[i][j];
    return out;
}

Matrix4x4 Matrix4x4::operator-(const Matrix4x4& rhs) const {
    Matrix4x4 out;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out.m[i][j] = m[i][j] - rhs.m[i][j];
    return out;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const {
    Matrix4x4 out;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out.m[i][j] = 
                m[i][0] * rhs.m[0][j] +
                m[i][1] * rhs.m[1][j] +
                m[i][2] * rhs.m[2][j] +
                m[i][3] * rhs.m[3][j];
        }
    }
    return out;
}

Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& rhs) {
    *this = *this * rhs;
    return *this;
}

Matrix4x4 Matrix4x4::operator*(float s) const {
    Matrix4x4 out;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out.m[i][j] = m[i][j] * s;
    return out;
}

Matrix4x4 Matrix4x4::operator/(float s) const {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this) * inv;
}

Matrix4x4& Matrix4x4::operator*=(float s) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] *= s;
    return *this;
}

Matrix4x4& Matrix4x4::operator/=(float s) {
    assert(std::fabs(s) > 1e-8f && "Division by zero");
    float inv = 1.0f / s;
    return (*this *= inv);
}

bool Matrix4x4::operator==(const Matrix4x4& rhs) const {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (std::fabs(m[i][j] - rhs.m[i][j]) > 1e-6f)
                return false;
    return true;
}

// ============================
// Vector transforms
// ============================
Vector3D Matrix4x4::TransformPoint(const Vector3D& v) const {
    float x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * 1.0f;
    float y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * 1.0f;
    float z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * 1.0f;
    float w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * 1.0f;
    if (std::fabs(w) > 1e-8f) {
        float invw = 1.0f / w;
        return { x * invw, y * invw, z * invw };
    }
    // If w==0 (shouldn't happen for points), just return xyz
    return { x, y, z };
}

Vector3D Matrix4x4::TransformVector(const Vector3D& v) const {
    // w=0 => translation ignored
    return {
        m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
        m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
        m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
    };
}

// ============================
// Linear algebra
// ============================
Matrix4x4 Matrix4x4::Transposed() const {
    return {
        m[0][0], m[1][0], m[2][0], m[3][0],
        m[0][1], m[1][1], m[2][1], m[3][1],
        m[0][2], m[1][2], m[2][2], m[3][2],
        m[0][3], m[1][3], m[2][3], m[3][3]
    };
}

static inline float Det3(const float a[3][3]) {
    return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
        - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
        + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
}

float Matrix4x4::Determinant() const {
    // Expand along row 0 using 3x3 minors
    float det = 0.0f;
    for (int col = 0; col < 4; ++col) {
        float sub[3][3];
        // build minor excluding row 0, column 'col'
        for (int i = 1, si = 0; i < 4; ++i, ++si) {
            int sj = 0;
            for (int j = 0; j < 4; ++j) {
                if (j == col) continue;
                sub[si][sj++] = m[i][j];
            }
        }
        float cofactor = ((col % 2) ? -1.0f : 1.0f) * Det3(sub);
        det += m[0][col] * cofactor;
    }
    return det;
}

// Robust Gauss-Jordan inverse
bool Matrix4x4::TryInverse(Matrix4x4& out) const {
    // Augment [A | I] and reduce to [I | A^{-1}]
    float a[4][8] = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) a[i][j] = m[i][j];
        for (int j = 0; j < 4; ++j) a[i][4 + j] = (i == j) ? 1.0f : 0.0f;
    }

    for (int col = 0; col < 4; ++col) {
        // Pivot selection (max abs)
        int pivot = col;
        float maxAbs = std::fabs(a[pivot][col]);
        for (int r = col + 1; r < 4; ++r) {
            float v = std::fabs(a[r][col]);
            if (v > maxAbs) { maxAbs = v; pivot = r; }
        }
        if (maxAbs < 1e-10f) return false; // singular

        // Swap pivot row
        if (pivot != col) {
            for (int j = 0; j < 8; ++j) std::swap(a[pivot][j], a[col][j]);
        }

        // Normalize pivot row
        float invPivot = 1.0f / a[col][col];
        for (int j = 0; j < 8; ++j) a[col][j] *= invPivot;

        // Eliminate other rows
        for (int r = 0; r < 4; ++r) {
            if (r == col) continue;
            float f = a[r][col];
            if (std::fabs(f) < 1e-20f) continue;
            for (int j = 0; j < 8; ++j) a[r][j] -= f * a[col][j];
        }
    }

    // Extract right block as inverse
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out.m[i][j] = a[i][4 + j];

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
    return {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
}

Matrix4x4 Matrix4x4::Zero() {
    return {
        0,0,0,0,
        0,0,0,0,
        0,0,0,0,
        0,0,0,0
    };
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
        t * x * x + c,     t * x * y - s * z, t * x * z + s * y, 0,
        t * x * y + s * z,   t * y * y + c,   t * y * z - s * x, 0,
        t * x * z - s * y,   t * y * z + s * x, t * z * z + c,   0,
        0,             0,           0,           1
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
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            os << mat.m[i][j];
            if (j < 3) os << ", ";
        }
        if (i < 3) os << '\n';
    }
    return os;
}