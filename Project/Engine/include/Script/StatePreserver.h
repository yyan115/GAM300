#pragma once
// StatePreserver.h
// Helpers and policy for persisting selected script state across reloads.
//
// - API to register a table/keys for preservation, serialize/deserialize hooks,
//   and the "persist scope" concept.
// - Notes on what can/cannot be persisted safely (simple values, tables of primitives,
//   asset handles) and how to map old userdata to new userdata.
// - Use cases: used by HotReloadManager when preserving critical game state across a reload.
//
// IMPORTANT NOTE ABOUT LIFETIME:
//   The API currently uses Lua registry refs (int) as instance identity keys. Registry refs
//   are valid only for the lifetime of a given lua_State. They are NOT stable across VM
//   destruction / reloads. If you need true cross-reload persistence, you should (a) store
//   a persistent ID in the instance table (e.g. instance.__preserve_id = "<unique>") and
//   register by that id, or (b) register by scriptPath + stable user-provided id. The
//   current implementation is conservative: it checks instance validity at Extract/Reinject
//   time and will fail gracefully if the registry ref is no longer a table.
//
// Threading: all operations are main-thread only (operate on lua_State passed in).

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

extern "C" { struct lua_State; }

namespace Scripting {

    // Called for each key when reinjecting. Parameters:
    //   L: lua_State of target VM
    //   targetInstanceRef: registry ref of the target instance (to write into)
    //   key: field name being injected
    //   tempIndex: absolute stack index where temporary parsed value currently resides
    // Return: true if the callback handled reinjection for this key (and took ownership of the temp value),
    //         false if StatePreserver should perform a default copy from temp -> target.
    using UserdataReconcileFn = std::function<bool(lua_State* /*L*/,
        int /*targetInstanceRef*/,
        const std::string& /*key*/,
        int /*tempIndex*/)>;


    // High-level interface: register which keys to preserve for an instance (or per script path)
    class StatePreserver {
    public:
        StatePreserver();
        ~StatePreserver();

        // Register a set of keys to preserve for a particular instance (registry ref).
        // The preserver will only extract these keys (others are ignored).
        // NOTE: registry refs are only valid for the life of the lua_State they were created with.
        void RegisterInstanceKeys(int instanceRef, const std::vector<std::string>& keys);

        // Unregister an instance (stop preserving it).
        void UnregisterInstance(int instanceRef);

        // Extract selected keys from an instance into a compact JSON string (main-thread only).
        // Uses ScriptSerializer for value serialization where appropriate.
        // Returns empty string on error.
        std::string ExtractState(lua_State* L, int instanceRef) const;

        // Reinject previously extracted JSON into target instance (main-thread only).
        // Optionally provide a userdataReconciler callback to allow mapping old userdata to new userdata.
        // Returns true on success.
        bool ReinjectState(lua_State* L, int targetInstanceRef, const std::string& json, const UserdataReconcileFn& userdataReconciler = nullptr) const;

        // Clear all registered instance keys
        void ClearAll();

    private:
        mutable std::mutex m_mutex;
        // map from instanceRef -> vector<keys>
        mutable std::unordered_map<int, std::vector<std::string>> m_registry;
    };

} // namespace Scripting
