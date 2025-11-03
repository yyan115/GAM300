#include "pch.h"
#include "ECS/ActiveComponent.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
REFL_REGISTER_START(ActiveComponent)
	REFL_REGISTER_PROPERTY(isActive)
REFL_REGISTER_END
#pragma endregion
