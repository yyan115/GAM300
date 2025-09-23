/*********************************************************************************
* @File			Matrix3x3.hpp
* @Author		Ernest Ho, h.yonghengernest@digipen.edu
* @Co-Author	-
* @Date			3/9/2025
* @Brief		This is the Declaration of 3x3 Matrix Class
*
* Copyright (C) 20xx DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/
#pragma once

#include "pch.h"
#include "Math/Vector3D.hpp"

struct Matrix3x3 {
    //REFL_SERIALIZABLE
    // Storage: row-major (m[row][col])
    struct Matrix
    {
        float m00, m01, m02;
        float m10, m11, m12;
		float m20, m21, m22;
    }m;

    // ---- ctors ----
    Matrix3x3();                                 // identity by default is convenient
    Matrix3x3(float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22);

    // ---- element access ----
    float& operator()(int r, int c);
    const float& operator()(int r, int c) const;

    // ---- arithmetic ----
    Matrix3x3  operator+(const Matrix3x3& rhs) const;
    Matrix3x3  operator-(const Matrix3x3& rhs) const;
    Matrix3x3  operator*(const Matrix3x3& rhs) const;     // composition
    Matrix3x3& operator*=(const Matrix3x3& rhs);
    Matrix3x3  operator*(float s) const;
    Matrix3x3  operator/(float s) const;
    Matrix3x3& operator*=(float s);
    Matrix3x3& operator/=(float s);

    Vector3D   operator*(const Vector3D& v) const;        // M * v

    bool operator==(const Matrix3x3& rhs) const;

    // ---- linear algebra ----
    float     Determinant() const;
    Matrix3x3 Cofactor() const;
    Matrix3x3 Transposed() const;
    bool      TryInverse(Matrix3x3& out) const;           // false if singular
    Matrix3x3 Inversed() const;                            // may assert/throw if singular

    // ---- factories (preferred over mutating setters) ----
    static Matrix3x3 Identity();
    static Matrix3x3 Zero();
    static Matrix3x3 Scale(float sx, float sy, float sz);
    static Matrix3x3 RotationX(float radians);
    static Matrix3x3 RotationY(float radians);
    static Matrix3x3 RotationZ(float radians);
    static Matrix3x3 RotationAxisAngle(const Vector3D& axis_unit, float radians); // optional
};

typedef Matrix3x3 Mat3;

// left scalar
inline Matrix3x3 operator*(float s, const Matrix3x3& M) { return M * s; }

// Output
std::ostream& operator<<(std::ostream& os, const Matrix3x3& mat);