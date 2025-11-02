#pragma once
#include <cmath>
#include <Math/Vector3D.hpp>
#include <Math/Matrix4x4.hpp>

# define M_PI           3.14159265358979323846f


struct Quaternion 
{
    REFL_SERIALIZABLE

    float w; // real part
    float x; // i component
    float y; // j component
    float z; // k component

    // Constructors
    Quaternion() : w(1), x(0), y(0), z(0) {} // identity rotation
    Quaternion(float w_, float x_, float y_, float z_)
        : w(w_), x(x_), y(y_), z(z_) {
    }

    // Normalize
    void Normalize() {
        float mag = std::sqrt(w * w + x * x + y * y + z * z);
        if (mag > 0.0f) {
            w /= mag; x /= mag; y /= mag; z /= mag;
        }
    }

    // Quaternion multiplication (rotation composition)
    Quaternion operator*(const Quaternion& rhs) const {
        return Quaternion(
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w
        );
    }

    // Inverse (for unit quaternions, this is just conjugate)
    Quaternion Inverse() const {
        return Quaternion(w, -x, -y, -z);
    }

    Matrix4x4 ToMatrix() const {
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;

        Matrix4x4 m = Matrix4x4::Identity();
        m.m.m00 = 1 - 2 * (yy + zz);
        m.m.m01 = 2 * (xy - wz);
        m.m.m02 = 2 * (xz + wy);

        m.m.m10 = 2 * (xy + wz);
        m.m.m11 = 1 - 2 * (xx + zz);
        m.m.m12 = 2 * (yz - wx);

        m.m.m20 = 2 * (xz - wy);
        m.m.m21 = 2 * (yz + wx);
        m.m.m22 = 1 - 2 * (xx + yy);

        return m;
    }

    Vector3D ToEulerDegrees() const {
        // assumes this quaternion is normalized
        Vector3D euler;

        // roll (x-axis rotation)
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        euler.x = std::atan2(sinr_cosp, cosr_cosp);

        // pitch (y-axis rotation)
        float sinp = 2.0f * (w * y - z * x);
        if (std::fabs(sinp) >= 1.0f)
            euler.y = std::copysign(M_PI / 2.0f, sinp); // clamp to 90 degrees if out of range
        else
            euler.y = std::asin(sinp);

        // yaw (z-axis rotation)
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        euler.z = std::atan2(siny_cosp, cosy_cosp);

        // convert to degrees
        const float RAD2DEG = 180.0f / M_PI;
        euler.x *= RAD2DEG;
        euler.y *= RAD2DEG;
        euler.z *= RAD2DEG;

        return euler;
    }


    static Quaternion FromEulerDegrees(const Vector3D& eulerDeg) {
        float rx = eulerDeg.x * M_PI / 180.f;
        float ry = eulerDeg.y * M_PI / 180.f;
        float rz = eulerDeg.z * M_PI / 180.f;

        float cx = std::cos(rx * 0.5f);
        float sx = std::sin(rx * 0.5f);
        float cy = std::cos(ry * 0.5f);
        float sy = std::sin(ry * 0.5f);
        float cz = std::cos(rz * 0.5f);
        float sz = std::sin(rz * 0.5f);

        return Quaternion(
            cx * cy * cz + sx * sy * sz,
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz
        );
    }

    static Quaternion FromMatrix(const Matrix4x4& m) {
        // Extract rotation-only 3x3
        float m00 = m.m.m00, m01 = m.m.m01, m02 = m.m.m02;
        float m10 = m.m.m10, m11 = m.m.m11, m12 = m.m.m12;
        float m20 = m.m.m20, m21 = m.m.m21, m22 = m.m.m22;

        float trace = m00 + m11 + m22;
        Quaternion q;

        if (trace > 0.0f) {
            float s = std::sqrt(trace + 1.0f) * 2.0f; // S = 4 * qw
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        }
        else if ((m00 > m11) && (m00 > m22)) {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f; // S = 4 * qx
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        }
        else if (m11 > m22) {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f; // S = 4 * qy
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        }
        else {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f; // S = 4 * qz
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }

        q.Normalize();
        return q;
    }

};