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
		transform.model = calculateModelMatrix(transform.position, transform.scale, transform.rotation);

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
			transform.model = calculateModelMatrix(transform.position, transform.scale, transform.rotation);
		}

		// Update the last known values
		transform.lastPosition = transform.position;
		transform.lastRotation = transform.rotation;
		transform.lastScale = transform.scale;
	}
}

#if 1
Matrix4x4 TransformSystem::calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation) {
	float radx = rotation.x * (M_PI / 180.f);
	float rady = rotation.y * (M_PI / 180.f);
	float radz = rotation.z * (M_PI / 180.f);

	//  TRS = T * R * S  (column-major, column vectors)
	Matrix4x4 R = Matrix4x4::RotationZ(radz) * Matrix4x4::RotationY(rady) * Matrix4x4::RotationX(radx);

	return Matrix4x4::TRS(position, R, scale);
}

void TransformSystem::SetPosition(Transform& transform, Vector3D position) {
	transform.position = position;
	transform.model = calculateModelMatrix(transform.position, transform.scale, transform.rotation);
	// Reset last values to force transform system to detect changes
	transform.lastPosition = { -99999.0f, -99999.0f, -99999.0f };
}

void TransformSystem::SetRotation(Transform& transform, Vector3D rotation) {
	transform.rotation = rotation;
	transform.model = calculateModelMatrix(transform.position, transform.scale, transform.rotation);
	// Reset last values to force transform system to detect changes
	transform.lastRotation = { -99999.0f, -99999.0f, -99999.0f };
}

void TransformSystem::SetScale(Transform& transform, Vector3D scale) {
	transform.scale = scale;
	transform.model = calculateModelMatrix(transform.position, transform.scale, transform.rotation);
	// Reset last values to force transform system to detect changes
	transform.lastScale = { -99999.0f, -99999.0f, -99999.0f };
}
#endif

#if 0
glm::mat4 TransformSystem::calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation) {
	//// First, extract the rotation basis of the transform
	//Vector3D x = Vector3D(1, 0, 0) * rotation; // Vec3 * Quat (right vector)
	//Vector3D y = Vector3D(0, 1, 0) * rotation; // Vec3 * Quat (up vector)
	//Vector3D z = Vector3D(0, 0, 1) * rotation; // Vec3 * Quat (forward vector)

	//// Next, scale the basis vectors
	//x = x * scale.x; // Vector * float
	//y = y * scale.y; // Vector * float
	//z = z * scale.z; // Vector * float

	//// Extract the position of the transform
	//Vector3D t = position;

	//// Create matrix
	//return glm::mat4(
	//	x.x, x.y, x.z, 0, // X basis (& Scale)
	//	y.x, y.y, y.z, 0, // Y basis (& scale)
	//	z.x, z.y, z.z, 0, // Z basis (& scale)
	//	t.x, t.y, t.z, 1  // Position
	//);

	// 1) Convert Euler degrees -> radians (XYZ order; adjust if your engine uses a different convention)
	float radx = glm::radians(rotation.x);
	float rady = glm::radians(rotation.y);
	float radz = glm::radians(rotation.z);
	glm::vec3 rads{ radx, rady, radz };

	// 2) Build rotation as a quaternion from Euler (prevents gimbal lock & matches GLM expectations)
	//    NOTE: glm::quat expects Euler angles as (pitch=x, yaw=y, roll=z) in radians.
	glm::quat q = glm::quat(rads);

	// 3) TRS = T * R * S  (column-major, column vectors; this is the common convention in GLM/OpenGL)
	glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
	glm::mat4 R = glm::mat4(q);
	glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

	return T * R * S;
}
#endif