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
// The public free functions at the bottom of the implementation file provide a singleton
// wrapper used by the rest of the engine.

#include "Scripting.h"
#include "ScriptFileSystem.h"
#include "CoroutineScheduler.h"
#include "ModuleLoader.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
    struct lua_State;
}

namespace Scripting {

    struct ILogger {
        virtual ~ILogger() = default;
        virtual void Info(const std::string& msg) = 0;
        virtual void Warn(const std::string& msg) = 0;
        virtual void Error(const std::string& msg) = 0;
    };

    struct ScriptingConfig {
        std::string mainScriptPath;
        int gcIntervalMs = 1000;
        bool createNewVM = true;
        bool openLibs = true;
        ScriptingConfig() = default;
    };

    class ScriptingRuntime {
    public:
        using BindingCallback = std::function<void(lua_State*)>;

        ScriptingRuntime();
        ~ScriptingRuntime();

        bool Initialize(const ScriptingConfig& cfg,
            std::shared_ptr<IScriptFileSystem> fs = nullptr,
            ILogger* logger = nullptr);

        void Shutdown();
        void Tick(float dtSeconds);
        void RequestReload();
        bool RunScriptFile(const std::string& path);
        EnvironmentId CreateEnvironment(const std::string& name);
        void DestroyEnvironment(EnvironmentId id);
        void RegisterBinding(BindingCallback cb);
        lua_State* GetLuaState();
        void CollectGarbageStep();
        void FullCollectGarbage();
        void SetHostLogHandler(std::function<void(const std::string&)> handler);

    private:
        bool create_lua_state(lua_State*& out);
        void close_lua_state(lua_State* L);
        void register_core_bindings(lua_State* L);
        void run_bindings_for_state(lua_State* L);
        bool load_and_run_main_script(lua_State* L);
        bool safe_pcall(lua_State* L, int nargs, int nresults);

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::atomic<int> m_activeUsers{ 0 };

        lua_State* m_L = nullptr;
        ScriptingConfig m_config;

        // The runtime holds a shared_ptr to the filesystem. If caller gave a raw/unique FS
        // they should pass a shared_ptr to ensure lifetime, otherwise runtime will create its own.
        std::shared_ptr<IScriptFileSystem> m_fsShared;
        IScriptFileSystem* m_fs = nullptr;

        ILogger* m_logger = nullptr;

        std::vector<BindingCallback> m_bindings;

        std::unordered_map<EnvironmentId, int> m_envRegistryRefs;
        EnvironmentId m_nextEnvId = 1;

        std::atomic<bool> m_reloadRequested{ false };

        std::chrono::steady_clock::time_point m_lastGcTime;

        std::unique_ptr<CoroutineScheduler> m_coroutineScheduler;

        std::unique_ptr<ModuleLoader> m_moduleLoader;

        std::function<void(const std::string&)> m_hostLogHandler;
    };

} // namespace Scripting