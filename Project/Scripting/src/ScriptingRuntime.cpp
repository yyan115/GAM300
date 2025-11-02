// ScriptingRuntime.cpp
// Implementation of ScriptingRuntime and the public Scripting APIs.
//
// Notes:
//  - This implementation now uses ScriptLog as its logging backend. Platform-specific
//    backends can be installed via EnsureWindowsBackend / EnsureAndroidBackend or by
//    calling SetBackend() before runtime initialization.

#include "ScriptingRuntime.h"
#include "ScriptError.h"
#include "Logging.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sys/stat.h> // for stat in LastWriteTimeUtc

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Scripting {
    namespace {
        static std::mutex s_initMutex; // protect s_singletonRuntime creation
        static ScriptingRuntime* g_runtime_for_cfuncs = nullptr; // only used internally for C callbacks

        // Adapter logger that forwards calls to the ScriptLog API.
        // We keep this small and stateless; the runtime will either use an injected ILogger
        // or this adapter as its "default" logger.
        struct ScriptLogAdapter : public ILogger {
            void Info(const std::string& msg) override 
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Info, "%s", msg.c_str());
            }
            void Warn(const std::string& msg) override 
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "%s", msg.c_str());
            }
            void Error(const std::string& msg) override 
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "%s", msg.c_str());
            }
        };

        // Simple minimal FS implementation that reads from disk. Useful for tests / desktop.
        struct DefaultFileSystem : public IScriptFileSystem {
            bool ReadAllText(const std::string& path, std::string& out) override {
                std::ifstream ifs(path, std::ios::binary);
                if (!ifs) return false;
                std::string content((std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
                out.swap(content);
                return true;
            }
            bool Exists(const std::string& path) override {
                std::ifstream ifs(path);
                return static_cast<bool>(ifs);
            }
            uint64_t LastWriteTimeUtc(const std::string& path) override {
                struct stat st;
                if (stat(path.c_str(), &st) != 0) return 0;
                return static_cast<uint64_t>(st.st_mtime);
            }
        };

        ////////////////////////////////////////////////////////////////////////////////
        // Lua C functions (registered into Lua)
        //

        static int l_cpp_log(lua_State* L) {
            const char* msg = luaL_optstring(L, 1, "");
            // Use ScriptLog API to print from Lua
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "%s", msg ? msg : "");
            return 0;
        }

        ////////////////////////////////////////////////////////////////////////////////
        // traceback helper used when calling lua_pcall to produce stack traces.
        ////////////////////////////////////////////////////////////////////////////////
        static int lua_traceback(lua_State* L) {
            const char* msg = lua_tostring(L, 1);
            if (msg) {
                luaL_traceback(L, L, msg, 1);
            }
            else {
                lua_pushliteral(L, "(no error message)");
            }
            return 1;
        }

    } // namespace (anonymous)

    ////////////////////////////////////////////////////////////////////////////////
    // ScriptingRuntime implementation
    ////////////////////////////////////////////////////////////////////////////////

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
        IScriptFileSystem* fs,
        ILogger* logger) {

        // Short critical section: check/assign basic state
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_L) {
                    if (logger) logger->Warn("ScriptingRuntime::Initialize called but already initialized.");
                    return false;
                }
                m_config = cfg;
                m_fs = fs ? fs : new DefaultFileSystem();

                if (logger) {
                    m_logger = logger;
                }
                else {
                    static ScriptLogAdapter s_adapter;
                    m_logger = &s_adapter;
                }
            } // release lock here

            // Create lua state and run scripts / bindings WITHOUT holding m_mutex.
            lua_State* newL = nullptr;
            if (!create_lua_state(newL)) {
                if (m_logger) m_logger->Error("Failed to create lua state");
                return false;
            }

            // make C callbacks see this runtime
            g_runtime_for_cfuncs = this;

            // run main script if provided (these run in newL only)
            if (!m_config.mainScriptPath.empty()) {
                if (!load_and_run_main_script(newL)) {
                    if (m_logger) m_logger->Error("Failed running main script: " + m_config.mainScriptPath);
                    // Not fatal per original behaviour; continue
                }
            }

            // run binding callbacks (so subsystems can attach their functions)
            run_bindings_for_state(newL);

            // publish the new state under lock
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_L = newL;
                m_lastGcTime = std::chrono::steady_clock::now();
            }

            return true;
    }


    void ScriptingRuntime::Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_reloadRequested.store(false);
        g_runtime_for_cfuncs = nullptr;

        // Wait until no active users are using the lua state
        while (m_activeUsers.load(std::memory_order_acquire) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (m_L) {
            close_lua_state(m_L);
            m_L = nullptr;
        }

        // Note: we intentionally do not delete m_fs or m_logger here for default instances.
    }

    void ScriptingRuntime::Tick(float dtSeconds) {
        // Handle reload request (do heavy work unlocked)
        if (m_reloadRequested.exchange(false)) {

            // Create new state and run scripts / bindings without holding m_mutex
            lua_State* newL = nullptr;
            if (!create_lua_state(newL)) {
                if (m_logger) m_logger->Error("Reload: failed to create new lua state");
            }
            else {
                // make C callbacks see this runtime
                g_runtime_for_cfuncs = this;

                bool success = true;
                if (!m_config.mainScriptPath.empty()) {
                    if (!load_and_run_main_script(newL)) {
                        if (m_logger) m_logger->Error("Reload: failed to run main script in new state");
                        success = false;
                    }
                }

                if (success) {
                    run_bindings_for_state(newL);

                    // call on_reload in new state (no locking)
                    lua_getglobal(newL, "on_reload");
                    if (lua_isfunction(newL, -1)) {
                        if (!safe_pcall(newL, 0, 0)) {
                            if (m_logger) m_logger->Warn("on_reload failed in new state");
                        }
                    }
                    else {
                        lua_pop(newL, 1);
                    }

                    // Wait until no active users are using the old state before swapping/closing it.
                    while (m_activeUsers.load(std::memory_order_acquire) > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }

                    // Swap states under lock and close the old one.
                    lua_State* old = nullptr;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        old = m_L;
                        m_L = newL;
                        m_lastGcTime = std::chrono::steady_clock::now();
                    }
                    if (old) close_lua_state(old);

                    if (m_logger) m_logger->Info("ScriptingRuntime: reload complete");
                }
                else {
                    // Clean up new state if it failed
                    close_lua_state(newL);
                }
            }
        }

        // Call user update() without holding m_mutex.
        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }

        if (snapshotL) {
            m_activeUsers.fetch_add(1);
            // call outside lock
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
            m_activeUsers.fetch_sub(1);
        }

        // incremental GC step (use the helper which will operate safely)
        if (m_config.gcIntervalMs > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastGcTime).count();
            if (elapsed >= static_cast<long long>(m_config.gcIntervalMs)) {
                CollectGarbageStep(); // the implementation below won't hold m_mutex across lua_gc
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
        return result;
    }

    EnvironmentId ScriptingRuntime::CreateEnvironment(const std::string& name) {
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
        int ref = luaL_ref(snapshotL, LUA_REGISTRYINDEX); // pops the thread and stores a ref
        EnvironmentId id = m_nextEnvId++;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_envRegistryRefs.emplace(id, ref);
        }
        m_activeUsers.fetch_sub(1);
        return id;
    }

    void ScriptingRuntime::DestroyEnvironment(EnvironmentId id) {
        int ref = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_envRegistryRefs.find(id);
            if (it == m_envRegistryRefs.end()) return;
            ref = it->second;
            m_envRegistryRefs.erase(it);
        }
        // Need an L snapshot for luaL_unref
        lua_State* snapshotL = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshotL = m_L;
        }
        if (!snapshotL) return;
        m_activeUsers.fetch_add(1);
        luaL_unref(snapshotL, LUA_REGISTRYINDEX, ref);
        m_activeUsers.fetch_sub(1);
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
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Private helpers
    ////////////////////////////////////////////////////////////////////////////////

    bool ScriptingRuntime::create_lua_state(lua_State*& out) {
        lua_State* L = luaL_newstate();
        if (!L) return false;
        luaL_openlibs(L);
        register_core_bindings(L);
        out = L;
        return true;
    }

    void ScriptingRuntime::close_lua_state(lua_State* L) {
        if (!L) return;
        lua_close(L);
    }

    void ScriptingRuntime::register_core_bindings(lua_State* L) {
        lua_pushcfunction(L, l_cpp_log);
        lua_setglobal(L, "cpp_log");
    }

    void ScriptingRuntime::run_bindings_for_state(lua_State* L) {
        std::vector<BindingCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cbs = m_bindings;
        }
        for (auto& cb : cbs) {
            if (cb) {
                cb(L);
            }
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
        int base = lua_gettop(L) - nargs; // position of function
        lua_pushcfunction(L, lua_traceback);
        lua_insert(L, base); // place errfunc under function/args
        int status = lua_pcall(L, nargs, nresults, base);
        if (status != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (m_logger) {
                m_logger->Error(std::string("Lua error: ") + (err ? err : "(no msg)"));
            }
            else {
                std::cerr << "Lua error: " << (err ? err : "(no msg)") << "\n";
            }
            lua_pop(L, 1); // pop error message
            lua_remove(L, base); // remove error handler
            return false;
        }
        lua_remove(L, base);
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Public Scripting module free functions (convenience wrappers to a single runtime)
    //
    namespace {
        static std::unique_ptr<ScriptingRuntime> s_singletonRuntime;
    }

    bool Initialize(const ScriptingConfig& cfg) {
        if (s_singletonRuntime) {
            return false;
        }
        s_singletonRuntime = std::make_unique<ScriptingRuntime>();
        return s_singletonRuntime->Initialize(cfg, nullptr, nullptr);
    }

    void Shutdown() {
        if (!s_singletonRuntime) return;
        s_singletonRuntime->Shutdown();
        s_singletonRuntime.reset();
    }

    void Tick(float dtSeconds) {
        if (!s_singletonRuntime) return;
        s_singletonRuntime->Tick(dtSeconds);
    }

    void RequestReload() {
        if (!s_singletonRuntime) return;
        s_singletonRuntime->RequestReload();
    }

    bool RunScriptFile(const std::string& path) {
        if (!s_singletonRuntime) return false;
        return s_singletonRuntime->RunScriptFile(path);
    }

    EnvironmentId CreateEnvironment(const std::string& name) {
        if (!s_singletonRuntime) return 0;
        return s_singletonRuntime->CreateEnvironment(name);
    }

    void DestroyEnvironment(EnvironmentId id) {
        if (!s_singletonRuntime) return;
        s_singletonRuntime->DestroyEnvironment(id);
    }

    lua_State* GetLuaState() {
        if (!s_singletonRuntime) return nullptr;
        return s_singletonRuntime->GetLuaState();
    }

} // namespace Scripting
