#pragma once
#include "glm/glm.hpp"
#include <array>

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
		glm::vec3 corners[8] = {
			glm::vec3(min.x, min.y, min.z),
			glm::vec3(max.x, min.y, min.z),
			glm::vec3(min.x, max.y, min.z),
			glm::vec3(max.x, max.y, min.z),
			glm::vec3(min.x, min.y, max.z),
			glm::vec3(max.x, min.y, max.z),
			glm::vec3(min.x, max.y, max.z),
			glm::vec3(max.x, max.y, max.z)
		};

		glm::vec3 newMin(FLT_MAX);
		glm::vec3 newMax(-FLT_MAX);

		for (int i = 0; i < 8; ++i) 
		{
			glm::vec4 transformed = transform * glm::vec4(corners[i], 1.0f);
			glm::vec3 point = glm::vec3(transformed) / transformed.w;

			newMin = glm::min(newMin, point);
			newMax = glm::max(newMax, point);
		}

		return AABB(newMin, newMax);
	}
};

class Frustum {
public:
	enum PlaneIndex {
		LEFT = 0,
		RIGHT,
		BOTTOM,
		TOP,
		NEAR,
		FAR,
		PLANE_COUNT
	};

	Frustum() = default;

	void Update(glm::mat4& viewProjection)
	{
		// Left plane
		planes[LEFT].normal.x = viewProjection[0][3] + viewProjection[0][0];
		planes[LEFT].normal.y = viewProjection[1][3] + viewProjection[1][0];
		planes[LEFT].normal.z = viewProjection[2][3] + viewProjection[2][0];
		planes[LEFT].distance = viewProjection[3][3] + viewProjection[3][0];

		// Right plane
		planes[RIGHT].normal.x = viewProjection[0][3] - viewProjection[0][0];
		planes[RIGHT].normal.y = viewProjection[1][3] - viewProjection[1][0];
		planes[RIGHT].normal.z = viewProjection[2][3] - viewProjection[2][0];
		planes[RIGHT].distance = viewProjection[3][3] - viewProjection[3][0];

		// Bottom plane
		planes[BOTTOM].normal.x = viewProjection[0][3] + viewProjection[0][1];
		planes[BOTTOM].normal.y = viewProjection[1][3] + viewProjection[1][1];
		planes[BOTTOM].normal.z = viewProjection[2][3] + viewProjection[2][1];
		planes[BOTTOM].distance = viewProjection[3][3] + viewProjection[3][1];

		// Top plane
		planes[TOP].normal.x = viewProjection[0][3] - viewProjection[0][1];
		planes[TOP].normal.y = viewProjection[1][3] - viewProjection[1][1];
		planes[TOP].normal.z = viewProjection[2][3] - viewProjection[2][1];
		planes[TOP].distance = viewProjection[3][3] - viewProjection[3][1];

		// Near plane
		planes[NEAR].normal.x = viewProjection[0][3] + viewProjection[0][2];
		planes[NEAR].normal.y = viewProjection[1][3] + viewProjection[1][2];
		planes[NEAR].normal.z = viewProjection[2][3] + viewProjection[2][2];
		planes[NEAR].distance = viewProjection[3][3] + viewProjection[3][2];

		// Far plane
		planes[FAR].normal.x = viewProjection[0][3] - viewProjection[0][2];
		planes[FAR].normal.y = viewProjection[1][3] - viewProjection[1][2];
		planes[FAR].normal.z = viewProjection[2][3] - viewProjection[2][2];
		planes[FAR].distance = viewProjection[3][3] - viewProjection[3][2];

		// Normalize all planes
		for (int i = 0; i < PLANE_COUNT; ++i) {
			planes[i].Normalize();
		}
	}

private:
	std::array<Plane, PLANE_COUNT> planes;
};