#include "pch.h"
#include "ECS/NameComponent.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
REFL_REGISTER_START(NameComponent)
	REFL_REGISTER_PROPERTY(name)
REFL_REGISTER_END
#pragma endregion
