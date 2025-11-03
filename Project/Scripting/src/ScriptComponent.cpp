// ScriptComponent.cpp
#include "ScriptComponent.h"
#include "ScriptSerializer.h" // concrete serializer
#include "Logging.hpp"        // ENGINE_PRINT / logging levels

#include <cassert>

extern "C" {
#include "lauxlib.h"
#include "lualib.h"
}

namespace Scripting {
    // RAII guard to ensure the message handler is removed from the stack
    struct MessageHandlerGuard {
        lua_State* L;
        int msgh_abs_index;
        bool active;
        MessageHandlerGuard(lua_State* L_, int msgh_abs) : L(L_), msgh_abs_index(msgh_abs), active(true) {}
        // non-copyable
        MessageHandlerGuard(const MessageHandlerGuard&) = delete;
        MessageHandlerGuard& operator=(const MessageHandlerGuard&) = delete;
        ~MessageHandlerGuard() {
            if (!L || !active) return;
            // remove the message handler if it still exists
            // Use lua_absindex to convert current index (in case stack mutated)
            // If msgh_abs_index is > 0 and <= current top, remove
            int top = lua_gettop(L);
            // If msgh_abs_index refers outside stack now, compute nearest absolute index (best-effort)
            if (msgh_abs_index >= 1 && msgh_abs_index <= top) {
                lua_remove(L, msgh_abs_index);
            }
            else {
                // best-effort: ignore — shouldn't happen in normal flow
            }
        }
        void dismiss() { active = false; }
    };

    // logging macro wrapper (adapt to your logging)
    #define SC_LOG(level, ...) ENGINE_PRINT(level, __VA_ARGS__)

    // local message handler to get stack traces from errors
    static int LuaMessageHandler(lua_State* L) {
        const char* msg = lua_tostring(L, 1);
        if (msg) {
            luaL_traceback(L, L, msg, 1);
        }
        else {
            lua_pushliteral(L, "(error object is not a string)");
        }
        return 1;
    }

    int ScriptComponent::PushMessageHandler(lua_State* L) {
        lua_pushcfunction(L, LuaMessageHandler);
        return lua_absindex(L, lua_gettop(L)); // stable absolute index
    }

    ScriptComponent::~ScriptComponent() {
        lua_State* L = GetMainState();
        if (L) {
            ClearRefs(L);
        }
        else {
            // VM gone: clear local state
            m_instanceRef = LUA_NOREF;
            m_fnAwakeRef = LUA_NOREF;
            m_fnStartRef = LUA_NOREF;
            m_fnUpdateRef = LUA_NOREF;
            m_fnOnDisableRef = LUA_NOREF;
        }
    }

    lua_State* ScriptComponent::GetMainState() const {
        return ::Scripting::GetLuaState(); // engine-provided
    }

    void ScriptComponent::ClearRefs(lua_State* L) {
        if (!L) return;
        if (m_instanceRef != LUA_NOREF) { luaL_unref(L, LUA_REGISTRYINDEX, m_instanceRef); m_instanceRef = LUA_NOREF; }
        if (m_fnAwakeRef != LUA_NOREF) { luaL_unref(L, LUA_REGISTRYINDEX, m_fnAwakeRef); m_fnAwakeRef = LUA_NOREF; }
        if (m_fnStartRef != LUA_NOREF) { luaL_unref(L, LUA_REGISTRYINDEX, m_fnStartRef); m_fnStartRef = LUA_NOREF; }
        if (m_fnUpdateRef != LUA_NOREF) { luaL_unref(L, LUA_REGISTRYINDEX, m_fnUpdateRef); m_fnUpdateRef = LUA_NOREF; }
        if (m_fnOnDisableRef != LUA_NOREF) { luaL_unref(L, LUA_REGISTRYINDEX, m_fnOnDisableRef); m_fnOnDisableRef = LUA_NOREF; }
    }

    int ScriptComponent::CaptureFunctionRef(lua_State* L, int tableIndex, const char* fieldName) const {
        if (!L) return LUA_NOREF;
        int absIndex = lua_absindex(L, tableIndex);
        lua_getfield(L, absIndex, fieldName); // pushes value or nil

        // direct function?
        if (lua_isfunction(L, -1)) {
            int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops function
            return ref;
        }

        // callable object? check metatable.__call
        if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
            if (lua_getmetatable(L, -1)) {           // pushes metatable
                lua_getfield(L, -1, "__call");      // pushes metatable.__call or nil
                if (lua_isfunction(L, -1)) {
                    // We want to call the object as method: push a small closure that does `return meta.__call(obj, ...)`.
                    // Simpler approach: keep a reference to the object itself (callable), and when calling,
                    // we'll push the object first and then the function retrieved each call — but that costs extra lookups.
                    // For now, store the object reference itself (so on call we'll push object and then lookup __call)
                    lua_pop(L, 2); // pop __call and metatable
                    int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops the object
                    return ref;
                }
                lua_pop(L, 1); // pop __call
                lua_pop(L, 1); // pop metatable
            }
        }

