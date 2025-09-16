#pragma once
#include <memory>
#include "ECS/System.hpp"
#include "Math/Matrix4x4.h"

# define M_PI           3.14159265358979323846f

class TransformSystem : public System {
public:
	TransformSystem() = default;
	~TransformSystem() = default;

	void Initialise();

	void update();
	static Matrix4x4 calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation);
};