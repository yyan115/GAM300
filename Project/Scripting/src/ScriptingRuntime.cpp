// ScriptingRuntime.cpp
#include "ScriptingRuntime.h"
#include "ScriptError.h"
#include "ScriptFileSystem.h"
#include "Logging.hpp"

#include "ModuleLoader.h"
#include <Asset Manager/AssetManager.hpp>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Scripting {
    namespace {
        static ScriptingRuntime* g_runtime_for_cfuncs = nullptr;

        // default adapter logger
        struct DefaultLogger : public ILogger {
            void Info(const std::string& msg) override { ENGINE_PRINT(EngineLogging::LogLevel::Info, msg.c_str()); }
            void Warn(const std::string& msg) override { ENGINE_PRINT(EngineLogging::LogLevel::Warn, msg.c_str()); }
            void Error(const std::string& msg) override { ENGINE_PRINT(EngineLogging::LogLevel::Error, msg.c_str()); }
        };

        // global fallback host log handler used by the C binding
        static std::function<void(const std::string&)> s_globalHostLogHandler;
        // host-provided component resolver callback.
        static std::function<bool(lua_State*, uint32_t, const std::string&)> s_globalGetComponentHandler;

        // Lua C binding for cpp_log - forwards to s_globalHostLogHandler if present, otherwise to ENGINE_PRINT
        static int l_cpp_log(lua_State* L) {
            const char* msg = luaL_optstring(L, 1, "");
            std::string s = msg ? msg : "";
            if (s_globalHostLogHandler) {
                try {
                    s_globalHostLogHandler(s);
                }
                catch (...) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Info, s.c_str());
                }
            }
            else {
                ENGINE_PRINT(EngineLogging::LogLevel::Info, s.c_str());
            }
            return 0;
        }

        // Lua C binding for GetComponent(entityId, componentName)
        // Expects: integer entityId, string componentName
        // The callback (set by host) will be invoked with the current lua_State and should push a single value.
        static int l_get_component(lua_State* L) {
            // Arg 1: entity id (number)
            // Arg 2: component name (string)
            uint32_t entityId = (uint32_t)luaL_checkinteger(L, 1);
            const char* name = luaL_checkstring(L, 2);

            if (s_globalGetComponentHandler) {
                try {
                    bool ok = s_globalGetComponentHandler(L, entityId, std::string(name));
                    if (ok) {
                        // callback must have pushed a value; return 1
                        return 1;
                    }
                    else {
                        lua_pushnil(L);
                        return 1;
                    }
                }
                catch (...) {
                    lua_pushnil(L); return 1;
                }
            }
            // No host handler registered -> return nil
            lua_pushnil(L);
            return 1;
        }

        // Commented out to fix warning C4505 - unreferenced function
        // Remove comments when this function is used
        // static bool ReadFileToString(const std::string& path, std::string& out) {
        //     std::ifstream ifs(path, std::ios::binary);
        //     if (!ifs) return false;
        //     std::ostringstream ss;
        //     ss << ifs.rdbuf();
        //     out = ss.str();
        //     return true;
        // }

        // Compatibility wrapper for obtaining table length across Lua versions
        inline int lua_table_len(lua_State * L, int idx) {
            #if LUA_VERSION_NUM >= 502
                 return (int)lua_rawlen(L, idx);
            #else
                 return (int)lua_objlen(L, idx);
            #endif
        }
    } // namespace

    // -------------------------------------------------------------------------
    // ScriptingRuntime implementation
    // -------------------------------------------------------------------------
    ScriptingRuntime::ScriptingRuntime() {
        m_fs = nullptr;
        m_logger = nullptr;
        m_L = nullptr;
        m_lastGcTime = std::chrono::steady_clock::now();
    }

    ScriptingRuntime::~ScriptingRuntime() {
        Shutdown();
    }

    bool ScriptingRuntime::Initialize(const ScriptingConfig& cfg,
        std::shared_ptr<IScriptFileSystem> fs,
        ILogger* logger)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_L) {
                if (logger) logger->Warn("ScriptingRuntime::Initialize called but already initialized.");
                return false;
            }
            m_config = cfg;

            if (fs) {
                m_fsShared = std::move(fs);
                m_fs = m_fsShared.get();
            }
            else {
                m_fsShared = CreateDefaultFileSystem();
                if (!m_fsShared) {
                    if (logger) logger->Error("ScriptingRuntime::Initialize: CreateDefaultFileSystem failed");
                    return false;
                }
                m_fs = m_fsShared.get();
            }

            if (logger) m_logger = logger;
            else {
                static DefaultLogger s_def;
                m_logger = &s_def;
            }

            if (s_globalHostLogHandler) {
                m_hostLogHandler = s_globalHostLogHandler;
            }

            // create and initialize module loader (use the same fs)
            m_moduleLoader = std::make_unique<ModuleLoader>();
            m_moduleLoader->Initialize(m_fsShared);
            m_moduleLoader->ClearSearchPaths();
            // ADD THESE SEARCH PATHS HERE:
