#include "pch.h"
#include "Graphics/Model/ModelRenderComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(ModelRenderComponent)
	REFL_REGISTER_PROPERTY(overrideFromPrefab)
	REFL_REGISTER_PROPERTY(modelGUID)
	REFL_REGISTER_PROPERTY(shaderGUID)
	REFL_REGISTER_PROPERTY(materialGUID)
	REFL_REGISTER_PROPERTY(transform)
	REFL_REGISTER_PROPERTY(isVisible)
REFL_REGISTER_END;
#pragma endregion