#include "pch.h"
#include "Transform/TransformComponent.hpp"
#include "Transform/TransformSystem.hpp"
#include "Logging.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include <Hierarchy/ChildrenComponent.hpp>
#include "Hierarchy/EntityGUIDRegistry.hpp"

namespace {
bool QuaternionEquals(const Quaternion& lhs, const Quaternion& rhs)
{
	return lhs.w == rhs.w && lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

const Matrix4x4& GetLocalMatrix(Transform& transform)
{
	const bool cacheMatches = transform.cachedLocalMatrixValid &&
		transform.cachedLocalPosition == transform.localPosition &&
		transform.cachedLocalScale == transform.localScale &&
		QuaternionEquals(
			transform.cachedLocalRotation, transform.localRotation);

	if (!cacheMatches) {
		transform.cachedLocalMatrix = TransformSystem::CalculateModelMatrix(
			transform.localPosition,
			transform.localScale,
			transform.localRotation);
		transform.cachedLocalPosition = transform.localPosition;
		transform.cachedLocalScale = transform.localScale;
		transform.cachedLocalRotation = transform.localRotation;
		transform.cachedLocalMatrixValid = true;
	}

	return transform.cachedLocalMatrix;
}

void UpdateWorldTransform(Transform& transform, const Transform* parentTransform)
{
	const Matrix4x4& localMatrix = GetLocalMatrix(transform);

	if (parentTransform) {
		transform.worldMatrix = Matrix4x4::MultiplyAffine(
			parentTransform->worldMatrix, localMatrix);
		transform.worldPosition = Matrix4x4::ExtractTranslation(transform.worldMatrix);
		transform.worldScale = Matrix4x4::ExtractScale(transform.worldMatrix);
		transform.worldRotation = parentTransform->worldRotation * transform.localRotation;
		transform.worldRotation.Normalize();
		++transform.worldRevision;
		return;
	}

	transform.worldMatrix = localMatrix;
	transform.worldPosition = transform.localPosition;
	transform.worldScale = {
		std::abs(transform.localScale.x),
		std::abs(transform.localScale.y),
		std::abs(transform.localScale.z)
	};
	transform.worldRotation = transform.localRotation;
	transform.worldRotation.Normalize();
	++transform.worldRevision;
}

void UpdateTransformRecursive(
	ECSManager& ecs,
	EntityGUIDRegistry& guidRegistry,
	Entity entity,
	const Transform* parentTransform,
	bool parentChanged)
{
	auto& transform = ecs.GetComponent<Transform>(entity);
	const bool needsUpdate = transform.isDirty || parentChanged;

	if (needsUpdate) {
		UpdateWorldTransform(transform, parentTransform);
		transform.isDirty = false;
	}

	if (auto children = ecs.TryGetComponent<ChildrenComponent>(entity)) {
		for (const Entity child : children->get().ResolveEntities()) {
			if (child != static_cast<Entity>(-1)) {
				UpdateTransformRecursive(
					ecs, guidRegistry, child, &transform, needsUpdate);
			}
		}
	}
}

const Transform* GetParentTransform(
	ECSManager& ecs,
	EntityGUIDRegistry& guidRegistry,
	Entity entity)
{
	auto parentComponent = ecs.TryGetComponent<ParentComponent>(entity);
	if (!parentComponent) {
		return nullptr;
	}

	const Entity parent =
		guidRegistry.GetEntityByGUID(parentComponent->get().parent);
	if (parent == static_cast<Entity>(-1) || parent >= MAX_ENTITIES) {
		return nullptr;
	}
	auto parentTransform = ecs.TryGetComponent<Transform>(parent);
	return parentTransform ? &parentTransform->get() : nullptr;
}

bool HasDirtyTransformAncestor(
	ECSManager& ecs,
	EntityGUIDRegistry& guidRegistry,
	Entity entity,
	std::vector<std::int8_t>& cache,
	std::vector<Entity>& pathScratch,
	std::vector<Entity>& touchedScratch)
{
	constexpr std::int8_t Visiting = -2;

	pathScratch.clear();
	Entity current = entity;
	bool hasDirtyAncestor = true;
	for (Entity depth = 0; depth < MAX_ENTITIES; ++depth) {
		if (current >= MAX_ENTITIES) {
			hasDirtyAncestor = false;
			break;
		}

		const std::int8_t cachedResult = cache[current];
		if (cachedResult >= 0) {
			hasDirtyAncestor = cachedResult != 0;
			break;
		}
		if (cachedResult == Visiting) {
			// A valid hierarchy is acyclic. Conservatively suppress duplicate
			// traversal if malformed parent data loops back into this path.
			hasDirtyAncestor = true;
			break;
		}

		cache[current] = Visiting;
		touchedScratch.push_back(current);
		pathScratch.push_back(current);

		auto parentComponent = ecs.TryGetComponent<ParentComponent>(current);
		if (!parentComponent) {
			hasDirtyAncestor = false;
			break;
		}

		const Entity parent =
			guidRegistry.GetEntityByGUID(parentComponent->get().parent);
		if (parent == static_cast<Entity>(-1) || parent >= MAX_ENTITIES) {
			hasDirtyAncestor = false;
			break;
		}

		auto parentTransform = ecs.TryGetComponent<Transform>(parent);
		if (parentTransform && parentTransform->get().isDirty) {
			hasDirtyAncestor = true;
			break;
		}
		current = parent;
	}

	const std::int8_t result = hasDirtyAncestor ? 1 : 0;
	for (const Entity pathEntity : pathScratch) {
		cache[pathEntity] = result;
	}
	return hasDirtyAncestor;
}
}

void TransformSystem::Initialise() {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& guidRegistry = EntityGUIDRegistry::GetInstance();
	rootEntitiesScratch.clear();
	rootEntitiesScratch.reserve(entities.size() / 4);
	dirtyAncestorCache.assign(MAX_ENTITIES, -1);
	dirtyAncestorPathScratch.clear();
	dirtyAncestorPathScratch.reserve(32);
	dirtyAncestorTouchedScratch.clear();
	dirtyAncestorTouchedScratch.reserve(entities.size());

	for (const auto& entity : entities) {
		if (!ecsManager.HasComponent<ParentComponent>(entity)) {
			rootEntitiesScratch.push_back(entity);
		}
	}

	// Force every hierarchy to initialize even if a serialized dirty flag was
	// cleared. Physics initialization immediately consumes these world values.
	for (const Entity root : rootEntitiesScratch) {
		UpdateTransformRecursive(ecsManager, guidRegistry, root, nullptr, true);
	}

	isInitialised = true;
}

void TransformSystem::Update() {
	PROFILE_FUNCTION(); // Will automatically show as "Transform" in profiler UI

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& guidRegistry = EntityGUIDRegistry::GetInstance();

	dirtyRootsScratch.clear();
	{
		PROFILE_SCOPED("TS::CollectDirtyRoots");
		dirtyRootsScratch.reserve(entities.size() / 8);
		if (dirtyAncestorCache.size() != MAX_ENTITIES) {
			dirtyAncestorCache.assign(MAX_ENTITIES, -1);
			dirtyAncestorTouchedScratch.clear();
		}
		else {
			// Reset only entries reached by last frame's dirty transforms. A
			// blanket 50k-entry fill polluted caches even in mostly static scenes.
			for (const Entity cachedEntity : dirtyAncestorTouchedScratch) {
				dirtyAncestorCache[cachedEntity] = -1;
			}
			dirtyAncestorTouchedScratch.clear();
		}

		for (const auto& entity : entities) {
			// TransformSystem membership already guarantees this component.
			// Avoid constructing an optional for every scene transform each frame.
			const auto& transform = ecsManager.GetComponent<Transform>(entity);
			if (!transform.isDirty) {
				continue;
			}

			// A dirty ancestor will propagate its change through this entire
			// subtree, so processing the descendant separately would duplicate
			// matrix work.
			if (!HasDirtyTransformAncestor(
				ecsManager,
				guidRegistry,
				entity,
				dirtyAncestorCache,
				dirtyAncestorPathScratch,
				dirtyAncestorTouchedScratch)) {
				dirtyRootsScratch.push_back(entity);
			}
		}
	}

	{
		PROFILE_SCOPED("TS::RecursiveUpdate");
		for (const auto& root : dirtyRootsScratch) {
			UpdateTransformRecursive(
				ecsManager,
				guidRegistry,
				root,
				GetParentTransform(ecsManager, guidRegistry, root),
				false);
		}
	}
}

void TransformSystem::UpdateTransform(Entity entity) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& transform = ecsManager.GetComponent<Transform>(entity);

