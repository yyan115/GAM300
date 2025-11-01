// Error helper API for converting lua errors to engine-readable strings.
// - Includes: FormatLuaError(lua_State*, int err), helpers to capture debug.traceback, and mapping of bytecode back to source (if necessary).
// - Contains: comments about protecting the error formatting from further Lua errors.
