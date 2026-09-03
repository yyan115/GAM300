/*********************************************************************************
* @File			Matrix4x4.hpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			3/9/2025
* @Brief		This is the Declaration of 4x4 Matrix Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/
#pragma once

#include "pch.h"
#include "Math/Vector3D.hpp"
#include "glm/mat4x4.hpp"

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

struct ENGINE_API Matrix4x4 {
    REFL_SERIALIZABLE
    // Row-major storage: m[row][col]
    struct Matrix
    {
        float m00, m01, m02, m03;
        float m10, m11, m12, m13;
        float m20, m21, m22, m23;
        float m30, m31, m32, m33;
    }m;

    // ---- ctors ----
    constexpr Matrix4x4() noexcept
        : m{1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f} {}
    constexpr Matrix4x4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) noexcept
        : m{m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23,
            m30, m31, m32, m33} {}

    // ---- element access ----
    float& operator()(int r, int s);
    const float& operator()(int r, int c) const;


    // ---- arithmetic ----
    Matrix4x4  operator+(const Matrix4x4& rhs) const;
    Matrix4x4  operator-(const Matrix4x4& rhs) const;
    Matrix4x4  operator*(const Matrix4x4& rhs) const; // composition
    Matrix4x4& operator*=(const Matrix4x4& rhs);
    Matrix4x4  operator*(float s) const;
    Matrix4x4  operator/(float s) const;
    Matrix4x4& operator*=(float s);
    Matrix4x4& operator/=(float s);

    bool operator==(const Matrix4x4& rhs) const;

    // ---- vector transforms (column-vector convention) ----
    // Treats v as (x,y,z,1). Returns perspective-divided Vector3D.
    Vector3D TransformPoint(const Vector3D& v) const;
    // Treats v as (x,y,z,0). Ignores translation.
    Vector3D TransformVector(const Vector3D& v) const;

    // ---- linear algebra ----
    Matrix4x4 Transposed() const;
    float     Determinant() const;
    bool      TryInverse(Matrix4x4& out) const;  // false if singular
    Matrix4x4 Inversed() const;                  // asserts if singular

    // glm conversions
    inline glm::mat4 ConvertToGLM() const {
        // GLM's scalar constructor takes columns. Feed our row-major fields in
        // column order directly instead of building an intermediate transpose.
        return glm::mat4(
            m.m00, m.m10, m.m20, m.m30,
            m.m01, m.m11, m.m21, m.m31,
            m.m02, m.m12, m.m22, m.m32,
            m.m03, m.m13, m.m23, m.m33);
    }

    inline static Matrix4x4 ConvertToMatrix4x4(const glm::mat4& m) {
        // GLM is column-major, Matrix4x4 is row-major, so we need to transpose
        return Matrix4x4(Matrix{
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3]
        });
    }

    // Composition for matrices whose final row is [0, 0, 0, 1]. Transform
    // hierarchies are affine by construction, so their hot path need not pay
    // for the unused projective row/column terms of a general 4x4 multiply.
    inline static Matrix4x4 MultiplyAffine(
        const Matrix4x4& lhs,
        const Matrix4x4& rhs) {
        return Matrix4x4(Matrix{
            lhs.m.m00 * rhs.m.m00 + lhs.m.m01 * rhs.m.m10 + lhs.m.m02 * rhs.m.m20,
            lhs.m.m00 * rhs.m.m01 + lhs.m.m01 * rhs.m.m11 + lhs.m.m02 * rhs.m.m21,
            lhs.m.m00 * rhs.m.m02 + lhs.m.m01 * rhs.m.m12 + lhs.m.m02 * rhs.m.m22,
            lhs.m.m00 * rhs.m.m03 + lhs.m.m01 * rhs.m.m13 + lhs.m.m02 * rhs.m.m23 + lhs.m.m03,
            lhs.m.m10 * rhs.m.m00 + lhs.m.m11 * rhs.m.m10 + lhs.m.m12 * rhs.m.m20,
            lhs.m.m10 * rhs.m.m01 + lhs.m.m11 * rhs.m.m11 + lhs.m.m12 * rhs.m.m21,
            lhs.m.m10 * rhs.m.m02 + lhs.m.m11 * rhs.m.m12 + lhs.m.m12 * rhs.m.m22,
            lhs.m.m10 * rhs.m.m03 + lhs.m.m11 * rhs.m.m13 + lhs.m.m12 * rhs.m.m23 + lhs.m.m13,
            lhs.m.m20 * rhs.m.m00 + lhs.m.m21 * rhs.m.m10 + lhs.m.m22 * rhs.m.m20,
            lhs.m.m20 * rhs.m.m01 + lhs.m.m21 * rhs.m.m11 + lhs.m.m22 * rhs.m.m21,
            lhs.m.m20 * rhs.m.m02 + lhs.m.m21 * rhs.m.m12 + lhs.m.m22 * rhs.m.m22,
            lhs.m.m20 * rhs.m.m03 + lhs.m.m21 * rhs.m.m13 + lhs.m.m22 * rhs.m.m23 + lhs.m.m23,
            0.0f, 0.0f, 0.0f, 1.0f
        });
    }

    // ---- factories ----
    static Matrix4x4 Identity();
    static Matrix4x4 Zero();

    static Matrix4x4 Translate(float tx, float ty, float tz);
    static Matrix4x4 Scale(float sx, float sy, float sz);
    static Matrix4x4 Scale(float s) { return Scale(s, s, s); }

    static Matrix4x4 RotationX(float radians);
    static Matrix4x4 RotationY(float radians);
    static Matrix4x4 RotationZ(float radians);
    static Matrix4x4 RotationAxisAngle(const Vector3D& axis_unit, float radians);

    // Compose: T * R * S (column-vector math; applied S then R then T)
    static Matrix4x4 TRS(const Vector3D& t, const Matrix4x4& R, const Vector3D& s);

    // ---- camera / projection (Right-Handed) ----
    static Matrix4x4 LookAtRH(const Vector3D& eye, const Vector3D& target, const Vector3D& up);

    // fovY in radians, aspect = width/height, zNear>0, zFar>zNear
    static Matrix4x4 PerspectiveFovRH(float fovY, float aspect, float zNear, float zFar);
    static Matrix4x4 OrthoRH(float left, float right, float bottom, float top, float zNear, float zFar);

    // Extract translation, scale, rotation from world matrix
    inline static Vector3D ExtractTranslation(const Matrix4x4& matrix) noexcept {
        return {matrix.m.m03, matrix.m.m13, matrix.m.m23};
    }
    inline static Vector3D ExtractScale(const Matrix4x4& matrix) noexcept {
        // Scaled local basis axes are columns for column-vector transforms.
        return {
            std::sqrt(matrix.m.m00 * matrix.m.m00 + matrix.m.m10 * matrix.m.m10 + matrix.m.m20 * matrix.m.m20),
            std::sqrt(matrix.m.m01 * matrix.m.m01 + matrix.m.m11 * matrix.m.m11 + matrix.m.m21 * matrix.m.m21),
            std::sqrt(matrix.m.m02 * matrix.m.m02 + matrix.m.m12 * matrix.m.m12 + matrix.m.m22 * matrix.m.m22)
        };
    }
    static Vector3D ExtractRotation(const Matrix4x4& m);

    static Matrix4x4 RemoveScale(const Matrix4x4& m);

private:
    // Internal fully-initialized construction avoids paying for the public
    // identity default before arithmetic overwrites every element.
    constexpr explicit Matrix4x4(Matrix values) noexcept : m(values) {}
};

// left scalar
inline Matrix4x4 operator*(float s, const Matrix4x4& M) { return M * s; }

// Output
std::ostream& operator<<(std::ostream& os, const Matrix4x4& mat);
