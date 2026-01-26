#pragma once
#include <string>
#include "Utilities/GUID.hpp" // if you prefer GUIDs

// Attach this to every entity instantiated from a prefab.
struct PrefabLinkComponent
{
    // Use either path or GUID (or both). Pick one canonical key.
    std::string prefabPath;     // normalized generic string
    // GUID_128 prefabGuid{};   // optional if you have GUIDs
};