// StatePreserver.cpp
#include "StatePreserver.h"
#include "ScriptSerializer.h"
#include "Logging.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cassert>
#include <sstream>

using namespace Scripting;

static inline void SP_LOG(EngineLogging::LogLevel lvl, const char* fmt, ...) 
{
    va_list ap; 
    va_start(ap, fmt);
    ENGINE_PRINT(lvl, fmt, ap);
    va_end(ap);
}

StatePreserver::StatePreserver() = default;
StatePreserver::~StatePreserver() = default;

void StatePreserver::RegisterInstanceKeys(int instanceRef, const std::vector<std::string>& keys)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_registry[instanceRef] = keys;
}

void StatePreserver::UnregisterInstance(int instanceRef)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_registry.erase(instanceRef);
}

void StatePreserver::ClearAll()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_registry.clear();
}

std::string StatePreserver::ExtractState(lua_State* L, int instanceRef) const
{
    if (!L || instanceRef == LUA_NOREF) return {};

    std::vector<std::string> keys;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_registry.find(instanceRef);
        if (it == m_registry.end()) {
            SP_LOG(EngineLogging::LogLevel::Warn, "StatePreserver::ExtractState - no registered keys for instanceRef %d", instanceRef);
            return {};
        }
        keys = it->second;
    }

    // push instance table
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        SP_LOG(EngineLogging::LogLevel::Warn, "StatePreserver::ExtractState - instanceRef not a table");
        return {};
    }
    int instanceIdx = lua_gettop(L);
    int absInstance = lua_absindex(L, instanceIdx);

    // create temp table and fill with selected keys
    lua_newtable(L);
    int tmpIdx = lua_gettop(L);
    int absTmp = lua_absindex(L, tmpIdx);

    for (const std::string& key : keys) {
        lua_getfield(L, absInstance, key.c_str()); // pushes value or nil
        lua_setfield(L, absTmp, key.c_str()); // sets tmp[key] = value (pops value)
    }

    // create a temp registry ref for serialization
    int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops temp table
    // use ScriptSerializer to serialize the temp table
    ScriptSerializer ss;
    std::string json = ss.SerializeInstanceToJson(L, tmpRef);
    luaL_unref(L, LUA_REGISTRYINDEX, tmpRef); // free
    // pop original instance
    lua_pop(L, 1);
    return json;
}

bool StatePreserver::ReinjectState(lua_State* L, int targetInstanceRef, const std::string& json, const UserdataReconcileFn& userdataReconciler) const
{
    if (!L || targetInstanceRef == LUA_NOREF) return false;
    if (json.empty()) return false;

    // Create a temporary table that will be populated from json by ScriptSerializer.
    lua_newtable(L);
    int tmpIdx = lua_gettop(L);
    int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops table

    ScriptSerializer ss;
    if (!ss.DeserializeJsonToInstance(L, tmpRef, json)) {
        SP_LOG(EngineLogging::LogLevel::Warn, "StatePreserver::ReinjectState - DeserializeJsonToInstance failed");
        luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
        return false;
    }

    // Now copy members of temp table into target instance (without clearing target).
    // Push temp and target on stack for iteration.
    lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef); // push temp
    lua_rawgeti(L, LUA_REGISTRYINDEX, targetInstanceRef); // push target
    int tempIndex = lua_gettop(L) - 1;
    int targetIndex = lua_gettop(L);
    int absTemp = lua_absindex(L, tempIndex);
    int absTarget = lua_absindex(L, targetIndex);

    // iterate temp table
    lua_pushnil(L);
    while (lua_next(L, absTemp) != 0) {
        // stack: ..., temp, key, value, target
        // key at -2, value at -1
        if (lua_type(L, -2) == LUA_TSTRING) {
            size_t len = 0; const char* k = lua_tolstring(L, -2, &len);
            std::string key(k, len);

            bool handled = false;
            if (userdataReconciler) {
                // give reconciler chance to handle this key.
                // temp value is at -1; provide its absolute index
                int valAbs = lua_absindex(L, lua_gettop(L));
                try 
                {
                    handled = userdataReconciler(L, targetInstanceRef, key, valAbs);
                }
                catch (...) {
                    SP_LOG(EngineLogging::LogLevel::Warn, "StatePreserver::ReinjectState - userdataReconciler threw for key %s", key.c_str());
                    handled = false;
                }
            }

            if (!handled) {
                // default: copy value into target table under key
                lua_pushvalue(L, -1); // copy value
                lua_setfield(L, absTarget, key.c_str()); // pops copy
            }
        }
        // pop value, keep key
        lua_pop(L, 1);
    }

    // cleanup: pop temp and target
    lua_pop(L, 2);
    luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
    return true;
}
