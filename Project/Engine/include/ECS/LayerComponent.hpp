/* Start Header ************************************************************************/
/*!
\file       LayerComponent.hpp
\author     Muhammad Zikry
\date       2025
\brief      Component representing an entity's layer within the ECS system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <string>
#include "Reflection/ReflectionBase.hpp"

struct LayerComponent
{
	REFL_SERIALIZABLE
	bool overrideFromPrefab = false;
	int layerIndex = 0; // Index into LayerManager's layer list (0-31)

	LayerComponent() = default;
	LayerComponent(int index) : layerIndex(index) {
		if (index < 0) layerIndex = 0;
		if (index >= 32) layerIndex = 31;
	}

	// Helper methods
	void SetLayer(const std::string& layerName);
	const ENGINE_API std::string& GetLayerName() const;
	bool IsInLayer(const std::string& layerName) const;
	int GetLayerMask() const { return 1 << layerIndex; }
};