        // not a function nor callable — pop and return NOREF
        lua_pop(L, 1);
        return LUA_NOREF;
    }


    bool ScriptComponent::AttachScript(const std::string& scriptPath) {
        lua_State* L = GetMainState();
        if (!L) {
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::AttachScript: no Lua state available");
            return false;
        }

        // detach current script (safe even if none)
        DetachScript();

        m_scriptPath = scriptPath;
        m_awakeCalled = false;
        m_startCalled = false;

        // load script chunk
        int loadStatus = luaL_loadfile(L, scriptPath.c_str());
        if (loadStatus != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Error, "ScriptComponent::AttachScript - load error: %s", msg ? msg : "(no msg)");
            lua_pop(L, 1);
            return false;
        }

        // protected call with message handler for traceback
        int msgh = PushMessageHandler(L);
        MessageHandlerGuard guard(L, msgh);
        int pcallStatus = lua_pcall(L, 0, 1, msgh);
        if (pcallStatus != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Error, "ScriptComponent::AttachScript - runtime error: %s (script=%s)", msg ? msg : "(no msg)", scriptPath.c_str());
            lua_pop(L, 1); // pop error
            // guard destructor will remove msgh
            return false;
        }
        // On success, we still want to remove the msgh before leaving this scope; either dismiss then remove manually
        // Dismiss and manually remove to keep same order (we still want returned value on stack)
        guard.dismiss();
        lua_remove(L, msgh);

        // ensure returned value is a table; if not, wrap into { _returned = <value> }
        if (!lua_istable(L, -1)) {
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::AttachScript - script did not return a table. Wrapping into table. (script=%s)", scriptPath.c_str());
            // create a temporary registry ref for the returned value to avoid stack-index fragility
            lua_pushvalue(L, -1); // copy returned value
            int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops copy, original still on stack

            lua_newtable(L);                       // new table
            lua_pushliteral(L, "_returned");       // key
            lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef); // push the copied value
            lua_settable(L, -3);                   // new_table["_returned"] = copied value
            luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);

            lua_pop(L, 1); // pop original returned value
            // new table is now on top
        }

        // create persistent registry ref for instance table
        m_instanceRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops the table
        if (m_instanceRef == LUA_NOREF) {
            SC_LOG(EngineLogging::LogLevel::Error, "ScriptComponent::AttachScript - failed to create registry ref for instance");
            return false;
        }

        // capture lifecycle functions (Awake/Start/Update/OnDisable)
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push table
        int tableIndex = lua_gettop(L);
        m_fnAwakeRef = CaptureFunctionRef(L, tableIndex, "Awake");
        m_fnStartRef = CaptureFunctionRef(L, tableIndex, "Start");
        m_fnUpdateRef = CaptureFunctionRef(L, tableIndex, "Update");
        m_fnOnDisableRef = CaptureFunctionRef(L, tableIndex, "OnDisable");
        lua_pop(L, 1); // pop table

        SC_LOG(EngineLogging::LogLevel::Info, "ScriptComponent attached script '%s'", m_scriptPath.c_str());
        return true;
    }

    void ScriptComponent::DetachScript() {
        lua_State* L = GetMainState();
        if (L) {
            ClearRefs(L);
        }
        else {
            // VM destroyed: just clear local refs
            m_instanceRef = LUA_NOREF;
            m_fnAwakeRef = LUA_NOREF;
            m_fnStartRef = LUA_NOREF;
            m_fnUpdateRef = LUA_NOREF;
            m_fnOnDisableRef = LUA_NOREF;
        }
        m_scriptPath.clear();
        m_awakeCalled = false;
        m_startCalled = false;
    }

    void ScriptComponent::Awake() {
        if (m_awakeCalled) return;
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return;
        if (m_fnAwakeRef == LUA_NOREF) { m_awakeCalled = true; return; }

        int msgh = PushMessageHandler(L);
        MessageHandlerGuard guard(L, msgh);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_fnAwakeRef);   // push callableOrFunc

        int nargs = 0;
        if (lua_isfunction(L, -1)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push self
            nargs = 1;
        }
        else {
            // assume callable object with __call
            if (!lua_getmetatable(L, -1)) {
                lua_pop(L, 1);
                SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Awake: callable object missing metatable (script=%s)", m_scriptPath.c_str());
                return;
            }
            lua_getfield(L, -1, "__call"); // push __call
            lua_remove(L, -2);             // remove metatable, stack: callableObj, __call
            lua_insert(L, -2);             // reorder to __call, callableObj
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push self
            nargs = 2; // callableObj + self
        }

        if (lua_pcall(L, nargs, 0, msgh) != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Awake error for script %s: %s", m_scriptPath.c_str(), msg ? msg : "(no msg)");
            lua_pop(L, 1);
            return; // guard removes msgh
        }

        m_awakeCalled = true;
        // guard removes message handler on scope exit
    }


    void ScriptComponent::Start() {
        if (m_startCalled) return;
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return;
        if (m_fnStartRef == LUA_NOREF) { m_startCalled = true; return; }

        int msgh = PushMessageHandler(L);
        MessageHandlerGuard guard(L, msgh);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_fnStartRef); // push callableOrFunc

        int nargs = 0;
        if (lua_isfunction(L, -1)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push self
            nargs = 1;
        }
        else {
            if (!lua_getmetatable(L, -1)) { lua_pop(L, 1); return; }
            lua_getfield(L, -1, "__call");
            lua_remove(L, -2);
            lua_insert(L, -2);
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef);
            nargs = 2;
        }

        if (lua_pcall(L, nargs, 0, msgh) != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Start error: %s (script=%s)", msg ? msg : "(no msg)", m_scriptPath.c_str());
            lua_pop(L, 1);
        }

        m_startCalled = true;
        // guard removes message handler on scope exit
    }


    void ScriptComponent::Update(float dt) {
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return;
        if (m_fnUpdateRef == LUA_NOREF) return;

        int msgh = PushMessageHandler(L);
        MessageHandlerGuard guard(L, msgh);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_fnUpdateRef); // push callableOrFunc

        int nargs = 0;
        if (lua_isfunction(L, -1)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push self
            lua_pushnumber(L, static_cast<lua_Number>(dt));   // push dt
            nargs = 2;
        }
        else {
            if (!lua_getmetatable(L, -1)) {
                lua_pop(L, 1);
                SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Update: callable object missing metatable (script=%s)", m_scriptPath.c_str());
                return;
            }
            lua_getfield(L, -1, "__call");
            lua_remove(L, -2);
            lua_insert(L, -2); // reorder to func, object
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push self
            lua_pushnumber(L, static_cast<lua_Number>(dt));   // push dt
            nargs = 3; // object + self + dt
        }

        if (lua_pcall(L, nargs, 0, msgh) != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Update error: %s (script=%s)", msg ? msg : "(no msg)", m_scriptPath.c_str());
            lua_pop(L, 1);
        }
        // guard removes message handler on scope exit
    }

    void ScriptComponent::OnDisable() {
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return;
        if (m_fnOnDisableRef == LUA_NOREF) return;

        int msgh = PushMessageHandler(L);
        MessageHandlerGuard guard(L, msgh);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_fnOnDisableRef); // push callableOrFunc

        int nargs = 0;
        if (lua_isfunction(L, -1)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push self
            nargs = 1;
        }
        else {
            if (!lua_getmetatable(L, -1)) { lua_pop(L, 1); return; }
            lua_getfield(L, -1, "__call");
            lua_remove(L, -2);
            lua_insert(L, -2);
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef);
            nargs = 2;
        }

        if (lua_pcall(L, nargs, 0, msgh) != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::OnDisable error: %s (script=%s)", msg ? msg : "(no msg)", m_scriptPath.c_str());
            lua_pop(L, 1);
        }
        // guard removes message handler on scope exit
    }

    std::string ScriptComponent::SerializeState() const {
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return "{}";
        if (!m_serializer) m_serializer = std::make_unique<ScriptSerializer>();
        return m_serializer->SerializeInstanceToJson(L, m_instanceRef);
    }

    bool ScriptComponent::DeserializeState(const std::string& json) const {
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return false;
        if (!m_serializer) m_serializer = std::make_unique<ScriptSerializer>();
        return m_serializer->DeserializeJsonToInstance(L, m_instanceRef, json);
    }

} // namespace Scripting
