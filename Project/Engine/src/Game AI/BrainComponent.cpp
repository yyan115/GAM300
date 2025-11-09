#include "pch.h"
#include "Game AI/BrainComponent.hpp"
#include "Reflection/ReflectionBase.hpp"

template<>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<BrainKind>() {
    using U = std::underlying_type_t<BrainKind>;
    return GetPrimitiveDescriptor<U>();
}

#pragma region Reflection
REFL_REGISTER_START(BrainComponent)
REFL_REGISTER_PROPERTY(kindInt)
REFL_REGISTER_PROPERTY(activeState)
REFL_REGISTER_PROPERTY(enabled)
REFL_REGISTER_END;
#pragma endregion