#pragma once
#include "glm/glm.hpp"
#include <array>
#include <cfloat>

struct Plane {
	glm::vec3 normal;
	float distance;

	Plane() : normal(0.f), distance(0.f) {}

	// Signed distance from point to plane
	float GetSignedDistanceToPoint(const glm::vec3& point) const 
	{
		return glm::dot(normal, point) + distance;
	}

	void Normalize()
	{
		float length = glm::length(normal);
		normal /= length;
		distance /= length;
	}
};

struct AABB {
	glm::vec3 min;
	glm::vec3 max;

	AABB() : min(0.f), max(0.f) {}
	AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

	glm::vec3 GetCenter() const 
	{
		return (max + min) * 0.5f;
	}

	glm::vec3 GetExtents() const
	{
		return (max - min) * 0.5f;
	}

	glm::vec3 GetPositiveVertex(const glm::vec3& normal) const
	{
		glm::vec3 p = min;
		if (normal.x >= 0) p.x = max.x;
		if (normal.y >= 0) p.y = max.y;
		if (normal.z >= 0) p.z = max.z;

		return p;
	}

	glm::vec3 GetNegativeVertex(const glm::vec3& normal) const
	{
		glm::vec3 n = max;
		if (normal.x >= 0) n.x = min.x;
		if (normal.y >= 0) n.y = min.y;
		if (normal.z >= 0) n.z = min.z;

		return n;
	}

	AABB Transform(const glm::mat4& transform) const 
	{
		// Model transforms are affine. Transforming center/extents is exact for
		// an AABB and avoids eight matrix multiplies and homogeneous divides.
		const glm::vec3 center = GetCenter();
		const glm::vec3 extents = GetExtents();
		const glm::mat3 linear(transform);
		const glm::mat3 absLinear(
			glm::abs(linear[0]),
			glm::abs(linear[1]),
			glm::abs(linear[2]));
		const glm::vec3 transformedCenter =
			glm::vec3(transform * glm::vec4(center, 1.0f));
		const glm::vec3 transformedExtents = absLinear * extents;

		return AABB(
			transformedCenter - transformedExtents,
			transformedCenter + transformedExtents);
	}
};

class Frustum {
public:
    enum class PlaneIndex {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count
    };

    static constexpr int PLANE_COUNT = 6;

    Frustum() = default;

    // Extract frustum planes from view-projection matrix
    void Update(const glm::mat4& viewProjection) {
        // Left plane
        planes[0].normal.x = viewProjection[0][3] + viewProjection[0][0];
        planes[0].normal.y = viewProjection[1][3] + viewProjection[1][0];
        planes[0].normal.z = viewProjection[2][3] + viewProjection[2][0];
        planes[0].distance = viewProjection[3][3] + viewProjection[3][0];

        // Right plane
        planes[1].normal.x = viewProjection[0][3] - viewProjection[0][0];
        planes[1].normal.y = viewProjection[1][3] - viewProjection[1][0];
        planes[1].normal.z = viewProjection[2][3] - viewProjection[2][0];
        planes[1].distance = viewProjection[3][3] - viewProjection[3][0];

        // Bottom plane
        planes[2].normal.x = viewProjection[0][3] + viewProjection[0][1];
        planes[2].normal.y = viewProjection[1][3] + viewProjection[1][1];
        planes[2].normal.z = viewProjection[2][3] + viewProjection[2][1];
        planes[2].distance = viewProjection[3][3] + viewProjection[3][1];

        // Top plane
        planes[3].normal.x = viewProjection[0][3] - viewProjection[0][1];
        planes[3].normal.y = viewProjection[1][3] - viewProjection[1][1];
        planes[3].normal.z = viewProjection[2][3] - viewProjection[2][1];
        planes[3].distance = viewProjection[3][3] - viewProjection[3][1];

        // Near plane
        planes[4].normal.x = viewProjection[0][3] + viewProjection[0][2];
        planes[4].normal.y = viewProjection[1][3] + viewProjection[1][2];
        planes[4].normal.z = viewProjection[2][3] + viewProjection[2][2];
        planes[4].distance = viewProjection[3][3] + viewProjection[3][2];

        // Far plane
        planes[5].normal.x = viewProjection[0][3] - viewProjection[0][2];
        planes[5].normal.y = viewProjection[1][3] - viewProjection[1][2];
        planes[5].normal.z = viewProjection[2][3] - viewProjection[2][2];
        planes[5].distance = viewProjection[3][3] - viewProjection[3][2];

        // Normalize all planes
        for (int i = 0; i < PLANE_COUNT; ++i) {
            planes[i].Normalize();
        }
    }

    // Test AABB against frustum (returns true if visible)
    bool IsBoxVisible(const AABB& box, float tolerance = 0.5f) const 
    {
		const glm::vec3 center = box.GetCenter();
		const glm::vec3 extents = box.GetExtents();
        for (int i = 0; i < PLANE_COUNT; ++i) 
        {
			const Plane& plane = planes[i];
			const float projectedRadius =
				glm::dot(glm::abs(plane.normal), extents);
            if (plane.GetSignedDistanceToPoint(center) + projectedRadius < -tolerance)
            {
                return false;
            }
        }
        return true;
    }

    // More conservative test - checks if box is completely outside
    bool IsBoxCompletelyOutside(const AABB& box) const 
    {
        for (int i = 0; i < PLANE_COUNT; ++i)
        {
            glm::vec3 positiveVertex = box.GetPositiveVertex(planes[i].normal);
            if (planes[i].GetSignedDistanceToPoint(positiveVertex) < 0) 
            {
                return true;
            }
        }
        return false;
    }

    // Test sphere against frustum
    bool IsSphereVisible(const glm::vec3& center, float radius) const
    {
        for (int i = 0; i < PLANE_COUNT; ++i) 
        {
            float distance = planes[i].GetSignedDistanceToPoint(center);
            if (distance < -radius) 
            {
                return false;
            }
        }
        return true;
    }

    const Plane& GetPlane(PlaneIndex index) const 
    {
        return planes[static_cast<int>(index)];
    }

private:
    std::array<Plane, PLANE_COUNT> planes;
};
