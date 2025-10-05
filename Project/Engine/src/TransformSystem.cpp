#include "pch.h"
#include "Transform/TransformComponent.hpp"
#include "Transform/TransformSystem.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include <Hierarchy/ChildrenComponent.hpp>
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include <Math/Matrix3x3.hpp>
#include "Performance/PerformanceProfiler.hpp"

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
	PROFILE_FUNCTION(); // Will automatically show as "Transform" in profiler UI
	
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
			auto& guidRegistry = EntityGUIDRegistry::GetInstance();
			auto& parentTransform = ecsManager.GetComponent<Transform>(guidRegistry.GetEntityByGUID(parentCompOpt->get().parent));
			//auto& rootParentTransform = GetRootParentTransform(entity);
			//transform.worldMatrix = parentTransform.worldMatrix * CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation);
			
			//Matrix4x4 parentNoScale = RemoveScale(parentTransform.worldMatrix);
			//Vector3D effectiveScale = parentTransform.localScale * transform.localScale;
			Matrix4x4 localMatrix = CalculateModelMatrix(
				transform.localPosition,   // unaffected by parent scale
				transform.localScale,
				transform.localRotation.ToEulerDegrees()
			);

			transform.worldMatrix = parentTransform.worldMatrix * localMatrix;
		}
		else {
			transform.worldMatrix = CalculateModelMatrix(transform.localPosition, transform.localScale, transform.localRotation.ToEulerDegrees());
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
		auto& guidRegistry = EntityGUIDRegistry::GetInstance();
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(entity);
		for (const auto& childGUID : childrenComp.children) {
			Entity child = guidRegistry.GetEntityByGUID(childGUID);
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
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
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
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		Matrix4x4 parentNoScale = Matrix4x4::RemoveScale(parentTransform.worldMatrix);
		Quaternion parentWorldRot = Quaternion::FromMatrix(parentNoScale);
		Quaternion desiredWorldRot = Quaternion::FromEulerDegrees(rotation);

		transform.localRotation = parentWorldRot.Inverse() * desiredWorldRot;
	}
	else {
		transform.localRotation = Quaternion::FromEulerDegrees(rotation);
	}

	SetDirtyRecursive(entity);
}

void TransformSystem::SetLocalRotation(Entity entity, Vector3D rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.localRotation = Quaternion::FromEulerDegrees(rotation);

	SetDirtyRecursive(entity);
}

void TransformSystem::SetWorldScale(Entity entity, Vector3D scale) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		Vector3D parentScale = Matrix4x4::ExtractScale(parentTransform.worldMatrix);
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
		for (const auto& childGUID : children) {
			Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);
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
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(currentEntity).parent);
		return GetRootParentTransform(parent);
	}
}