// Internal header for the runtime manager that owns the main Lua state(s).
// - Includes: declarations for ScriptingRuntime class: create/destroy Lua state, accessors, GC control, binding registration entry points.
// - Contains: comments about thread-safety (main thread only), responsibility to provide platform hooks (logging, file IO).
// - Use cases: ScriptingRuntime is instantiated by Scripting module and used by HotReloadManager, ScriptComponent system, and editor integration code.
