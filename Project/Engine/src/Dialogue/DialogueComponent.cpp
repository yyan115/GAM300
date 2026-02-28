#include <pch.h>
#include "Dialogue/DialogueComponent.hpp"

#pragma region Reflection

REFL_REGISTER_START(DialogueEntry)
REFL_REGISTER_PROPERTY(text)
REFL_REGISTER_PROPERTY(scrollTypeID)
REFL_REGISTER_PROPERTY(autoTime)
REFL_REGISTER_PROPERTY(triggerEntityGuidStr)
REFL_REGISTER_END

REFL_REGISTER_START(DialogueComponent)
REFL_REGISTER_PROPERTY(dialogueName)
REFL_REGISTER_PROPERTY(textEntityGuidStr)
REFL_REGISTER_PROPERTY(appearanceModeID)
REFL_REGISTER_PROPERTY(fadeDuration)
REFL_REGISTER_PROPERTY(typewriterEnabled)
REFL_REGISTER_PROPERTY(textSpeed)
REFL_REGISTER_PROPERTY(entries)
REFL_REGISTER_PROPERTY(autoStart)
REFL_REGISTER_END

#pragma endregion
