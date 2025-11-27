/* Start Header ************************************************************************/
/*!
\file       SiblingIndexComponent.hpp
\author     Muhammad Zikry
\date       2025
\brief      Component to track entity ordering in the hierarchy.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Reflection/ReflectionBase.hpp"

/**
 * @brief Component to track entity ordering in the hierarchy.
 * 
 * This component stores the sibling index of an entity, which determines
 * its display order among its siblings (entities with the same parent,
 * or root entities if no parent).
 * 
 * Lower index values appear first in the hierarchy.
 */
struct SiblingIndexComponent
{
    REFL_SERIALIZABLE
    bool overrideFromPrefab = false;
    int siblingIndex = 0; // Order among siblings (0 = first)

    SiblingIndexComponent() = default;
    SiblingIndexComponent(int index) : siblingIndex(index) {}
};
