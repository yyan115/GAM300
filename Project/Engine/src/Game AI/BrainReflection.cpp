#include "pch.h"
#include "Game AI/Brain.hpp"
#include "Reflection/ReflectionBase.hpp"

// Register reflection info for Brain in a single translation unit to avoid
// multiple-definition / duplicate registration errors that occur when the
// registration macro is placed in a header included by many TUs.

template<>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<BrainKind>() {
    using U = std::underlying_type_t<BrainKind>;
    return GetPrimitiveDescriptor<U>();
}

REFL_REGISTER_START(Brain)
REFL_REGISTER_PROPERTY(kind)
REFL_REGISTER_PROPERTY(started)
REFL_REGISTER_PROPERTY(activeState)
REFL_REGISTER_END