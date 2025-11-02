#pragma once
// ScriptingRuntime.h
//
// Internal runtime manager for the scripting subsystem.
//
// Responsibilities:
//  - Owns the primary lua_State for the application.
//  - Offers safe helpers to create environments (lua threads), perform safe pcall,
//    register binding callbacks (so other subsystems can attach functions), and control GC.
// Threading model:
//  - This class is designed to be used from the main thread. A small number of helper
//    functions are safe to call concurrently (RequestReload()). However, most operations
//    assume they run on the main thread.
//
// This header intentionally contains minimal platform-specific code. Platform-specific
// file I/O and logging should be provided via the injected interfaces below.

#include "Scripting.h"
#include "ScriptFileSystem.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <thread>

extern "C" 
{
    struct lua_State;
}

namespace Scripting 
{
    // Minimal logger interface you can implement per-platform.
    struct ILogger {
        virtual ~ILogger() = default;
        virtual void Info(const std::string& msg) = 0;
        virtual void Warn(const std::string& msg) = 0;
        virtual void Error(const std::string& msg) = 0;
    };

    // Internal runtime class.
    class ScriptingRuntime {
    public:
        using BindingCallback = std::function<void(lua_State*)>;

        ScriptingRuntime();
        ~ScriptingRuntime();

        // Initialize with config. Optionally inject filesystem and logger (ownership not transferred).
        // The runtime does not take ownership of the provided pointers.
        bool Initialize(const ScriptingConfig& cfg,
            IScriptFileSystem* fs = nullptr,
            ILogger* logger = nullptr);

        // Shutdown and free resources.
        void Shutdown();

        // Per-frame tick (main thread).
        void Tick(float dtSeconds);

        // Request a reload (thread-safe).
        void RequestReload();

        // Execute a script file immediately on the main thread.
        bool RunScriptFile(const std::string& path);

        // Create/destroy per-script environments (main thread only).
        EnvironmentId CreateEnvironment(const std::string& name);
        void DestroyEnvironment(EnvironmentId id);

        // Register binding callbacks that will be executed each time a new lua_State is created
        // (initialization and after reload). Callbacks run on the main thread.
        void RegisterBinding(BindingCallback cb);

        // Access raw lua_State* (main-thread only).
        lua_State* GetLuaState();

        // GC control helpers (main thread).
        void CollectGarbageStep();
        void FullCollectGarbage();

    private:
        // internal helpers
        bool create_lua_state(lua_State*& out);
        void close_lua_state(lua_State* L);
        void register_core_bindings(lua_State* L);
        void run_bindings_for_state(lua_State* L);
        bool load_and_run_main_script(lua_State* L);
        bool safe_pcall(lua_State* L, int nargs, int nresults);

    private:
        mutable std::mutex m_mutex; // guards m_L and environment maps if other threads read for inspection
        std::atomic<int> m_activeUsers{ 0 };  // counts threads/callbacks currently using the lua_State*
        lua_State* m_L = nullptr;
        ScriptingConfig m_config;
        // Filesystem ownership: if we create a default FS we hold it here.
        std::unique_ptr<IScriptFileSystem> m_ownedFs;
        IScriptFileSystem * m_fs = nullptr; // not owned (may point into m_ownedFs)
        ILogger* m_logger = nullptr;       // not owned
        std::vector<BindingCallback> m_bindings;

        // Environment bookkeeping (env id -> registry ref)
        std::unordered_map<EnvironmentId, int> m_envRegistryRefs;
        EnvironmentId m_nextEnvId = 1;

        // reload request flag
        std::atomic<bool> m_reloadRequested{ false };

        // GC timing
        std::chrono::steady_clock::time_point m_lastGcTime;
    };

} // namespace Scripting
/*
If you simply stop holding m_mutex while calling Lua, 
you still have a race where a reload on another thread 
may close the old lua_State while this thread is mid-call 
into it — causing UB. m_activeUsers prevents closing the 
state until all in-flight uses finish. We use a simple spin-wait
in reload/Shutdown for clarity; you can improve with condition 
variables if you want to avoid sleeping, but for your runtime 
this small sleep loop is usually acceptable.
*/