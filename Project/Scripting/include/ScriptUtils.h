// Small cross-cutting helpers for Lua <-> C++ conversions.
// - Includes: safe pushers/poppers for strings/numbers/vec3, table helpers, registry helpers, and RAII stack guard for lua_State.
// - Contains: comments on error handling conventions and stack discipline.
// Utf8Utils.h

#pragma once
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>

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

#else // non-windows: trivial identity conversions (platform code should adapt where needed)
static inline std::wstring Utf8ToWide(const std::string& utf8) {
    return std::wstring(utf8.begin(), utf8.end());
}
static inline std::string WideToUtf8(const std::wstring& w) {
    return std::string(w.begin(), w.end());
}
#endif
