#pragma once
// Scripting.h
// Public API for the scripting subsystem.
//
// Thread-safety notes:
//  - Most Scripting API calls must be made from the main thread (the thread that owns the VM).
//  - Some calls are safe to call from other threads (RequestReload()) but may only *request* an action
//    that is performed on the main thread during Tick().
// Lifetime notes:
//  - Initialize() must be called before any other Scripting functions.
//  - Shutdown() must be called during application shutdown to release resources.

#include <cstdint>
#include <functional>
#include <string>

extern "C" {
    // Forward-declare Lua state here to avoid exposing lua headers to every translation unit
    struct lua_State;
}

namespace Scripting {

    using EnvironmentId = uint32_t; // opaque opaque handle for per-script environments (threads/contexts)

    // Lightweight configuration for runtime initialization.
    // - mainScriptPath: optional path to a script to run immediately after Initialize.
    // - gcIntervalMs: target interval between incremental GC steps (0 = default behavior).
    // - logCallback: optional callback for low-level runtime logging (info/warn/error messages).
    struct ScriptingConfig {
        std::string mainScriptPath;
        unsigned int gcIntervalMs = 100; // ms between incremental GC steps
        // Optional callback used for runtime-level messages. Signature: (level, message)
        // level: "info", "warn", "error"
        std::function<void(const std::string& level, const std::string& msg)> logCallback;
    };

    // Initialize the scripting subsystem and (optionally) run the configured main script.
    // Must be called on the main thread before other Scripting calls.
    // Returns true on success.
    bool Initialize(const ScriptingConfig& cfg);

    // Shutdown the scripting subsystem, freeing all resources.
    // Must be called on the main thread.
    void Shutdown();

    // Called once per-frame on the main thread; runs scheduled script work (update() callbacks,
    // coroutine scheduling, incremental GC, and performs reloads requested via RequestReload()).
    void Tick(float dtSeconds);

    // Request that the runtime reload the main script on the next Tick().
    // Thread-safe (only posts a request).
    void RequestReload();

    // Run a single script file immediately on the main thread. Returns true on success.
    bool RunScriptFile(const std::string& path);

    // Create/destroy per-script environments.
    // A created environment returns a non-zero EnvironmentId. Environments are backed by
    // a lua thread (lua_State * created with lua_newthread) and are safe to use for
    // isolated instances (e.g. one script per entity). IDs are valid until DestroyEnvironment is called.
    //
    // These functions must be called on the main thread (they touch the VM).
    //
    // NOTE: Environments (and their registry references) are **invalidated** when the runtime reloads
    // (e.g. via RequestReload() + Tick()). The runtime frees the old lua_State and associated registry
    // references during reload. If you require environments to persist across reloads you must re-create
    // them and re-establish any registry references after reload (this is not handled automatically).
    EnvironmentId CreateEnvironment(const std::string& name);
    void DestroyEnvironment(EnvironmentId env);

    // Return the current raw lua_State* for read-only / inspection use. This pointer is
    // owned by the runtime and must not be closed by the caller. Accessing the raw lua_State
    // must be done on the main thread or otherwise synchronized by the caller.
    lua_State* GetLuaState();

} // namespace Scripting
