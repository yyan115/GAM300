// Vector2D.cpp - Reflection registration for Vector2D struct
#include "pch.h"
#include "Script/LuaBindableSystems.hpp"

#pragma region Reflection
REFL_REGISTER_START(Vector2D)
	REFL_REGISTER_PROPERTY(x)
	REFL_REGISTER_PROPERTY(y)
REFL_REGISTER_END
#pragma endregion
