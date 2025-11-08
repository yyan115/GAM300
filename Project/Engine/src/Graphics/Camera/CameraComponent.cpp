#include "pch.h"
#include "Graphics/Camera/CameraComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(CameraComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(isActive)
	REFL_REGISTER_PROPERTY(priority)
	// target, up, shakeOffset are glm::vec3 - handled by custom renderer
	REFL_REGISTER_PROPERTY(yaw)
	REFL_REGISTER_PROPERTY(pitch)
	REFL_REGISTER_PROPERTY(useFreeRotation)
	// projectionType is enum - handled by custom renderer
	REFL_REGISTER_PROPERTY(fov)
	REFL_REGISTER_PROPERTY(nearPlane)
	REFL_REGISTER_PROPERTY(farPlane)
	REFL_REGISTER_PROPERTY(orthoSize)
	REFL_REGISTER_PROPERTY(movementSpeed)
	REFL_REGISTER_PROPERTY(mouseSensitivity)
	REFL_REGISTER_PROPERTY(minZoom)
	REFL_REGISTER_PROPERTY(maxZoom)
	REFL_REGISTER_PROPERTY(zoomSpeed)
	REFL_REGISTER_PROPERTY(shakeIntensity)
	REFL_REGISTER_PROPERTY(shakeDuration)
	REFL_REGISTER_PROPERTY(shakeFrequency)
	REFL_REGISTER_PROPERTY(skyboxTextureGUID)
REFL_REGISTER_END
#pragma endregion