	if (isInitialised && !transform.isDirty) return;

	const Transform* parentTransform = nullptr;
	if (auto parentComp = ecsManager.TryGetComponent<ParentComponent>(entity)) {
		const Entity parentEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(
			parentComp->get().parent);
		if (parentEntity == static_cast<Entity>(-1)) {
			ENGINE_PRINT(
				EngineLogging::LogLevel::Error,
				"[TransformSystem] Entity ", entity, " has invalid parent GUID\n");
		}
		else {
			parentTransform = &ecsManager.GetComponent<Transform>(parentEntity);
		}
	}

	UpdateWorldTransform(transform, parentTransform);

	// Keep this dirty until the root-first update propagates the change to
	// descendants. Direct updates are used by editor and slider code.
}

// Internal helper for TraverseHierarchy with cycle detection
static void TraverseHierarchyInternal(Entity entity, std::function<void(Entity)>& updateTransform) {
	//// Cycle detection - prevent infinite recursion
	//if (visited.count(entity) > 0) {
	//	std::cerr << "[TransformSystem] ERROR: Circular hierarchy reference detected in TraverseHierarchy for entity " << entity << std::endl;
	//	return;
	//}
	//visited.insert(entity);

	updateTransform(entity);

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(entity);

		for (const Entity child : childrenComp.ResolveEntities()) {
			if (child == static_cast<Entity>(-1)) {
				continue; // Skip this invalid child
			}
			TraverseHierarchyInternal(child, updateTransform);
		}
	}
}

