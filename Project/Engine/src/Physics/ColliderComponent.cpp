#include "pch.h"
#include "Physics/ColliderComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(ColliderComponent)
	REFL_REGISTER_PROPERTY(layerID)
	REFL_REGISTER_PROPERTY(version)
	REFL_REGISTER_PROPERTY(shapeTypeID)
	REFL_REGISTER_PROPERTY(boxHalfExtents)
REFL_REGISTER_END;
#pragma endregion