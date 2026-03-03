#include "pch.h"
#include "Graphics/BloomComponent.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
REFL_REGISTER_START(BloomComponent)
    REFL_REGISTER_PROPERTY(enabled)
    REFL_REGISTER_PROPERTY(bloomIntensity)
REFL_REGISTER_END
#pragma endregion
