// TransformComponent.cpp
#include "pch.h"
#include "Transform/TransformComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(Transform)
    REFL_REGISTER_PROPERTY(localPosition)
	REFL_REGISTER_PROPERTY(localScale)
	REFL_REGISTER_PROPERTY(localRotation)
	REFL_REGISTER_PROPERTY(isDirty)
	REFL_REGISTER_PROPERTY(worldMatrix)
REFL_REGISTER_END;
#pragma endregion
