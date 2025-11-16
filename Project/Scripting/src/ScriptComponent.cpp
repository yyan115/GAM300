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
            // After pcall, msgh should be at the bottom of the current stack
            // Remove the element at position 1 (or the stored absolute index if still valid)
            int top = lua_gettop(L);
            if (top >= 1) {
                lua_remove(L, 1);  // Remove the bottommost element (the msgh)
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

    // ScriptComponent.cpp  (replace existing CaptureFunctionRef)
    int ScriptComponent::CaptureFunctionRef(lua_State* L, int tableIndex, const char* fieldName) const {
        if (!L) return LUA_NOREF;

        // preserve stack top and ensure some stack space
        int baseTop = lua_gettop(L);
        if (!lua_checkstack(L, 32)) {
            SC_LOG(EngineLogging::LogLevel::Warn, "CaptureFunctionRef: lua_checkstack failed");
            return LUA_NOREF;
        }

        int absIndex = lua_absindex(L, tableIndex);
        int t = lua_type(L, absIndex);
        if (t != LUA_TTABLE && t != LUA_TUSERDATA) {
            SC_LOG(EngineLogging::LogLevel::Warn,
                "CaptureFunctionRef: expected table/userdata at index ", absIndex,
                " but got type=", lua_typename(L, t));
            lua_settop(L, baseTop);
            return LUA_NOREF;
        }

        // Create a registry tmp-ref for the object to avoid any index fragility
        lua_pushvalue(L, absIndex);                 // push copy
        int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pop copy, keep ref

        // push owned copy from registry
        lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef); // +1 -> object

        // Helper lambda for function/callable detection on the object currently at top-1
        auto try_field_on_top_object = [&](const char* key) -> int {
            // stack: ... , object (at -1)
            lua_getfield(L, -1, key); // push candidate (or nil)
            int candType = lua_type(L, -1);
            if (candType == LUA_TFUNCTION) {
                int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops function
                return ref;
            }
            if (candType == LUA_TTABLE || candType == LUA_TUSERDATA) {
                // check metatable.__call on the candidate
                if (lua_getmetatable(L, -1)) {             // push metatable
                    lua_getfield(L, -1, "__call");        // push __call
                    if (lua_isfunction(L, -1)) {
                        // candidate is callable: ref candidate (not __call)
                        lua_pop(L, 1); // pop __call
                        lua_pop(L, 1); // pop metatable
                        int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop candidate, ref it
                        return ref;
                    }
                    lua_pop(L, 1); // pop __call
                    lua_pop(L, 1); // pop metatable
                }
            }
            // not a function/callable: pop candidate and continue
            lua_pop(L, 1);
            return LUA_NOREF;
            };

        // 1) Try direct field on the instance: instance[fieldName]
        int foundRef = try_field_on_top_object(fieldName);
        if (foundRef != LUA_NOREF) {
            // cleanup object and tmpRef
            lua_pop(L, 1); // pop object
            luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
            lua_settop(L, baseTop);
            return foundRef;
        }

        // 2) Try instance._returned (some loaders wrap non-table returns there)
        lua_getfield(L, -1, "_returned"); // push instance._returned or nil
        if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
            // move this table to replace object slot so our helper sees it as "object"
            lua_remove(L, -2); // remove the original instance, leaving _returned at top
            // try field on _returned
            foundRef = try_field_on_top_object(fieldName);
            if (foundRef != LUA_NOREF) {
                // cleanup and return
                // stack currently has _returned at top
                lua_pop(L, 1); // pop _returned
                luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
                lua_settop(L, baseTop);
                return foundRef;
            }
            // not found on _returned: pop it and re-push original instance for next checks
            lua_pop(L, 1); // pop _returned
            lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef); // re-push object
        }
        else {
            // instance._returned not present (pop nil)
            lua_pop(L, 1);
        }

        // 3) Inspect object's metatable.__index if present and it is a table
        if (lua_getmetatable(L, -1)) { // push metatable
            lua_getfield(L, -1, "__index"); // push __index
            if (lua_istable(L, -1)) {
                // __index[fieldName] may be the function or callable object
                lua_getfield(L, -1, fieldName); // push candidate
                int candType = lua_type(L, -1);
                if (candType == LUA_TFUNCTION) {
                    int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop function and ref it
                    // cleanup: pop __index and metatable and object
                    lua_pop(L, 2); // pop __index and metatable
                    lua_pop(L, 1); // pop object
                    luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
                    lua_settop(L, baseTop);
                    return ref;
                }
                if (candType == LUA_TTABLE || candType == LUA_TUSERDATA) {
                    // candidate might be callable via its metatable.__call
                    if (lua_getmetatable(L, -1)) {
                        lua_getfield(L, -1, "__call");
                        if (lua_isfunction(L, -1)) {
                            // candidate is callable -> ref candidate itself
                            lua_pop(L, 1); // pop __call
                            lua_pop(L, 1); // pop inner metatable
                            int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops candidate
                            lua_pop(L, 2); // pop __index and metatable
                            lua_pop(L, 1); // pop object
                            luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
                            lua_settop(L, baseTop);
                            return ref;
                        }
                        lua_pop(L, 1); // pop __call
                        lua_pop(L, 1); // pop inner metatable
                    }
                }
                // candidate not usable -> pop it
                lua_pop(L, 1);
            }
            // cleanup __index and metatable
            lua_pop(L, 1); // pop __index (or non-table)
            lua_pop(L, 1); // pop metatable
        }

        // Not found anywhere: dump diagnostic info (instance keys/types) to help debugging
        {
            // stack top currently: object
            // We'll call a small diagnostic helper (below) to list keys and their types
            SC_LOG(EngineLogging::LogLevel::Debug, "CaptureFunctionRef: method '", fieldName, "' not found on instance. Dumping keys:");
            // call helper (defined below) - use absolute index of object
            int objIndex = lua_gettop(L);
            // call debugging routine inline
            lua_pushnil(L); // for lua_next
            while (lua_next(L, objIndex) != 0) {
                const char* k = nullptr;
                if (lua_type(L, -2) == LUA_TSTRING) k = lua_tostring(L, -2);
                const char* vtype = luaL_typename(L, -1);
                if (k) SC_LOG(EngineLogging::LogLevel::Debug, "  key=", k, " type=", vtype);
                else SC_LOG(EngineLogging::LogLevel::Debug, "  <non-string-key> type=", vtype);
                lua_pop(L, 1); // pop value, leave key for next
            }
        }

        // cleanup and return NOREF
        luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
        lua_settop(L, baseTop);
        return LUA_NOREF;
    }

    bool ScriptComponent::AttachScript(const std::string& scriptPath) {
        SC_LOG(EngineLogging::LogLevel::Info, "AttachScript: loading '", scriptPath.c_str(), "'");
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

        // protected call with message handler for traceback
        int msgh = PushMessageHandler(L);
        MessageHandlerGuard guard(L, msgh);
        int loadStatus = luaL_loadfile(L, scriptPath.c_str());
        if (loadStatus != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Error, "ScriptComponent::AttachScript - load error: ", msg ? msg : "(no msg)");
            lua_pop(L, 1);
            return false;
        }
        int pcallStatus = lua_pcall(L, 0, 1, msgh);
        if (pcallStatus != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            SC_LOG(EngineLogging::LogLevel::Error, "ScriptComponent::AttachScript - runtime error: ", msg ? msg : "(no msg)", " (script=", scriptPath.c_str(), ")");
            lua_pop(L, 1); // pop error
            // guard destructor will remove msgh safely
            return false;
        }
        // DO NOT manually dismiss and remove the message handler here.
        // Let the MessageHandlerGuard destructor run at end of scope to remove msgh.
        // The returned value is on the stack and will survive the guard destructor
        // (lua_remove removes msgh which is below the returned value; the returned
        // value will shift down but remain on top).

        // ensure returned value is a table; if not, wrap into { _returned = <value> }
        if (!lua_istable(L, -1)) {
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::AttachScript - script did not return a table. Wrapping into table. (script=", m_scriptPath.c_str(), ")");
            // create a temporary registry ref for the returned value to avoid stack-index fragility
            lua_pushvalue(L, -1); // copy returned value
            int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops copy, original still on stack

            lua_newtable(L);                       // new table  (stack: ... , origReturned, newTable)
            lua_pushliteral(L, "_returned");       // key        (stack: ..., origReturned, newTable, "_returned")
            lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef); // push the copied value (stack: ..., origReturned, newTable, "_returned", value)
            lua_settable(L, -3);                   // newTable["_returned"] = value; pops key+value (stack: ..., origReturned, newTable)

            luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);

            // REMOVE the original returned value (which sits at -2), leaving newTable at top.
            // Using lua_remove(L, -2) removes the original while keeping newTable on top.
            lua_remove(L, -2); // <- CORRECT: remove original returned value, not the new table
            // now new table is on top
        }

        // create persistent registry ref for instance table
        m_instanceRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops the table
        if (m_instanceRef == LUA_NOREF) {
            SC_LOG(EngineLogging::LogLevel::Error, "ScriptComponent::AttachScript - failed to create registry ref for instance");
            return false;
        }
        if (m_instanceRef == LUA_NOREF) {
            SC_LOG(EngineLogging::LogLevel::Error, "AttachScript: failed to create instance ref for ", scriptPath.c_str());
        }
        else {
            SC_LOG(EngineLogging::LogLevel::Info, "AttachScript: created instanceRef=", m_instanceRef);
        }

        // capture lifecycle functions (Awake/Start/Update/OnDisable)
        // push instance table from registry for inspection (we have a ref)
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // push instance
        int tableIndex = lua_gettop(L); // absolute index of instance on stack


        // Inspect instance._returned safely (use absolute index)
        lua_getfield(L, tableIndex, "_returned"); // pushes instance._returned or nil
        if (!lua_isnil(L, -1)) {
            if (lua_isstring(L, -1)) {
                const char* s = lua_tostring(L, -1);
                SC_LOG(EngineLogging::LogLevel::Info, "AttachScript: instance._returned (string) = '", s ? s : "(null)", "'");
            }
            else {
                // Dump its type / shape
                SC_LOG(EngineLogging::LogLevel::Info, "AttachScript: instance._returned type = ", luaL_typename(L, -1));
            }
        }
        // pop the _returned value we pushed
        lua_pop(L, 1);

        // Now capture lifecycle refs (table still at tableIndex)
        m_fnAwakeRef = CaptureFunctionRef(L, tableIndex, "Awake");
        m_fnStartRef = CaptureFunctionRef(L, tableIndex, "Start");
        m_fnUpdateRef = CaptureFunctionRef(L, tableIndex, "Update");
        m_fnOnDisableRef = CaptureFunctionRef(L, tableIndex, "OnDisable");

        // pop the instance table
        lua_pop(L, 1);
        SC_LOG(EngineLogging::LogLevel::Info, "ScriptComponent attached script '", m_scriptPath.c_str(), "'");

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
                SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Awake: callable object missing metatable (script=", m_scriptPath.c_str(),")");
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
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Awake error for script ", m_scriptPath.c_str(), ": ", msg ? msg : "(no msg)");
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
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Start error: ", msg ? msg : "(no msg)"," (script=", m_scriptPath.c_str(),")");
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
                SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Update: callable object missing metatable (script=", m_scriptPath.c_str(),")");
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
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::Update error: ", msg ? msg : "(no msg)", " (script=", m_scriptPath.c_str(),")");
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
            SC_LOG(EngineLogging::LogLevel::Warn, "ScriptComponent::OnDisable error: ", msg ? msg : "(no msg)"," (script=", m_scriptPath.c_str(),")");
            lua_pop(L, 1);
        }
        // guard removes message handler on scope exit
    }

    std::string ScriptComponent::SerializeState() const {
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return "{}";
        if (!m_serializer) m_serializer = std::make_unique<ScriptSerializer>();

        int baseTop = lua_gettop(L);
        // push instance
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // +1
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return "{}"; }

        // get instance.fields
        lua_getfield(L, -1, "fields"); // +1 -> fields or nil
        if (lua_isnil(L, -1)) {
            lua_pop(L, 2); // pop nil and instance
            return "{}";
        }
        // make a temp ref for fields
        int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops fields
        lua_pop(L, 1); // pop instance

        std::string out = m_serializer->SerializeInstanceToJson(L, tmpRef);

        luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
        // restore stack to base
        lua_settop(L, baseTop);
        return out;
    }

    bool ScriptComponent::DeserializeState(const std::string& json) const {
        lua_State* L = GetMainState();
        if (!L || m_instanceRef == LUA_NOREF) return false;
        if (!m_serializer) m_serializer = std::make_unique<ScriptSerializer>();
        int baseTop = lua_gettop(L);

        // push instance
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_instanceRef); // +1
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }

        // create or replace fields table for instance
        // push a new table and set it as instance.fields, then call DeserializeJsonToInstance into that table
        lua_newtable(L);                          // +1 new fields table
        int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops table and returns ref

        // use serializer to populate table
        bool ok = m_serializer->DeserializeJsonToInstance(L, tmpRef, json);

        if (ok) {
            // set instance.fields = registry[tmpRef]
            lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef); // push populated table
            lua_setfield(L, -2, "fields"); // set on instance; pops table
        }
        else {
            // leave instance.fields as-is (we created a temp table; no assignment)
        }

        luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
        lua_pop(L, 1); // pop instance
        lua_settop(L, baseTop);
        return ok;
    }


} // namespace Scripting