void TransformSystem::TraverseHierarchy(Entity entity, std::function<void(Entity)> updateTransform) {
	//std::set<Entity> visited;
	TraverseHierarchyInternal(entity, updateTransform);
}

Matrix4x4 TransformSystem::CalculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation) {
	float radx = rotation.x * (M_PI / 180.f);
	float rady = rotation.y * (M_PI / 180.f);
	float radz = rotation.z * (M_PI / 180.f);

	//  TRS = T * R * S  (column-major, column vectors)
	Matrix4x4 R = Matrix4x4::RotationZ(radz) * Matrix4x4::RotationY(rady) * Matrix4x4::RotationX(radx);

	return Matrix4x4::TRS(position, R, scale);
}

Matrix4x4 TransformSystem::CalculateModelMatrix(
	Vector3D const& position,
	Vector3D const& scale,
	Quaternion const& rotation)
{
	return Matrix4x4::TRS(position, rotation.ToMatrix(), scale);
}

void TransformSystem::SetWorldPosition(Entity entity, Vector3D position) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	Vector3D localPosition = position;

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);
		
		// Convert world to local.
		Matrix4x4 invParent = parentTransform.worldMatrix.Inversed();
		localPosition = invParent.TransformPoint(position);
	}

	if (transform.localPosition == localPosition) return;
	transform.localPosition = localPosition;
	transform.isDirty = true;
}

void TransformSystem::SetLocalPosition(Entity entity, Vector3D position) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	if (transform.localPosition == position) return;
	transform.localPosition = position;
	transform.isDirty = true;
}

void TransformSystem::SetWorldRotation(Entity entity, Vector3D rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	Quaternion localRotation;

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		Quaternion desiredWorldRot = Quaternion::FromEulerDegrees(rotation);

		localRotation = parentTransform.worldRotation.Inverse() * desiredWorldRot;
	}
	else {
		localRotation = Quaternion::FromEulerDegrees(rotation);
	}

	if (QuaternionEquals(transform.localRotation, localRotation)) return;
	transform.localRotation = localRotation;
	transform.isDirty = true;
}

