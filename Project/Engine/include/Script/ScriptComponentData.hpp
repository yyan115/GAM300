#pragma once
// ScriptComponentData.hpp
// Engine-side POD for attaching scripts to entities.
// Plain-old-data suitable for storage in ComponentManager.

#include <string>
#include <vector>
#include "Reflection/ReflectionBase.hpp"

// Data for a single script instance
struct ScriptData {
    REFL_SERIALIZABLE
    std::string scriptPath;                 // e.g. "Resources/Scripts/mono_behaviour.lua"
    bool enabled = true;
    std::vector<std::string> preserveKeys;  // keys to preserve across hot-reload (optional)

    // Runtime bookkeeping (managed by ScriptSystem)
    int instanceId = -1;        // debug mirror of Lua registry ref (optional)
    bool instanceCreated = false;

    // Entry options
    std::string entryFunction = "OnInit"; // engine will call this after instance creation
    bool autoInvokeEntry = true;

    // If scene load happened when scripting runtime was not available, the serialized
    // instance state (JSON) is kept here until ScriptSystem creates the runtime and
    // can restore it. This prevents losing instance data when loading in environments
    // where Lua isn't initialized yet (editor startup ordering, background loading, etc).
    std::string pendingInstanceState;
};

// Component that can hold multiple scripts per entity (like Unity)
struct ScriptComponentData {
    REFL_SERIALIZABLE
    std::vector<ScriptData> scripts;
};
