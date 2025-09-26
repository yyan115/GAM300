#include "pch.h"
#include "Transform/TransformComponent.hpp"
#include "Transform/TransformSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"

void TransformSystem::Initialise() {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities) {
		auto& transform = ecsManager.GetComponent<Transform>(entity);
		// Update model matrix
		transform.model = CalculateModelMatrix(transform.position, transform.scale, transform.rotation);

		// Update the last known values
		transform.lastPosition = transform.position;
		transform.lastRotation = transform.rotation;
		transform.lastScale = transform.scale;
	}
}

void TransformSystem::update() {
	//for (auto& [entities, transform] : transformSystem.forEach()) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities) {
		auto& transform = ecsManager.GetComponent<Transform>(entity);
		// Update model matrix only if there is a change
		if (transform.position != transform.lastPosition || transform.scale != transform.lastScale || transform.rotation != transform.lastRotation) {
			transform.model = CalculateModelMatrix(transform.position, transform.scale, transform.rotation);
		}

		// Update the last known values
		transform.lastPosition = transform.position;
		transform.lastRotation = transform.rotation;
		transform.lastScale = transform.scale;
	}
}

#if 1
Matrix4x4 TransformSystem::CalculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation) {
	float radx = rotation.x * (M_PI / 180.f);
	float rady = rotation.y * (M_PI / 180.f);
	float radz = rotation.z * (M_PI / 180.f);

	//  TRS = T * R * S  (column-major, column vectors)
	Matrix4x4 R = Matrix4x4::RotationZ(radz) * Matrix4x4::RotationY(rady) * Matrix4x4::RotationX(radx);

	return Matrix4x4::TRS(position, R, scale);
}

void TransformSystem::SetPosition(Transform& transform, Vector3D position) {
	transform.position = position;
	transform.model = CalculateModelMatrix(transform.position, transform.scale, transform.rotation);
	// Reset last values to force transform system to detect changes
	transform.lastPosition = { -99999.0f, -99999.0f, -99999.0f };
}

void TransformSystem::SetRotation(Transform& transform, Vector3D rotation) {
	transform.rotation = rotation;
	transform.model = CalculateModelMatrix(transform.position, transform.scale, transform.rotation);
	// Reset last values to force transform system to detect changes
	transform.lastRotation = { -99999.0f, -99999.0f, -99999.0f };
}

void TransformSystem::SetScale(Transform& transform, Vector3D scale) {
	transform.scale = scale;
	transform.model = CalculateModelMatrix(transform.position, transform.scale, transform.rotation);
	// Reset last values to force transform system to detect changes
	transform.lastScale = { -99999.0f, -99999.0f, -99999.0f };
}
#endif
