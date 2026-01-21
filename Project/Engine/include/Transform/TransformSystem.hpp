#pragma once
#include <memory>
#include "ECS/System.hpp"
#include "Math/Matrix4x4.hpp"
#include "TransformComponent.hpp"
#include "../Engine.h"  // For ENGINE_API macro

# define M_PI           3.14159265358979323846f

class TransformSystem : public System {
public:
	TransformSystem() = default;
	~TransformSystem() = default;

	void Initialise();

	void Update();
	void ENGINE_API UpdateTransform(Entity entity);
	void TraverseHierarchy(Entity entity, std::function<void(Entity)> updateTransform);
	static Matrix4x4 CalculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation);

	void ENGINE_API SetWorldPosition(Entity entity, Vector3D position);
	void ENGINE_API SetLocalPosition(Entity entity, Vector3D position);

	void ENGINE_API SetWorldRotation(Entity entity, Vector3D rotation);
	void ENGINE_API SetLocalRotation(Entity entity, Vector3D rotation);
	void ENGINE_API SetLocalRotation(Entity entity, Quaternion rotation);

	void ENGINE_API SetWorldScale(Entity entity, Vector3D scale);
	void ENGINE_API SetLocalScale(Entity entity, Vector3D scale);

	Vector3D& GetWorldPosition(Entity entity);
	Vector3D& GetWorldRotation(Entity entity);
	Vector3D& GetWorldScale(Entity entity);

	void SetDirtyRecursive(Entity entity);
	Transform& GetRootParentTransform(Entity currentEntity);

	std::vector<Entity> ENGINE_API GetAllChildEntitiesVector(Entity parentEntity);
	std::set<Entity> ENGINE_API GetAllChildEntitiesSet(Entity parentEntity);
};