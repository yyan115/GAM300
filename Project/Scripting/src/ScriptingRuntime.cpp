// Implementation of ScriptingRuntime: lifecycle and low-level Lua integration.
// - Includes: creation of lua_State, luaL_openlibs, register core bindings, error handler helpers, safe lua_pcall wrappers, and GC tuning helpers.
// - Contains: scheduling of GC steps, convenience API for pushing/popping values, and registration points for other subsystems to register their bindings.
// - Use cases: called by Scripting::Initialize/Shutdown and used by higher-level script managers to execute Lua.
