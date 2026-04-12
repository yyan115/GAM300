/* Start Header ************************************************************************/
/*!
\file       ActiveComponent.hpp
\author
\date
\brief      Component that controls whether an entity is active or inactive

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "Reflection/ReflectionBase.hpp"

struct ActiveComponent
{
	REFL_SERIALIZABLE
	bool isActive = true;  // Entity active state

	// Runtime flag — NOT serialized. Set by ScriptSystem when this entity
	// transitions from inactive-in-hierarchy to active-in-hierarchy. Cleared
	// after the same frame's script Update runs. Lets Lua detect reactivation
	// (e.g. popup buttons that need to reset stale hover sprites).
	bool justActivated = false;

	ActiveComponent() = default;
	ActiveComponent(bool active) : isActive(active) {}
};
