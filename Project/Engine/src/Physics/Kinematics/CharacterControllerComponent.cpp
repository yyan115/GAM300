#include "pch.h"
#include "Physics/Kinematics/CharacterControllerComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(CharacterControllerComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(speed)
	REFL_REGISTER_PROPERTY(jumpHeight)
REFL_REGISTER_END;
#pragma endregion
