#include "pch.h"

#include "Graphics/IRenderComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(IRenderComponent)
	REFL_REGISTER_PROPERTY(isVisible)
	REFL_REGISTER_PROPERTY(renderOrder)
REFL_REGISTER_END;
#pragma endregion
