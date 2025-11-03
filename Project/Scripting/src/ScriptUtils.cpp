// ScriptUtils.cpp
#include "ScriptUtils.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstring>

namespace Scripting {

    void PushStringSafe(lua_State* L, const std::string& s) {
        if (!L) return;
        lua_pushlstring(L, s.c_str(), s.size());
    }

    void PushNumberSafe(lua_State* L, double v) {
        if (!L) return;
        lua_pushnumber(L, static_cast<lua_Number>(v));
    }

    void PushBooleanSafe(lua_State* L, bool b) {
        if (!L) return;
        lua_pushboolean(L, b ? 1 : 0);
    }

    bool GetStringSafe(lua_State* L, int idx, std::string& out) {
        if (!L) return false;
        if (!lua_isstring(L, idx)) return false;
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        if (!s) return false;
        out.assign(s, len);
        return true;
    }

    bool GetNumberSafe(lua_State* L, int idx, double& out) {
        if (!L) return false;
        if (!lua_isnumber(L, idx)) return false;
        out = static_cast<double>(lua_tonumber(L, idx));
        return true;
    }

    bool GetBooleanSafe(lua_State* L, int idx, bool& out) {
        if (!L) return false;
        if (!lua_isboolean(L, idx)) return false;
        out = (lua_toboolean(L, idx) != 0);
        return true;
    }

} // namespace Scripting
