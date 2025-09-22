#pragma once
#include <memory>
#include "ECS/System.hpp"
#include "Math/Matrix4x4.hpp"
#include "TransformComponent.hpp"
#include "../Engine.h"  // For ENGINE_API macro

# define M_PI           3.14159265358979323846f

class ENGINE_API TransformSystem : public System {
public:
	TransformSystem() = default;
	~TransformSystem() = default;

	void Initialise();

	void update();
	static Matrix4x4 calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation);

	static void SetPosition(Transform& transform, Vector3D position);
	static void SetRotation(Transform& transform, Vector3D rotation);
	static void SetScale(Transform& transform, Vector3D scale);
};