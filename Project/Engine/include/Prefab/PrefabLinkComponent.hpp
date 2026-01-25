#pragma once
#include <string>
#include "Utilities/GUID.hpp" // if you prefer GUIDs
#include "Reflection/ReflectionBase.hpp"

// Attach this to every entity instantiated from a prefab.
struct PrefabLinkComponent
{
    REFL_SERIALIZABLE
    // Use either path or GUID (or both). Pick one canonical key.
    std::string prefabPath;     // normalized generic string
    // GUID_128 prefabGuid{};   // optional if you have GUIDs
};