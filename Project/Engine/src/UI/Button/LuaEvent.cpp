#include "pch.h"
#include "UI/Button/LuaEvent.hpp"

void LuaEvent::AddListener(lua_State* L, int funcIndex) {
    // Store function reference in registry
    lua_pushvalue(L, funcIndex);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    this->L = L;
}

void LuaEvent::Invoke() {
    if (ref == LUA_NOREF) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        printf("Lua error: %s\n", err);
        lua_pop(L, 1);
    }
}