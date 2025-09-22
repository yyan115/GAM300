#include "pch.h"
#include "Transform/TransformComponent.hpp"
#include "Transform/TransformSystem.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include <Hierarchy/ChildrenComponent.hpp>
#include <Math/Matrix3x3.hpp>

void TransformSystem::Initialise() {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	// Update entities' transform starting from root entities, in a depth-first manner.
	for (const auto& entity : entities) {
		if (!ecsManager.HasComponent<ParentComponent>(entity)) {
			TraverseHierarchy(entity, [this](Entity _entity) {
				UpdateTransform(_entity);
			});
		}
	}

	//for (const auto& entity : entities) {
	//	auto& transform = ecsManager.GetComponent<Transform>(entity);
	//	// Update model matrix
	//	transform.model = CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation);

	//	// Update the last known values
	//	transform.lastPosition = transform.localPosition;
	//	transform.lastRotation = transform.localRotation;
	//	transform.lastScale = transform.localScale;
	//}
}

void TransformSystem::Update() {
	//for (auto& [entities, transform] : transformSystem.forEach()) {
	//ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	//for (const auto& entity : entities) {
	//	auto& transform = ecsManager.GetComponent<Transform>(entity);

	//	// Update model matrix only if there is a change
	//	if (transform.localPosition != transform.lastPosition || transform.localScale != transform.lastScale || transform.localRotation != transform.lastRotation) {
	//		auto parentCompOpt = ecsManager.TryGetComponent<ParentComponent>(entity);
	//		// If the entity has a parent
	//		if (parentCompOpt.has_value()) {
	//			auto& parentTransform = ecsManager.GetComponent<Transform>(parentCompOpt->get().parent);
	//			transform.model = parentTransform.model * CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation);
	//		}
	//		else {
	//			transform.model = CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation);
	//		}
	//	}

	//	// Update the last known values
	//	transform.lastPosition = transform.localPosition;
	//	transform.lastRotation = transform.localRotation;
	//	transform.lastScale = transform.localScale;
	//}

	// Update entities' transform starting from root entities, in a depth-first manner.
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	// Update entities' transform starting from root entities, in a depth-first manner.
	for (const auto& entity : entities) {
		if (!ecsManager.HasComponent<ParentComponent>(entity)) {
			TraverseHierarchy(entity, [this](Entity _entity) {
				UpdateTransform(_entity);
			});
		}
	}
}

void TransformSystem::UpdateTransform(Entity entity) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& transform = ecsManager.GetComponent<Transform>(entity);

	// Update model matrix only if there is a change
	if (transform.isDirty) {
		auto parentCompOpt = ecsManager.TryGetComponent<ParentComponent>(entity);
		// If the entity has a parent
		if (parentCompOpt.has_value()) {
			auto& parentTransform = ecsManager.GetComponent<Transform>(parentCompOpt->get().parent);
			//auto& rootParentTransform = GetRootParentTransform(entity);
			//transform.worldMatrix = parentTransform.worldMatrix * CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation);
			
			//Matrix4x4 parentNoScale = RemoveScale(parentTransform.worldMatrix);
			//Vector3D effectiveScale = parentTransform.localScale * transform.localScale;
			Matrix4x4 localMatrix = CalculateModelMatrix(
				transform.localPosition,   // unaffected by parent scale
				transform.localScale,
				transform.localRotation
			);

			transform.worldMatrix = parentTransform.worldMatrix * localMatrix;
		}
		else {
			transform.worldMatrix = CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation);
		}

		transform.isDirty = false;
	}

	//// Update the last known values
	//transform.lastPosition = transform.localPosition;
	//transform.lastRotation = transform.localRotation;
	//transform.lastScale = transform.localScale;
}

void TransformSystem::TraverseHierarchy(Entity entity, std::function<void(Entity)> updateTransform) {
	// Update its own transform first.
	updateTransform(entity);

	// Then traverse children.
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(entity);
		for (const auto& child : childrenComp.children) {
			TraverseHierarchy(child, updateTransform);
		}
	}
}

Matrix4x4 TransformSystem::CalculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation) {
	float radx = rotation.x * (M_PI / 180.f);
	float rady = rotation.y * (M_PI / 180.f);
	float radz = rotation.z * (M_PI / 180.f);

	//  TRS = T * R * S  (column-major, column vectors)
	Matrix4x4 R = Matrix4x4::RotationZ(radz) * Matrix4x4::RotationY(rady) * Matrix4x4::RotationX(radx);

	return Matrix4x4::TRS(position, R, scale);
}

void TransformSystem::SetWorldPosition(Entity entity, Vector3D position) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = ecsManager.GetComponent<ParentComponent>(entity).parent;
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);
		
		// Convert world to local.
		Matrix4x4 invParent = parentTransform.worldMatrix.Inversed();
		transform.localPosition = invParent.TransformPoint(position);
	}
	else {
		transform.localPosition = position;
	}

	SetDirtyRecursive(entity);
}

void TransformSystem::SetLocalPosition(Entity entity, Vector3D position) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.localPosition = position;

	SetDirtyRecursive(entity);
}

