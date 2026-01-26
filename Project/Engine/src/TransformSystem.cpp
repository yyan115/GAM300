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
#include <ECS/NameComponent.hpp>

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
		// Automatically set the child transforms as dirty, as the parent transform has been modified.
		if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
			SetDirtyRecursive(entity);
		}

		auto parentCompOpt = ecsManager.TryGetComponent<ParentComponent>(entity);
		// If the entity has a parent
		if (parentCompOpt.has_value()) {
			auto& guidRegistry = EntityGUIDRegistry::GetInstance();
			GUID_128 parentGUID = parentCompOpt->get().parent;
			Entity parentEntity = guidRegistry.GetEntityByGUID(parentGUID);
			if (parentEntity == static_cast<Entity>(-1)) {
				std::cerr << "[TransformSystem] ERROR: Entity '" << ecsManager.GetComponent<NameComponent>(entity).name
					<< "' (" << entity << ") has invalid parent GUID: "
					<< GUIDUtilities::ConvertGUID128ToString(parentGUID) << "\n";
			}
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

		transform.worldPosition = Matrix4x4::ExtractTranslation(transform.worldMatrix);
		transform.worldRotation = Matrix4x4::ExtractRotation(transform.worldMatrix);
		transform.worldScale = Matrix4x4::ExtractScale(transform.worldMatrix);

		transform.isDirty = false;
	}

	//// Update the last known values
	//transform.lastPosition = transform.localPosition;
	//transform.lastRotation = transform.localRotation;
	//transform.lastScale = transform.localScale;
}

void TransformSystem::TraverseHierarchy(Entity entity, std::function<void(Entity)> updateTransform) {
	updateTransform(entity);

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
		auto& guidRegistry = EntityGUIDRegistry::GetInstance();
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(entity);

		//std::string entityName = ecsManager.HasComponent<NameComponent>(entity)
		//	? ecsManager.GetComponent<NameComponent>(entity).name
		//	: "Unnamed";

		for (const auto& childGUID : childrenComp.children) {
			Entity child = guidRegistry.GetEntityByGUID(childGUID);
			if (child == static_cast<Entity>(-1)) {
				//std::cerr << "[TransformSystem] ERROR: Entity '" << entityName
				//	<< "' (" << entity << ") has invalid child GUID: "
				//	<< GUIDUtilities::ConvertGUID128ToString(childGUID) << "\n";
				continue; // Skip this invalid child
			}
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

void TransformSystem::SetLocalRotation(Entity entity, Quaternion rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);

	transform.localRotation = rotation; // Direct assignment
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

Vector3D& TransformSystem::GetWorldPosition(Entity entity) {
	auto& transform = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<Transform>(entity);
	return transform.worldPosition;
}

Vector3D& TransformSystem::GetWorldRotation(Entity entity) {
	auto& transform = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<Transform>(entity);
	return transform.worldRotation;
}

Vector3D& TransformSystem::GetWorldScale(Entity entity) {
	auto& transform = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<Transform>(entity);
	return transform.worldScale;
}

void TransformSystem::SetDirtyRecursive(Entity entity) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	//// --- DEBUG LOG START: Current Entity ---
	//std::string entityName = "Unnamed";
	//if (ecsManager.HasComponent<NameComponent>(entity)) {
	//	entityName = ecsManager.GetComponent<NameComponent>(entity).name;
	//}

	//// Print: [TransformSystem] Setting Dirty: EntityID 5 [Goblin_Root]
	//std::cout << "[TransformSystem] Setting Dirty: EntityID " << static_cast<uint64_t>(entity)
	//	<< " [" << entityName << "]" << std::endl;
	//// --- DEBUG LOG END ---

	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.isDirty = true;

	if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
		auto& children = ecsManager.GetComponent<ChildrenComponent>(entity).children;
		for (const auto& childGUID : children) {
			Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);

			//// --- DEBUG LOG START: Recursion Info ---
			//std::string childName = "Unnamed";
			//if (ecsManager.HasComponent<NameComponent>(child)) {
			//	childName = ecsManager.GetComponent<NameComponent>(child).name;
			//}

			//// Print with indentation to show hierarchy depth
			////     -> Recursing to Child: EntityID 6 [Goblin_Arm]
			//std::cout << "    -> Recursing to Child: EntityID " << static_cast<uint64_t>(child)
			//	<< " [" << childName << "]" << std::endl;
			//// --- DEBUG LOG END ---

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

std::vector<Entity> TransformSystem::GetAllChildEntitiesVector(Entity parentEntity) {
	std::vector<Entity> allChildEntities;
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(parentEntity)) {
		auto& guidRegistry = EntityGUIDRegistry::GetInstance();
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(parentEntity);
		for (const auto& childGUID : childrenComp.children) {
			Entity child = guidRegistry.GetEntityByGUID(childGUID);
			allChildEntities.push_back(child);
			// Recursively get grandchildren
			auto grandchildren = GetAllChildEntitiesVector(child);
			allChildEntities.insert(allChildEntities.end(), grandchildren.begin(), grandchildren.end());
		}
	}

	return allChildEntities;
}

std::set<Entity> TransformSystem::GetAllChildEntitiesSet(Entity parentEntity) {
	std::set<Entity> allChildEntities;
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(parentEntity)) {
		auto& guidRegistry = EntityGUIDRegistry::GetInstance();
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(parentEntity);
		for (const auto& childGUID : childrenComp.children) {
			Entity child = guidRegistry.GetEntityByGUID(childGUID);
			allChildEntities.insert(child);
			// Recursively get grandchildren
			auto grandchildren = GetAllChildEntitiesSet(child);
			allChildEntities.insert(grandchildren.begin(), grandchildren.end());
		}
	}

	return allChildEntities;
}
