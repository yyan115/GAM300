#include "pch.h"
#include "Physics/ColliderComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(ColliderComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(layerID)
	REFL_REGISTER_PROPERTY(version)
	REFL_REGISTER_PROPERTY(shapeTypeID)
	REFL_REGISTER_PROPERTY(boxHalfExtents)
	REFL_REGISTER_PROPERTY(sphereRadius)
	REFL_REGISTER_PROPERTY(capsuleRadius)
	REFL_REGISTER_PROPERTY(capsuleHalfHeight)
	REFL_REGISTER_PROPERTY(cylinderRadius)
	REFL_REGISTER_PROPERTY(cylinderHalfHeight)
	REFL_REGISTER_PROPERTY(center)
	REFL_REGISTER_PROPERTY(offset)
REFL_REGISTER_END;
#pragma endregion