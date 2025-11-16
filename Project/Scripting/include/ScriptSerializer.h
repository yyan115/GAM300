#pragma once
// ScriptSerializer.h
// Convert a Lua instance table <-> compact JSON.
// - Uses engine reflection (TypeDescriptor) to (de)serialize reflected C++ userdata.
// - All functions operate on the main thread / main lua_State only.
// - No RapidJSON types in this header (implementation-only).

#include <string>
#include <unordered_set>

extern "C" { struct lua_State; }

namespace Scripting {

    class ScriptSerializer {
    public:
        ScriptSerializer();
        ~ScriptSerializer();

        // Serialize the Lua table referenced by 'instanceRef' (registry ref) into a compact JSON string.
        // Returns "{}" on error.
        std::string SerializeInstanceToJson(lua_State* L, int instanceRef) const;

        // Deserialize JSON into the Lua table referenced by 'instanceRef' (registry ref).
        // Returns true on success, false on parse/validation error.
        bool DeserializeJsonToInstance(lua_State* L, int instanceRef, const std::string& json) const;

    private:
        // Internal helpers (implementation-only types are hidden in .cpp)
        bool LuaValueToJson(lua_State* L, int idx,
            /* out */ void* valuePlaceholder,
            /* out */ void* allocatorPlaceholder,
            std::unordered_set<const void*>& visited) const;

        bool JsonToLuaValue(lua_State* L, const void* valuePlaceholder) const;

        bool SerializeReflectedUserdata(lua_State* L, int idx,
            /* out */ void* valuePlaceholder,
            /* out */ void* allocatorPlaceholder) const;

        bool DeserializeReflectedUserdata(lua_State* L, const void* valuePlaceholder) const;
    };

} // namespace Scripting
