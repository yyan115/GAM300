#pragma once
#include <lua.hpp>

class LuaEvent {
public:
    void AddListener(lua_State* L, int funcIndex);
    void Invoke();

private:
    lua_State* L = nullptr;
    int ref = LUA_NOREF;
};
