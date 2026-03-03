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
	// Post-processing fields (blurLayerMask, cgTint handled by custom serialization)
	REFL_REGISTER_PROPERTY(blurEnabled)
	REFL_REGISTER_PROPERTY(blurIntensity)
	REFL_REGISTER_PROPERTY(blurRadius)
	REFL_REGISTER_PROPERTY(blurPasses)
	REFL_REGISTER_PROPERTY(bloomEnabled)
	REFL_REGISTER_PROPERTY(bloomThreshold)
	REFL_REGISTER_PROPERTY(bloomIntensity)
	REFL_REGISTER_PROPERTY(vignetteEnabled)
	REFL_REGISTER_PROPERTY(vignetteIntensity)
	REFL_REGISTER_PROPERTY(vignetteSmoothness)
	// vignetteColor is glm::vec3 — handled by custom serialization
	REFL_REGISTER_PROPERTY(colorGradingEnabled)
	REFL_REGISTER_PROPERTY(cgBrightness)
	REFL_REGISTER_PROPERTY(cgContrast)
	REFL_REGISTER_PROPERTY(cgSaturation)
	REFL_REGISTER_PROPERTY(chromaticAberrationEnabled)
	REFL_REGISTER_PROPERTY(chromaticAberrationIntensity)
	REFL_REGISTER_PROPERTY(chromaticAberrationPadding)
REFL_REGISTER_END
#pragma endregion