void ENGINE_API TransformSystem::SetWorldRotation(Entity entity, Quaternion rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	Quaternion localRotation = rotation;

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		localRotation = parentTransform.worldRotation.Inverse() * rotation;
	}

	if (QuaternionEquals(transform.localRotation, localRotation)) return;
	transform.localRotation = localRotation;
	transform.isDirty = true;
}

void TransformSystem::SetLocalRotation(Entity entity, Vector3D rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	const Quaternion localRotation = Quaternion::FromEulerDegrees(rotation);
	if (QuaternionEquals(transform.localRotation, localRotation)) return;
	transform.localRotation = localRotation;
	transform.isDirty = true;
}

void TransformSystem::SetLocalRotation(Entity entity, Quaternion rotation) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);

	if (QuaternionEquals(transform.localRotation, rotation)) return;
	transform.localRotation = rotation;
	transform.isDirty = true;
}

void TransformSystem::SetWorldScale(Entity entity, Vector3D scale) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	Vector3D localScale = scale;

	if (ecsManager.HasComponent<ParentComponent>(entity)) {
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(entity).parent);
		Transform& parentTransform = ecsManager.GetComponent<Transform>(parent);

		// Convert world to local.
		localScale = scale / parentTransform.worldScale;
	}

	if (transform.localScale == localScale) return;
	transform.localScale = localScale;
	transform.isDirty = true;
}

void TransformSystem::SetLocalScale(Entity entity, Vector3D scale) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	if (transform.localScale == scale) return;
	transform.localScale = scale;
	transform.isDirty = true;
}

Vector3D& TransformSystem::GetWorldPosition(Entity entity) {
	auto& transform = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<Transform>(entity);
	return transform.worldPosition;
}

Quaternion& TransformSystem::GetWorldRotation(Entity entity) {
	auto& transform = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<Transform>(entity);
	return transform.worldRotation;
}

Vector3D& TransformSystem::GetWorldScale(Entity entity) {
	auto& transform = ECSRegistry::GetInstance().GetActiveECSManager().GetComponent<Transform>(entity);
	return transform.worldScale;
}

void TransformSystem::SetLocalTransform(Entity entity, const Vector3D& pos, const Quaternion& rot, const Vector3D& scale) {
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
	SetLocalTransform(ecs.GetComponent<Transform>(entity), pos, rot, scale);
}

void TransformSystem::SetLocalTransform(
	Transform& tr,
	const Vector3D& pos,
	const Quaternion& rot,
	const Vector3D& scale) {

	if (tr.localPosition == pos &&
		QuaternionEquals(tr.localRotation, rot) &&
		tr.localScale == scale) {
		return;
	}

	tr.localPosition = pos;
	tr.localRotation = rot;
	tr.localScale = scale;
	// The root-first transform traversal propagates parent changes to descendants.
	// Marking every descendant here made animation updates quadratic in bone count.
	tr.isDirty = true;
}

void TransformSystem::SetLocalTransform(
	Transform& tr,
	const Vector3D& pos,
	const Quaternion& rot,
	const Vector3D& scale,
	const Matrix4x4& localMatrix) {

	const bool changed = tr.localPosition != pos ||
		!QuaternionEquals(tr.localRotation, rot) ||
		tr.localScale != scale;

	tr.localPosition = pos;
	tr.localRotation = rot;
	tr.localScale = scale;
	tr.cachedLocalMatrix = localMatrix;
	tr.cachedLocalPosition = pos;
	tr.cachedLocalRotation = rot;
	tr.cachedLocalScale = scale;
	tr.cachedLocalMatrixValid = true;

	if (changed) {
		tr.isDirty = true;
	}
}

// Internal helper for SetDirtyRecursive with cycle detection
static void SetDirtyRecursiveInternal(Entity entity, std::set<Entity>& visited) {
	// Cycle detection - prevent infinite recursion
	if (visited.count(entity) > 0) {
		std::cerr << "[TransformSystem] ERROR: Circular hierarchy reference detected for entity " << entity << std::endl;
		return;
	}
	visited.insert(entity);

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	// Validate entity has Transform component
	if (!ecsManager.HasComponent<Transform>(entity)) {
		return;
	}

	Transform& transform = ecsManager.GetComponent<Transform>(entity);
	transform.isDirty = true;

	if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
		auto& children = ecsManager.GetComponent<ChildrenComponent>(entity);
		for (const Entity child : children.ResolveEntities()) {
			if (child == static_cast<Entity>(-1)) {
				continue; // Skip invalid children
			}
			SetDirtyRecursiveInternal(child, visited);
		}
	}
}

