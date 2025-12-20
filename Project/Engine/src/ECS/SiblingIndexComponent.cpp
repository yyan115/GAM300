#include "pch.h"
#include "ECS/SiblingIndexComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(SiblingIndexComponent)
    REFL_REGISTER_PROPERTY(overrideFromPrefab)
    REFL_REGISTER_PROPERTY(siblingIndex)
REFL_REGISTER_END
#pragma endregion