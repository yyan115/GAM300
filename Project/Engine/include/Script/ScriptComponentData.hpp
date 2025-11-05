#pragma once
// ScriptComponentData.hpp
// Engine-side POD for attaching scripts to entities.
// Plain-old-data suitable for storage in ComponentManager.

#include <string>
#include <vector>

struct ScriptComponentData {
    std::string scriptPath;                 // e.g. "Resources/Scripts/mono_behaviour.lua"
    bool enabled = true;
    std::vector<std::string> preserveKeys;  // keys to preserve across hot-reload (optional)

    // Runtime bookkeeping (managed by ScriptSystem)
    int instanceId = -1;        // debug mirror of Lua registry ref (optional)
    bool instanceCreated = false;

    // Entry options
    std::string entryFunction = "OnInit"; // engine will call this after instance creation
    bool autoInvokeEntry = true;
};