void TransformSystem::SetDirtyRecursive(Entity entity) {
	std::set<Entity> visited;
	SetDirtyRecursiveInternal(entity, visited);
}

// Internal helper for GetRootParentTransform with cycle detection
static Transform& GetRootParentTransformInternal(Entity currentEntity, std::set<Entity>& visited) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	// Cycle detection
	if (visited.count(currentEntity) > 0) {
		std::cerr << "[TransformSystem] ERROR: Circular parent reference detected in GetRootParentTransform for entity " << currentEntity << std::endl;
		return ecsManager.GetComponent<Transform>(currentEntity);
	}
	visited.insert(currentEntity);

	if (!ecsManager.HasComponent<ParentComponent>(currentEntity)) {
		return ecsManager.GetComponent<Transform>(currentEntity);
	}
	else {
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(ecsManager.GetComponent<ParentComponent>(currentEntity).parent);
		if (parent == static_cast<Entity>(-1)) {
			std::cerr << "[TransformSystem] ERROR: Invalid parent GUID in GetRootParentTransform for entity " << currentEntity << std::endl;
			return ecsManager.GetComponent<Transform>(currentEntity);
		}
		return GetRootParentTransformInternal(parent, visited);
	}
}

Transform& TransformSystem::GetRootParentTransform(Entity currentEntity) {
	std::set<Entity> visited;
	return GetRootParentTransformInternal(currentEntity, visited);
}

// Internal helper for GetAllChildEntitiesVector with cycle detection
static void GetAllChildEntitiesVectorInternal(Entity parentEntity, std::vector<Entity>& allChildEntities, std::set<Entity>& visited) {
	// Cycle detection
	if (visited.count(parentEntity) > 0) {
		std::cerr << "[TransformSystem] ERROR: Circular hierarchy reference detected in GetAllChildEntitiesVector for entity " << parentEntity << std::endl;
		return;
	}
	visited.insert(parentEntity);

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(parentEntity)) {
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(parentEntity);
		for (const Entity child : childrenComp.ResolveEntities()) {
			if (child == static_cast<Entity>(-1)) {
				continue; // Skip invalid children
			}
			allChildEntities.push_back(child);
			// Recursively get grandchildren
			GetAllChildEntitiesVectorInternal(child, allChildEntities, visited);
		}
	}
}

std::vector<Entity> TransformSystem::GetAllChildEntitiesVector(Entity parentEntity) {
	std::vector<Entity> allChildEntities;
	std::set<Entity> visited;
	GetAllChildEntitiesVectorInternal(parentEntity, allChildEntities, visited);
	return allChildEntities;
}

// Internal helper for GetAllChildEntitiesSet with cycle detection
static void GetAllChildEntitiesSetInternal(Entity parentEntity, std::set<Entity>& allChildEntities, std::set<Entity>& visited) {
	// Cycle detection
	if (visited.count(parentEntity) > 0) {
		std::cerr << "[TransformSystem] ERROR: Circular hierarchy reference detected in GetAllChildEntitiesSet for entity " << parentEntity << std::endl;
		return;
	}
	visited.insert(parentEntity);

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<ChildrenComponent>(parentEntity)) {
		auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(parentEntity);
		for (const Entity child : childrenComp.ResolveEntities()) {
			if (child == static_cast<Entity>(-1)) {
				continue; // Skip invalid children
			}
			allChildEntities.insert(child);
			// Recursively get grandchildren
			GetAllChildEntitiesSetInternal(child, allChildEntities, visited);
		}
	}
}

std::set<Entity> TransformSystem::GetAllChildEntitiesSet(Entity parentEntity) {
	std::set<Entity> allChildEntities;
	std::set<Entity> visited;
	GetAllChildEntitiesSetInternal(parentEntity, allChildEntities, visited);
	return allChildEntities;
}
