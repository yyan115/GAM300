#pragma once
// ScriptUtils.h
// Small cross-cutting helpers for Lua <-> C++ conversions.
// - Includes: safe pushers/poppers for strings/numbers, table helpers, registry helpers,
//   and RAII stack guard for lua_State.
// - Contains: comments on error handling conventions and stack discipline.

#include <string>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Scripting {

    // RAII guard that restores Lua stack top when destroyed.
    // Usage:
    //    LuaStackGuard g(L);
    //    // push/pop freely
    //    // on exit stack is restored
    struct LuaStackGuard {
        lua_State* L = nullptr;
        int top = 0;
        LuaStackGuard(lua_State* L_) : L(L_), top(L_ ? lua_gettop(L_) : 0) {}
        ~LuaStackGuard() { if (L) lua_settop(L, top); }
        // non-copyable
        LuaStackGuard(const LuaStackGuard&) = delete;
        LuaStackGuard& operator=(const LuaStackGuard&) = delete;
    };

    // Safe push helpers (do not throw)
    void PushStringSafe(lua_State* L, const std::string& s);
    void PushNumberSafe(lua_State* L, double v);
    void PushBooleanSafe(lua_State* L, bool b);

    // Safe getter helpers; return true if pushed/populated
    bool GetStringSafe(lua_State* L, int idx, std::string& out);
    bool GetNumberSafe(lua_State* L, int idx, double& out);
    bool GetBooleanSafe(lua_State* L, int idx, bool& out);

    // UTF-8 / wide helpers (inline for convenience)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
    static inline std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return std::wstring();
        int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (needed <= 0) return std::wstring();
        std::wstring out;
        out.resize(needed);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], needed);
        return out;
    }

    static inline std::string WideToUtf8(const std::wstring& w) {
        if (w.empty()) return std::string();
        int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return std::string();
        std::string out;
        out.resize(needed);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], needed, nullptr, nullptr);
        return out;
    }
#else
    static inline std::wstring Utf8ToWide(const std::string& utf8) {
        return std::wstring(utf8.begin(), utf8.end());
    }
    static inline std::string WideToUtf8(const std::wstring& w) {
        return std::string(w.begin(), w.end());
    }
#endif

} // namespace Scripting
