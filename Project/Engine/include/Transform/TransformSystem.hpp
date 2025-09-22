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

	void Update();
	void UpdateTransform(Entity entity);
	void TraverseHierarchy(Entity entity, std::function<void(Entity)> updateTransform);
	Matrix4x4 CalculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation);

	void SetWorldPosition(Entity entity, Vector3D position);
	void SetLocalPosition(Entity entity, Vector3D position);

	void SetWorldRotation(Entity entity, Vector3D rotation);
	void SetLocalRotation(Entity entity, Vector3D rotation);

	void SetWorldScale(Entity entity, Vector3D scale);
	void SetLocalScale(Entity entity, Vector3D scale);

	void SetDirtyRecursive(Entity entity);
	Transform& GetRootParentTransform(Entity currentEntity);

	// Extract translation, scale, rotation from matrix
	static Vector3D ExtractTranslation(const Matrix4x4& m);
	static Vector3D ExtractScale(const Matrix4x4& m);
	static Vector3D ExtractRotation(const Matrix4x4& m);

	static Matrix4x4 RemoveScale(const Matrix4x4& m);
};