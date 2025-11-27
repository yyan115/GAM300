/* Start Header ************************************************************************/
/*!
\file       TagComponent.cpp
\author     Muhammad Zikry
\date       2025
\brief      Component representing an entity's tag within the ECS system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "pch.h"
#include "ECS/TagComponent.hpp"
#include "ECS/TagManager.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
REFL_REGISTER_START(TagComponent)
	REFL_REGISTER_PROPERTY(overrideFromPrefab)
	REFL_REGISTER_PROPERTY(tagIndex)
REFL_REGISTER_END
#pragma endregion

void TagComponent::SetTag(const std::string& tagName) {
	tagIndex = TagManager::GetInstance().GetTagIndex(tagName);
	if (tagIndex == -1) {
		// Tag doesn't exist, add it
		tagIndex = TagManager::GetInstance().AddTag(tagName);
	}
}

const std::string& TagComponent::GetTagName() const {
	return TagManager::GetInstance().GetTagName(tagIndex);
}

bool TagComponent::HasTag(const std::string& tagName) const {
	return GetTagName() == tagName;
}