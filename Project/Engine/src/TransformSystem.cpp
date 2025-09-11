#include "pch.h"
#include "TransformSystem.hpp"
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

#if 0
Matrix4x4 TransformSystem::calculateModelMatrix(Vector3D const& position, Vector3D const& scale, Vector3D rotation) {
	Matrix4x4 modelMatrix;
	modelMatrix.Translate(position.x, position.y, position.z);
	modelMatrix = modelMatrix.RotationX(rotation.x * (M_PI / -180.f));
	modelMatrix = modelMatrix.RotationY(rotation.y * (M_PI / -180.f));
	modelMatrix = modelMatrix.RotationZ(rotation.z * (M_PI / -180.f));

	return modelMatrix.Scale(scale.x, scale.y, scale.z);
	//return Matrix4x4::Translate(position.x, position.y, position.z) * Matrix4x4::RotationZ(rotation.z) * Matrix4x4::Scale(scale.x, scale.y, scale.z);
}
#endif

#if 1
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