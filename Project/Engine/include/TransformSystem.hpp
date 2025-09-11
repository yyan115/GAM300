#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include "ECS/System.hpp"
#include "Math/Matrix4x4.h"

# define M_PI           3.14159265358979323846f

struct Transform {
	Vector3D position;
	Vector3D scale = { 1, 1, 1 };
	Vector3D rotation = { 0, 0, 0 };

	Vector3D lastPosition;
	Vector3D lastScale;
	Vector3D lastRotation;

	//Matrix4x4 model;
	glm::mat4 model;
};

class TransformSystem : public System {
public:
	TransformSystem() = default;
	~TransformSystem() = default;

	void Initialise();

	void update();
	//static Matrix4x4 calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation);
	static glm::mat4 calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation);
};