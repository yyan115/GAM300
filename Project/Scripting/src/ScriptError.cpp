// ScriptError.cpp
//
// Implementation of error formatting & stacktrace capture.
//
// Behavior:
//  - Reads the Lua error message (if present) from the top of the stack.
//  - Calls luaL_traceback to build a stack traceback string and returns a composed message.
//  - Protects itself from causing additional Lua errors while formatting.
//
// Caveat: callers must ensure it's safe to mutate the provided lua_State's stack.

#include "ScriptError.h"
#include "Logging.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <sstream>

namespace Scripting {
    namespace Error {

        // Default SourceMapLookup simply returns "filename:line" — override in your project if desired.
        std::string SourceMapLookup(const std::string& filename, int line) {
            // No source map by default; keep the filename as-is.
            (void)line;
            return filename;
        }

        static int safe_tostring(lua_State* L, int index, std::string& out) {
            // lua_tolstring can call metamethods on some objects; to be safe we catch if the value
            // is already a string; otherwise use luaL_tolstring which pushes a string result.
            if (lua_type(L, index) == LUA_TSTRING) {
                size_t len = 0;
                const char* s = lua_tolstring(L, index, &len);
                if (s) {
                    out.assign(s, len);
                    return 1;
                }
                return 0;
            }
            else {
                // luaL_tolstring pushes a string representation onto the stack; we then pop it.
                luaL_tolstring(L, index, nullptr); // pushes result
                size_t len = 0;
                const char* s = lua_tolstring(L, -1, &len);
                if (s) {
                    out.assign(s, len);
                }
                lua_pop(L, 1);
                return 1;
            }
        }

        std::string FormatLuaError(lua_State* L, int err) {
            if (!L) return std::string("Lua error (null lua_State)");

            // We'll capture stack top to restore later.
            int top = lua_gettop(L);
            std::string message;

            // If an error object is on the top, try to convert it to string safely.
            if (top > 0) {
                safe_tostring(L, top, message);
            }

            // Build a traceback using luaL_traceback.
            // luaL_traceback pushes a string with the stack trace onto the stack.
            luaL_traceback(L, L, message.empty() ? nullptr : message.c_str(), 1);
            const char* tb = lua_tostring(L, -1);
            std::string traceback = tb ? tb : "(no traceback)";

            // Compose final message: include error code name if applicable.
            std::ostringstream oss;
            oss << "Lua error";
            switch (err) {
            case LUA_ERRRUN: oss << " (runtime)"; break;
            case LUA_ERRMEM: oss << " (memory)"; break;
            case LUA_ERRERR: oss << " (error while handling error)"; break;
            case LUA_ERRSYNTAX: oss << " (syntax)"; break;
            default: break;
            }
            oss << ": ";
            if (!message.empty()) {
                oss << message;
            }
            else {
                oss << "(no message)";
            }
            oss << "\n\nStack traceback:\n" << traceback;

            // Optionally post-process the traceback to map file:line entries using SourceMapLookup.
            // Example traceback lines look like: "\tpath/to/file.lua:123: in function 'foo'"
            // We will do a simple scan for ":\d+:" patterns and attempt to map the filename part.
            std::string result = oss.str();

            // naive pass: very careful string parsing
            // Find occurrences of "\n\t" followed by path and ':' number
            size_t pos = 0;
            std::ostringstream mapped;
            while (pos < result.size()) {
                size_t lineStart = result.find('\n', pos);
                if (lineStart == std::string::npos) {
                    mapped << result.substr(pos);
                    break;
                }
                // copy up to lineStart+1
                mapped << result.substr(pos, lineStart - pos + 1);
                pos = lineStart + 1;

                // attempt to parse filename:line following pos
                // If not a stack entry, continue
                if (pos < result.size() && result[pos] == '\t') {
                    size_t colon = result.find(':', pos + 1);
                    if (colon != std::string::npos) {
                        // parse until next ':' which likely separates line number and message
                        size_t colon2 = result.find(':', colon + 1);
                        if (colon2 != std::string::npos && colon2 > colon) {
                            std::string filename = result.substr(pos + 1, colon - (pos + 1));
                            std::string lineStr = result.substr(colon + 1, colon2 - (colon + 1));
                            // try convert to int
                            try {
                                int lineNo = std::stoi(lineStr);
                                std::string mappedFile = SourceMapLookup(filename, lineNo);
                                // write mapped entry
                                mapped << "\t" << mappedFile << ":" << lineNo;
                                // append remainder of line up to next newline
                                size_t nextNL = result.find('\n', colon2 + 1);
                                if (nextNL == std::string::npos) {
                                    mapped << result.substr(colon2);
                                    pos = result.size();
                                }
                                else {
                                    mapped << result.substr(colon2, nextNL - (colon2));
                                    pos = nextNL;
                                }
                                continue;
                            }
                            catch (...) {
                                // conversion failed; fallback to original substring
                            }
                        }
                    }
                }
                // default: copy rest of line until next newline
                size_t nextNL = result.find('\n', pos);
                if (nextNL == std::string::npos) {
                    mapped << result.substr(pos);
                    break;
                }
                else {
                    mapped << result.substr(pos, nextNL - pos);
                    pos = nextNL;
                }
            }

            // restore stack to previous top
            lua_settop(L, top);

            std::string finalStr = mapped.str();
            // Also log it at error level via scripting log (if available)
            ENGINE_PRINT(EngineLogging::LogLevel::Error, finalStr.c_str());
            return finalStr;
        }

    } // namespace Error
} // namespace Scripting