#ifdef _WIN32
            std::string rootDir = AssetManager::GetInstance().GetRootAssetDirectory();
            m_moduleLoader->AddSearchPath(rootDir + "/Scripts/?.lua");
            m_moduleLoader->AddSearchPath(rootDir + "/Scripts/?/init.lua");
            m_moduleLoader->AddSearchPath(rootDir + "/extensions/?.lua");
#else
            m_moduleLoader->AddSearchPath("Resources/Scripts/?.lua");
            m_moduleLoader->AddSearchPath("Resources/Scripts/?/init.lua");
            m_moduleLoader->AddSearchPath("Resources/extensions/?.lua");
#endif

            ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader search paths configured");
        }

        // create new state (no lock)
        lua_State* newL = nullptr;
        if (!create_lua_state(newL)) {
            if (m_logger) m_logger->Error("ScriptingRuntime: create_lua_state failed");
            return false;
        }

        // Install our module loader searcher into the new state
        if (m_moduleLoader) {
            m_moduleLoader->InstallLuaSearcher(newL, -1);

            // Debug verification
            lua_getglobal(newL, "package");
            lua_getfield(newL, -1, "searchers");
            int len = lua_table_len(newL, -1);
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "package.searchers length = ", len);
            lua_rawgeti(newL, -1, len);
            bool isFunc = lua_isfunction(newL, -1);
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "last searcher is function? ", (int)isFunc);
            lua_pop(newL, 3);
        }

        // ... rest of initialization
        g_runtime_for_cfuncs = this;

        if (!m_config.mainScriptPath.empty()) {
            if (!load_and_run_main_script(newL)) {
                if (m_logger) m_logger->Warn("ScriptingRuntime: failed to run main script at init; continuing");
            }
        }

        run_bindings_for_state(newL);

        m_coroutineScheduler = std::make_unique<CoroutineScheduler>();
        m_coroutineScheduler->Initialize(newL);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_L = newL;
            m_lastGcTime = std::chrono::steady_clock::now();
        }

        if (m_logger) m_logger->Info("ScriptingRuntime: initialized");
        return true;
    }

    void ScriptingRuntime::Shutdown() {
        m_reloadRequested.store(false);
        g_runtime_for_cfuncs = nullptr;

        // wait for in-flight users
        std::unique_ptr<CoroutineScheduler> schedulerToShutdown;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this]() { return m_activeUsers.load(std::memory_order_acquire) == 0; });

            // move scheduler out while still holding the runtime lock to avoid races with other threads
            if (m_coroutineScheduler) {
                schedulerToShutdown = std::move(m_coroutineScheduler);
                // m_coroutineScheduler is now null while we call Shutdown() later (outside the lock)
            }

            // keep other cleanup items that require the runtime lock here (e.g. env refs)
            // but do not call into scheduler while holding this lock.
        } // unlock runtime mutex here

        // Safely shutdown scheduler outside runtime lock
        if (schedulerToShutdown) {
            try {
                schedulerToShutdown->Shutdown();
            }
            catch (const std::exception& ex) {
                if (m_logger) m_logger->Warn(std::string("CoroutineScheduler shutdown threw: ") + ex.what());
            }
            catch (...) {
                if (m_logger) m_logger->Warn("CoroutineScheduler shutdown threw unknown exception");
            }
            // schedulerToShutdown will be destroyed here (unique_ptr goes out of scope)
        }

        // Now acquire runtime lock again only for Lua-state specific cleanup
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            if (m_L) {
                for (const auto& kv : m_envRegistryRefs) {
                    int ref = kv.second;
                    if (ref != LUA_NOREF) {
                        luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
                    }
                }
                m_envRegistryRefs.clear();
                m_nextEnvId = 1;

                if (m_moduleLoader) {
                    m_moduleLoader->FlushAll(m_L);
                }

                close_lua_state(m_L);
                m_L = nullptr;
            }
        }

        m_fsShared.reset();
        m_fs = nullptr;
        m_moduleLoader.reset();
        if (m_logger) m_logger->Info("ScriptingRuntime: shutdown complete");
    }


    void ScriptingRuntime::Tick(float dtSeconds) 
    {
        // handle reload request
        if (m_reloadRequested.exchange(false)) {
            lua_State* newL = nullptr;
            if (!create_lua_state(newL)) {
                if (m_logger) m_logger->Error("Reload: failed to create new lua state");
            }
            else
            {
                // Install module loader early so main script and bindings can require modules.
                if (m_moduleLoader)
                {
                    m_moduleLoader->ClearSearchPaths();
                    // Re-add search paths with correct root directory
#ifdef _WIN32
                    std::string rootDir = AssetManager::GetInstance().GetRootAssetDirectory();
                    m_moduleLoader->AddSearchPath(rootDir + "/Scripts/?.lua");
                    m_moduleLoader->AddSearchPath(rootDir + "/Scripts/?/init.lua");
                    m_moduleLoader->AddSearchPath(rootDir + "/extensions/?.lua");
#else
                    m_moduleLoader->AddSearchPath("Resources/Scripts/?.lua");
                    m_moduleLoader->AddSearchPath("Resources/Scripts/?/init.lua");
                    m_moduleLoader->AddSearchPath("Resources/extensions/?.lua");
#endif

                    m_moduleLoader->InstallLuaSearcher(newL, -1);
                    ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader reinstalled for reload with root: ", rootDir.c_str());
                }

                g_runtime_for_cfuncs = this;
                bool success = true;

                if (!m_config.mainScriptPath.empty()) 
                {
                    if (!load_and_run_main_script(newL)) 
                    {
                        if (m_logger) m_logger->Error("Reload: main script failed in new state");
                        success = false;
                    }
                }
                if (success) {
                    run_bindings_for_state(newL);
                    std::unique_ptr<CoroutineScheduler> newScheduler = std::make_unique<CoroutineScheduler>();
                    newScheduler->Initialize(newL);

                    // call on_reload in new state
                    lua_getglobal(newL, "on_reload");
                    if (lua_isfunction(newL, -1)) {
                        if (!safe_pcall(newL, 0, 0)) {
                            if (m_logger) m_logger->Warn("on_reload failed in new state");
                        }
                    }
                    else lua_pop(newL, 1);

                    // wait for current in-flight users to finish, swap states
                    {
                        std::unique_lock<std::mutex> lk(m_mutex);
                        m_cv.wait(lk, [this]() { return m_activeUsers.load(std::memory_order_acquire) == 0; });

                        lua_State* old = m_L;
                        std::unique_ptr<CoroutineScheduler> oldScheduler;
                        oldScheduler.swap(m_coroutineScheduler);
                        m_coroutineScheduler = std::move(newScheduler);

                        m_L = newL;
                        m_lastGcTime = std::chrono::steady_clock::now();

                        // unref old env refs in old state
                        if (old && !m_envRegistryRefs.empty()) {
                            for (const auto& kv : m_envRegistryRefs) {
                                luaL_unref(old, LUA_REGISTRYINDEX, kv.second);
                            }
                        }
                        
                        // Flush modules loaded by our ModuleLoader from the old state.
                        if (old && m_moduleLoader) 
                        {
                            m_moduleLoader->FlushAll(old);
                        }

                        m_envRegistryRefs.clear();
                        m_nextEnvId = 1;

                        // release lock before actually closing old
                    }

                    // close old state (and oldScheduler) outside lock (they go out of scope)
                    if (m_logger) 
                        m_logger->Info("ScriptingRuntime: reload complete");
                }
                else {
                    close_lua_state(newL);
                }
            }
        }

        // snapshot L
        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }

        if (snapshotL) {
            // mark active user
            m_activeUsers.fetch_add(1);
            // call update()
            lua_getglobal(snapshotL, "update");
            if (lua_isfunction(snapshotL, -1)) {
                lua_pushnumber(snapshotL, static_cast<lua_Number>(dtSeconds));
                if (!safe_pcall(snapshotL, 1, 0)) {
                    if (m_logger) m_logger->Error("Error while calling update()");
                }
            }
            else {
                lua_pop(snapshotL, 1);
            }

            // tick coroutine scheduler (scheduler pointer protected by mutex)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_coroutineScheduler) m_coroutineScheduler->Tick(dtSeconds);
            }

            m_activeUsers.fetch_sub(1);
            m_cv.notify_all();
        }

        // GC interval
        if (m_config.gcIntervalMs > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastGcTime).count();
            if (elapsed >= static_cast<long long>(m_config.gcIntervalMs)) {
                CollectGarbageStep();
                m_lastGcTime = now;
            }
        }
    }

    void ScriptingRuntime::RequestReload() {
        m_reloadRequested.store(true);
    }

    bool ScriptingRuntime::RunScriptFile(const std::string& path) {
        lua_State* snapshotL = nullptr;
        IScriptFileSystem* fsSnapshot = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
            fsSnapshot = m_fs;
        }
        if (!snapshotL) return false;

        m_activeUsers.fetch_add(1);
        bool result = false;
        do {
            std::string content;
            if (fsSnapshot && fsSnapshot->ReadAllText(path, content)) {
                int loadStatus = luaL_loadbuffer(snapshotL, content.c_str(), content.size(), path.c_str());
                if (loadStatus != LUA_OK) {
                    const char* msg = lua_tostring(snapshotL, -1);
                    if (m_logger) m_logger->Error(std::string("RunScriptFile - load error: ") + (msg ? msg : "(no msg)"));
                    lua_pop(snapshotL, 1);
                    break;
                }
                if (!safe_pcall(snapshotL, 0, 0)) {
                    if (m_logger) m_logger->Error("RunScriptFile - runtime error on pcall");
                    break;
                }
                result = true;
            }
            else {
                int err = luaL_dofile(snapshotL, path.c_str());
                if (err != LUA_OK) {
                    const char* msg = lua_tostring(snapshotL, -1);
                    if (m_logger) m_logger->Error(std::string("RunScriptFile - dofile error: ") + (msg ? msg : "(no msg)"));
                    lua_pop(snapshotL, 1);
                    break;
                }
                result = true;
            }
        } while (false);

        m_activeUsers.fetch_sub(1);
        m_cv.notify_all();
        return result;
    }

    EnvironmentId ScriptingRuntime::CreateEnvironment(const std::string& name) {
        // Commented out to fix warning C4100 - unreferenced parameter
        // Remove this line when 'name' is used
        (void)name;

        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }
        if (!snapshotL) return 0;

        m_activeUsers.fetch_add(1);
        lua_State* thread = lua_newthread(snapshotL);
        if (!thread) {
            m_activeUsers.fetch_sub(1);
            return 0;
        }
        int ref = luaL_ref(snapshotL, LUA_REGISTRYINDEX);
        EnvironmentId id = m_nextEnvId++;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_envRegistryRefs.emplace(id, ref);
        }
        m_activeUsers.fetch_sub(1);
        m_cv.notify_all();
        return id;
    }

    void ScriptingRuntime::DestroyEnvironment(EnvironmentId id) {
        int ref = LUA_NOREF;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_envRegistryRefs.find(id);
            if (it == m_envRegistryRefs.end()) return;
            ref = it->second;
            m_envRegistryRefs.erase(it);
        }
        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }
        if (!snapshotL) return;
        m_activeUsers.fetch_add(1);
        luaL_unref(snapshotL, LUA_REGISTRYINDEX, ref);
        m_activeUsers.fetch_sub(1);
        m_cv.notify_all();
    }

    void ScriptingRuntime::RegisterBinding(BindingCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (cb) m_bindings.push_back(cb);
    }

    lua_State* ScriptingRuntime::GetLuaState() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_L;
    }

    void ScriptingRuntime::CollectGarbageStep() {
        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }
        if (!snapshotL) return;
        m_activeUsers.fetch_add(1);
        lua_gc(snapshotL, LUA_GCSTEP, 1);
        m_activeUsers.fetch_sub(1);
        m_cv.notify_all();
    }

    void ScriptingRuntime::FullCollectGarbage() {
        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }
        if (!snapshotL) return;
        m_activeUsers.fetch_add(1);
        lua_gc(snapshotL, LUA_GCCOLLECT, 0);
        m_activeUsers.fetch_sub(1);
        m_cv.notify_all();
    }

    void ScriptingRuntime::SetHostLogHandler(std::function<void(const std::string&)> handler) {
        // single lock; do not attempt to re-lock the same mutex twice
        std::lock_guard<std::mutex> lock(m_mutex);

        m_hostLogHandler = std::move(handler);
        // also set the C-binding friendly global so C functions can use it
        s_globalHostLogHandler = m_hostLogHandler;

        // If VM exists, install the global C function into the state under the same lock
        // (we hold the runtime lock to avoid races with state swap in Tick/Reload).
        if (m_L) {
            lua_pushcfunction(m_L, &l_cpp_log);
            lua_setglobal(m_L, "cpp_log");
            // Also register as cpp_print for backwards compatibility
            lua_pushcfunction(m_L, &l_cpp_log);
            lua_setglobal(m_L, "cpp_print");
        }
    }

    void ScriptingRuntime::SetHostGetComponentHandler(std::function<bool(lua_State*, uint32_t, const std::string&)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        s_globalGetComponentHandler = std::move(handler);
        // The GetComponent C function is already installed in any live lua state by register_core_bindings
        // so nothing more to do here.
    }

    void ScriptingRuntime::SetFileSystem(std::shared_ptr<IScriptFileSystem> fs)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (fs) {
            m_fsShared = std::move(fs);
            m_fs = m_fsShared.get();
        }
        else {
            m_fsShared.reset();
            m_fs = nullptr;
        }

        // Reinitialize ModuleLoader with the new filesystem (preserves ModuleLoader instance)
        if (m_moduleLoader) {
            try {
                m_moduleLoader->Initialize(m_fsShared);
            }
            catch (...) {
                if (m_logger) m_logger->Warn("SetFileSystem: ModuleLoader::Initialize threw");
            }
        }

        // If a live VM exists, reinstall the Lua searcher so new requires use the new FS
        if (m_L && m_moduleLoader) {
            try {
                m_moduleLoader->InstallLuaSearcher(m_L, -1);
            }
            catch (...) {
                if (m_logger) m_logger->Warn("SetFileSystem: InstallLuaSearcher threw");
            }
        }
    }
    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------
    bool ScriptingRuntime::create_lua_state(lua_State*& out) {
        lua_State* L = luaL_newstate();
        if (!L) return false;
        if (m_config.openLibs) luaL_openlibs(L);
        register_core_bindings(L);
        out = L;
        return true;
    }

    void ScriptingRuntime::close_lua_state(lua_State* L) {
        if (!L) return;
        lua_close(L);
    }

    void ScriptingRuntime::register_core_bindings(lua_State* L) {
        // existing log binding
        lua_pushcfunction(L, l_cpp_log);
        lua_setglobal(L, "cpp_log");

        // Also register as cpp_print for backwards compatibility with old scripts
        lua_pushcfunction(L, l_cpp_log);
        lua_setglobal(L, "cpp_print");

        // register GetComponent(entityId, name)
        lua_pushcfunction(L, l_get_component);
        lua_setglobal(L, "GetComponent");

        // install a tiny helper that binds instance:GetComponent for a particular instance.
        // This helper is used by Scripting::BindInstanceToEntity to avoid repeated C closures.
        const char* helper_code =
            "function __engine_bind_instance_helpers(inst)\n"
            "  function inst:GetComponent(name)\n"
            "    return GetComponent(inst.entityId, name)\n"
            "  end\n"
            "end\n";
        if (luaL_dostring(L, helper_code) != LUA_OK) {
            // ignore errors � fallback will be used by BindInstanceToEntity
            lua_pop(L, 1);
        }
    }


    void ScriptingRuntime::run_bindings_for_state(lua_State* L) {
        std::vector<BindingCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cbs = m_bindings;
        }
        for (auto& cb : cbs) {
            if (cb) cb(L);
        }
    }

    bool ScriptingRuntime::load_and_run_main_script(lua_State* L) {
        if (!L) return false;
        if (m_config.mainScriptPath.empty()) return true;
        std::string content;
        if (m_fs && m_fs->ReadAllText(m_config.mainScriptPath, content)) {
            int status = luaL_loadbuffer(L, content.c_str(), content.size(), m_config.mainScriptPath.c_str());
            if (status != LUA_OK) {
                const char* msg = lua_tostring(L, -1);
                if (m_logger) m_logger->Error(std::string("load_and_run_main_script: load error: ") + (msg ? msg : "(no msg)"));
                lua_pop(L, 1);
                return false;
            }
            if (!safe_pcall(L, 0, 0)) {
                if (m_logger) m_logger->Error("load_and_run_main_script: runtime error executing script");
                return false;
            }
            return true;
        }
        else {
            int err = luaL_dofile(L, m_config.mainScriptPath.c_str());
            if (err != LUA_OK) {
                const char* msg = lua_tostring(L, -1);
                if (m_logger) m_logger->Error(std::string("load_and_run_main_script: dofile error: ") + (msg ? msg : "(no msg)"));
                lua_pop(L, 1);
                return false;
            }
            return true;
        }
    }

    bool ScriptingRuntime::safe_pcall(lua_State* L, int nargs, int nresults) {
        // compute base index of function
        int base = lua_gettop(L) - nargs;
        if (base < 1) base = 1;
        // push handler and insert at base
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* msg = lua_tostring(L, 1);
            if (msg) luaL_traceback(L, L, msg, 1);
            else lua_pushliteral(L, "(no error message)");
            return 1;
            });
        lua_insert(L, base);
        int status = lua_pcall(L, nargs, nresults, base);
        if (status != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (m_logger) m_logger->Error(std::string("Lua error: ") + (err ? err : "(no msg)"));
            else std::cerr << "Lua error: " << (err ? err : "(no msg)") << "\n";
            lua_pop(L, 1); // pop error message
            // remove handler
            int top = lua_gettop(L);
            if (base >= 1 && base <= top) lua_remove(L, base);
            return false;
        }
        // remove handler
        int top = lua_gettop(L);
        if (base >= 1 && base <= top) lua_remove(L, base);
        return true;
    }

    // -------------------------------------------------------------------------
    // Public module free functions (singleton wrapper)
    // -------------------------------------------------------------------------
    namespace {
        static std::unique_ptr<ScriptingRuntime> s_singletonRuntime;
    }
} // namespace Scripting
