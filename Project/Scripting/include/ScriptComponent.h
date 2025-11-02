#pragma once
// ScriptComponent.h
// Attach a Lua module (table) instance to an engine entity.
// - All Lua operations must run on the main thread owning the lua_State.
// - Lifecycle functions cached as registry refs: Awake, Start, Update, OnDisable.
// - Uses ScriptSerializer for JSON serialization/deserialization of instance state.
// - Minimal global pollution: everything inside namespace Scripting.

#include <string>
#include <memory>

extern "C" {
#include "lua.h"    // ensures LUA_NOREF etc are available to translation units including this header
}

namespace Scripting {

    class ScriptSerializer; // forward

    // Host-side accessor the engine must implement somewhere in Scripting namespace.
    lua_State* GetLuaState();

    class ScriptComponent {
    public:
        ScriptComponent() = default;
        ~ScriptComponent();

        // non-copyable
        ScriptComponent(const ScriptComponent&) = delete;
        ScriptComponent& operator=(const ScriptComponent&) = delete;

        // Attach a script by file path (relative or absolute depends on engine). The script chunk is executed
        // and expected to return a table (module instance). If not a table, the return value will be wrapped.
        // Returns true on success.
        bool AttachScript(const std::string& scriptPath);

        // Detach currently attached script and clear Lua refs. Safe to call when VM is alive or destroyed.
        void DetachScript();

        // Lifecycle methods — to be called by the engine on the main thread at appropriate times.
        void Awake();
        void Start();
        void Update(float dt);
        void OnDisable();

        // Serialization helpers. Operate on the currently attached instance table.
        // SerializeState returns a compact JSON string (\"{}\" on error).
        std::string SerializeState() const;
        // DeserializeState restores primitive/table/reflected userdata fields into the instance table.
        // Returns true on success.
        bool DeserializeState(const std::string& json) const;

        // Accessors
        int GetInstanceRef() const { return m_instanceRef; }
        bool IsAttached() const { return m_instanceRef != LUA_NOREF; }
        const std::string& GetScriptPath() const { return m_scriptPath; }

    private:
        // helpers (main-thread only)
        lua_State* GetMainState() const;
        int CaptureFunctionRef(lua_State* L, int tableIndex, const char* fieldName) const;
        void ClearRefs(lua_State* L);

        // Pushes message handler for pcall (traceback). Returns absolute index of msg handler on stack.
        static int PushMessageHandler(lua_State* L);

    private:
        // Lua registry refs
        int m_instanceRef = LUA_NOREF;
        int m_fnAwakeRef = LUA_NOREF;
        int m_fnStartRef = LUA_NOREF;
        int m_fnUpdateRef = LUA_NOREF;
        int m_fnOnDisableRef = LUA_NOREF;

        // metadata / state
        std::string m_scriptPath;
        bool m_awakeCalled = false;
        bool m_startCalled = false;

        // serializer (lazy, owned)
        mutable std::unique_ptr<ScriptSerializer> m_serializer;
    };

} // namespace Scripting