void TransformSystem::SetWorldRotation(Entity entity, Vector3D rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = ecsManager.GetComponent<ParentComponent>(entity).parent;
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		float radx = rotation.x * (M_PI / 180.f);
		float rady = rotation.y * (M_PI / 180.f);
		float radz = rotation.z * (M_PI / 180.f);
		Matrix4x4 desiredR = Matrix4x4::RotationZ(radz)
			* Matrix4x4::RotationY(rady)
			* Matrix4x4::RotationX(radx);
		Vector3D parentRotation = ExtractRotation(parentTransform.worldMatrix);
		float p_radx = parentRotation.x * (M_PI / 180.f);
		float p_rady = parentRotation.y * (M_PI / 180.f);
		float p_radz = parentRotation.z * (M_PI / 180.f);
		Matrix4x4 parentR = Matrix4x4::RotationZ(p_radz)
			* Matrix4x4::RotationY(p_rady)
			* Matrix4x4::RotationX(p_radx);
		Matrix4x4 localR = parentR.Inversed() * desiredR;
		Vector3D rotationRad = ExtractRotation(localR);
		transform.localRotation = rotationRad * (180.0f / M_PI);
	}
	else {
		transform.localRotation = rotation;
	}

	SetDirtyRecursive(entity);
}

void TransformSystem::SetLocalRotation(Entity entity, Vector3D rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.localRotation = rotation;

	SetDirtyRecursive(entity);
}

void TransformSystem::SetWorldScale(Entity entity, Vector3D scale) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = ecsManager.GetComponent<ParentComponent>(entity).parent;
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		Vector3D parentScale = ExtractScale(parentTransform.worldMatrix);
		transform.localScale = scale / parentScale;
	}
	else {
		transform.localScale = scale;
	}

	SetDirtyRecursive(entity);
}

void TransformSystem::SetLocalScale(Entity entity, Vector3D scale) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.localScale = scale;

	SetDirtyRecursive(entity);
}

void TransformSystem::SetDirtyRecursive(Entity entity) {
	ECSManager & ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.isDirty = true;

	if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
		auto& children = ecsManager.GetComponent<ChildrenComponent>(entity).children;
		for (Entity child : children) {
			SetDirtyRecursive(child);
		}
	}
}

Transform& TransformSystem::GetRootParentTransform(Entity currentEntity) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (!ecsManager.HasComponent<ParentComponent>(currentEntity)) {
		return ecsManager.GetComponent<Transform>(currentEntity);
	}
	else {
		Entity parent = ecsManager.GetComponent<ParentComponent>(currentEntity).parent;
		return GetRootParentTransform(parent);
	}
}

Vector3D TransformSystem::ExtractTranslation(const Matrix4x4& m) {
	// Assuming row-major layout
	Vector3D localPosition = Vec3(m[3][0], m[3][1], m[3][2]);
	return localPosition;
}

Vector3D TransformSystem::ExtractScale(const Matrix4x4& m) {
	Vector3D xAxis = Vector3D(m[0][0], m[0][1], m[0][2]);
	Vector3D yAxis = Vector3D(m[1][0], m[1][1], m[1][2]);
	Vector3D zAxis = Vector3D(m[2][0], m[2][1], m[2][2]);

	Vector3D localScale = Vector3D(xAxis.Length(), yAxis.Length(), zAxis.Length());
	return localScale;
}

Vector3D TransformSystem::ExtractRotation(const Matrix4x4& m) {
	Vector3D xAxis = Vector3D(m[0][0], m[0][1], m[0][2]).Normalize();
	Vector3D yAxis = Vector3D(m[1][0], m[1][1], m[1][2]).Normalize();
	Vector3D zAxis = Vector3D(m[2][0], m[2][1], m[2][2]).Normalize();

	Matrix3x3 rotMat(xAxis.x, xAxis.y, xAxis.z,
		yAxis.x, yAxis.y, yAxis.z,
		zAxis.x, zAxis.y, zAxis.z);

	float pitch = -std::asin(rotMat[1][2]);
	float yaw = std::atan2(rotMat[0][2], rotMat[2][2]);
	float roll = std::atan2(rotMat[1][0], rotMat[1][1]);

	return Vector3D(pitch, yaw, roll); // in radians
}

Matrix4x4 TransformSystem::RemoveScale(const Matrix4x4& m) {
	// Extract basis vectors from columns
	Vector3D xAxis(m[0][0], m[1][0], m[2][0]);
	Vector3D yAxis(m[0][1], m[1][1], m[2][1]);
	Vector3D zAxis(m[0][2], m[1][2], m[2][2]);

	// Normalize to remove scale
	xAxis = xAxis.Normalize();
	yAxis = yAxis.Normalize();
	zAxis = zAxis.Normalize();

	// Extract translation from last column
	Vector3D translation(m[0][3], m[1][3], m[2][3]);

	Matrix4x4 result = Matrix4x4::Identity();
	result[0][0] = xAxis.x; result[1][0] = xAxis.y; result[2][0] = xAxis.z;
	result[0][1] = yAxis.x; result[1][1] = yAxis.y; result[2][1] = yAxis.z;
	result[0][2] = zAxis.x; result[1][2] = zAxis.y; result[2][2] = zAxis.z;

	result[0][3] = translation.x;
	result[1][3] = translation.y;
	result[2][3] = translation.z;

	return result;
}

