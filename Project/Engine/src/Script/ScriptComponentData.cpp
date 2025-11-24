// ScriptComponentData.cpp
#include "pch.h"
#include "Script/ScriptComponentData.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
// Register ScriptData (individual script)
REFL_REGISTER_START(ScriptData)
    REFL_REGISTER_PROPERTY(scriptGuidStr)
    //REFL_REGISTER_PROPERTY(scriptPath)
    REFL_REGISTER_PROPERTY(enabled)
    REFL_REGISTER_PROPERTY(preserveKeys)
    REFL_REGISTER_PROPERTY(entryFunction)
    REFL_REGISTER_PROPERTY(autoInvokeEntry)
    REFL_REGISTER_PROPERTY(pendingInstanceState)  // Save edited field values to scene file
REFL_REGISTER_END

// Register ScriptComponentData (collection of scripts)
REFL_REGISTER_START(ScriptComponentData)
    REFL_REGISTER_PROPERTY(scripts)
REFL_REGISTER_END
#pragma endregion
