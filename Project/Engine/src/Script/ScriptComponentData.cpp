// ScriptComponentData.cpp
#include "pch.h"
#include "Script/ScriptComponentData.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
REFL_REGISTER_START(ScriptComponentData)
    REFL_REGISTER_PROPERTY(scriptPath)
    REFL_REGISTER_PROPERTY(enabled)
    REFL_REGISTER_PROPERTY(preserveKeys)
    REFL_REGISTER_PROPERTY(entryFunction)
    REFL_REGISTER_PROPERTY(autoInvokeEntry)
REFL_REGISTER_END
#pragma endregion
