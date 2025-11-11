#pragma once
// ScriptError.h
//
// Error helper API for converting lua errors to engine-readable strings.
//
// Primary function:
//   std::string FormatLuaError(lua_State* L, int err)
//     - L: lua_State that contains the error object (typically at the top of stack).
//     - err: the lua_pcall / luaL_loadbuffer error code (LUA_OK, LUA_ERRSYNTAX, etc).
//     - Returns: a multi-line string containing the error message and a stack traceback.
//
// Thread-safety & Lua usage notes:
//  - This helper mutates the Lua stack while building the formatted error. Callers should
//    ensure they call it in a context where stack mutation is acceptable (main thread or
//    while holding a mutex that protects the VM).
//  - The function is defensive: it tries not to call arbitrary Lua code that could re-enter
//    user scripts or cause further errors. It relies on luaL_traceback which is provided
//    by Lua's auxiliary library and is generally safe.
//
// Limitations:
//  - Mapping from precompiled bytecode back to original source (source maps) is NOT implemented.
//    A hook point (SourceMapLookup) is provided if you wish to map file:line pairs to original
//    locations later.

#include <string>

extern "C" {
    struct lua_State;
}

namespace Scripting {
    namespace Error {

        // Format a Lua error object (on the stack) into a readable string with a traceback.
        // - L: lua_State pointer (must be valid).
        // - err: the error code returned by lua_pcall or luaL_load* (LUA_OK means no error).
        // The function will leave the stack balanced (i.e., it will pop/push as needed and restore top to previous).
        std::string FormatLuaError(lua_State* L, int err);

        // Optional hook: if you want to perform source mapping (bytecode -> original source file/line),
        // implement this function in your codebase and link it in. Default implementation returns the input.
        std::string SourceMapLookup(const std::string& filename, int line);

    } // namespace Error
} // namespace Scripting
