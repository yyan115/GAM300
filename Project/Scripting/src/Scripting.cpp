// src/Scripting.cpp
#include "Scripting.h"
#include "ScriptSerializer.h"
#include "StatePreserver.h"
#include "CoroutineScheduler.h"
#include "Logging.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <mutex>
#include <cassert>
#include <sstream>
#include <atomic>
#include <condition_variable>
#include <memory>

using namespace Scripting;

namespace {

    // Global state for the Scripting glue (single VM)
    std::mutex g_mutex;
    std::condition_variable g_cv;
    std::atomic<int> g_activeUsers{ 0 }; // used to prevent shutdown while pcall active

    lua_State* g_L = nullptr;
    bool g_ownsVM = false;

    // logger/file reader stored in atomic wrappers to allow use from C callbacks without locking
    std::atomic<std::shared_ptr<HostLogFn>> g_hostLoggerPtr{ nullptr };
    std::atomic<std::shared_ptr<ReadAllTextFn>> g_fileReaderPtr{ nullptr };

    std::unique_ptr<CoroutineScheduler> g_coroutineScheduler;
    std::unique_ptr<StatePreserver> g_statePreserver;

    // Default host logger fallback
    void DefaultHostLog(const std::string& s) {
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "%s", s.c_str());
    }

    // Lua: cpp_log(s)
    static int Lua_CppLog(lua_State* L) {
        const char* msg = luaL_optstring(L, 1, "");
        const std::string s = msg ? msg : "";

        auto handlerShared = g_hostLoggerPtr.load(std::memory_order_acquire);
        if (handlerShared && *handlerShared) {
            try {
                // Call outside any mutex; handler should be reentrant safe.
                (*handlerShared)(s);
            }
            catch (...) {
                // swallow exceptions from host logger to avoid crashing Lua.
                DefaultHostLog(s);
            }
        }
        else {
            DefaultHostLog(s);
        }
        return 0;
    }

    // push a message handler *below* the function that will be called.
    // nargs is number of arguments that have been pushed after function (0..n).
    // Returns the absolute stack index of the inserted message handler.
    static int PushMessageHandlerBelowFunction(lua_State* L, int nargs) {
        // function is at top - nargs
        int funcIndex = lua_gettop(L) - nargs;
        if (funcIndex < 1) funcIndex = 1;
        // push handler on top
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* msg = lua_tostring(L, 1);
            if (msg) luaL_traceback(L, L, msg, 1);
            else lua_pushliteral(L, "(error object is not a string)");
            return 1;
            });
        // move the handler to just below the function (i.e., to funcIndex)
        lua_insert(L, funcIndex);
        // the handler now lives at funcIndex
        return funcIndex;
    }

    // Helper: load script to top using override or luaL_loadfile
    static int LoadScriptToTop(lua_State* L, const std::string& path) {
        auto readerShared = g_fileReaderPtr.load(std::memory_order_acquire);
        if (readerShared && *readerShared) {
            std::string content;
            if ((*readerShared)(path, content)) {
                return luaL_loadbuffer(L, content.c_str(), content.size(), path.c_str());
            }
        }
        return luaL_loadfile(L, path.c_str());
    }

    // Helper to increment active users and decrement in RAII manner
    struct ActiveUserGuard {
        ActiveUserGuard() { g_activeUsers.fetch_add(1, std::memory_order_acq_rel); }
        ~ActiveUserGuard() {
            g_activeUsers.fetch_sub(1, std::memory_order_acq_rel);
            g_cv.notify_all();
        }
    };

    // safe pcall that ensures the message handler is inserted in the proper place.
    // NOTE: this function DOES NOT take any synchronization lock. Caller must ensure the lua_State* is valid
    // and not closed while this runs (use active user protocol).
    static bool SafePCall(lua_State* L, int nargs, int nresults, std::string* outErr = nullptr) {
        // place handler below function
        int msghIndex = PushMessageHandlerBelowFunction(L, nargs);

        // call lua_pcall; note lua_pcall expects the msgh index as absolute index
        int status = lua_pcall(L, nargs, nresults, msghIndex);
        if (status != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (outErr) *outErr = err ? err : "(no msg)";
            else {
                // fallback log
                DefaultHostLog(std::string("Lua error: ") + (err ? err : "(no msg)"));
            }
            // pop error message
            lua_pop(L, 1);
            // remove handler (handler still at msghIndex if stack unchanged)
            int top = lua_gettop(L);
            if (msghIndex >= 1 && msghIndex <= top) lua_remove(L, msghIndex);
            return false;
        }
        // remove handler (it is at msghIndex)
        int top = lua_gettop(L);
        if (msghIndex >= 1 && msghIndex <= top) lua_remove(L, msghIndex);
        return true;
    }

} // anonymous

// ------------------------- Public API implementations -------------------------
bool Scripting::Init(const InitOptions& opts) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_L) return true;

    if (opts.createNewVM) {
        g_L = luaL_newstate();
        if (!g_L) return false;
        g_ownsVM = true;
        if (opts.openLibs) luaL_openlibs(g_L);
    }
    // register cpp_log
    if (g_L) {
        lua_pushcfunction(g_L, &Lua_CppLog);
        lua_setglobal(g_L, "cpp_log");
    }

    g_coroutineScheduler = std::make_unique<CoroutineScheduler>();
    if (g_L) g_coroutineScheduler->Initialize(g_L);
    if (!g_statePreserver) g_statePreserver = std::make_unique<StatePreserver>();

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::Init - done");
    return true;
}

void Scripting::Shutdown() {
    // wait for active users
    {
        std::unique_lock<std::mutex> lk(g_mutex);
        g_cv.wait(lk, []() { return g_activeUsers.load(std::memory_order_acquire) == 0; });
    }

    // teardown
    if (g_coroutineScheduler) {
        g_coroutineScheduler->Shutdown();
        g_coroutineScheduler.reset();
    }
    g_statePreserver.reset();

    // close VM if owned
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_L && g_ownsVM) {
            lua_close(g_L);
        }
        g_L = nullptr;
        g_ownsVM = false;
    }

    // clear function pointers
    g_hostLoggerPtr.store(nullptr, std::memory_order_release);
    g_fileReaderPtr.store(nullptr, std::memory_order_release);

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::Shutdown - done");
}

void Scripting::SetLuaState(lua_State* L) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_L && g_ownsVM) {
        lua_close(g_L);
    }
    g_L = L;
    g_ownsVM = false;
    if (g_L) {
        lua_pushcfunction(g_L, &Lua_CppLog);
        lua_setglobal(g_L, "cpp_log");
    }
    if (!g_coroutineScheduler) g_coroutineScheduler = std::make_unique<CoroutineScheduler>();
    if (g_L) g_coroutineScheduler->Initialize(g_L);
}

lua_State* Scripting::GetLuaState() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_L;
}

void Scripting::Tick(float dtSeconds) {
    // snapshot L
    lua_State* snapshot = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        snapshot = g_L;
    }
    if (!snapshot) return;

    ActiveUserGuard g; // increment active users for the duration
    // tick coroutines first
    if (g_coroutineScheduler) g_coroutineScheduler->Tick(dtSeconds);

    // call global update(dt) if present
    lua_getglobal(snapshot, "update");
    if (lua_isfunction(snapshot, -1)) {
        lua_pushnumber(snapshot, static_cast<lua_Number>(dtSeconds));
        // we must call SafePCall without holding g_mutex and with active user counted
        SafePCall(snapshot, 1, 0);
    }
    else {
        lua_pop(snapshot, 1);
    }
}

int Scripting::CreateInstanceFromFile(const std::string& scriptPath) {
    // snapshot lua_State and file reader
    lua_State* snapshot = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        snapshot = g_L;
    }
    if (!snapshot) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CreateInstanceFromFile: no lua state");
        return LUA_NOREF;
    }

    ActiveUserGuard g; // protect against shutdown while we operate

    int loadStatus = LoadScriptToTop(snapshot, scriptPath);
    if (loadStatus != LUA_OK) {
        const char* msg = lua_tostring(snapshot, -1);
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "CreateInstanceFromFile - load: %s (script=%s)", msg ? msg : "(no msg)", scriptPath.c_str());
        lua_pop(snapshot, 1);
        return LUA_NOREF;
    }

    // call chunk expecting 1 result
    // push msgh below the chunk (nargs = 0)
    int msghIndex = PushMessageHandlerBelowFunction(snapshot, 0);
    int pcallStatus = lua_pcall(snapshot, 0, 1, msghIndex);
    if (pcallStatus != LUA_OK) {
        const char* msg = lua_tostring(snapshot, -1);
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "CreateInstanceFromFile - runtime error: %s (script=%s)", msg ? msg : "(no msg)", scriptPath.c_str());
        lua_pop(snapshot, 1);
        // remove msgh if still present
        int top = lua_gettop(snapshot);
        if (msghIndex >= 1 && msghIndex <= top) lua_remove(snapshot, msghIndex);
        return LUA_NOREF;
    }
    // remove msgh (it sits below the result)
    int top = lua_gettop(snapshot);
    if (msghIndex >= 1 && msghIndex <= top) lua_remove(snapshot, msghIndex);

    // If result is not a table, wrap it in a table as _returned
    if (!lua_istable(snapshot, -1)) {
        lua_newtable(snapshot);
        lua_pushliteral(snapshot, "_returned");
        lua_pushvalue(snapshot, -2); // original return value
        lua_settable(snapshot, -3);
        lua_remove(snapshot, -2); // remove original value
    }

    int ref = luaL_ref(snapshot, LUA_REGISTRYINDEX); // pops table
    return ref;
}

void Scripting::DestroyInstance(int instanceRef) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_L) return;
    if (instanceRef == LUA_NOREF) return;
    luaL_unref(g_L, LUA_REGISTRYINDEX, instanceRef);
}

bool Scripting::IsValidInstance(int instanceRef) {
    return instanceRef != LUA_NOREF;
}

bool Scripting::CallInstanceFunction(int instanceRef, const std::string& funcName) {
    // snapshot L
    lua_State* snapshot = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        snapshot = g_L;
    }
    if (!snapshot) return false;
    if (instanceRef == LUA_NOREF) return false;

    ActiveUserGuard g; // prevent shutdown while running

    // push table
    lua_rawgeti(snapshot, LUA_REGISTRYINDEX, instanceRef);
    if (!lua_istable(snapshot, -1)) { lua_pop(snapshot, 1); return false; }
    int tableIdx = lua_gettop(snapshot);

    // get field
    lua_getfield(snapshot, tableIdx, funcName.c_str()); // push func/value

    if (lua_isnil(snapshot, -1)) { lua_pop(snapshot, 2); return false; }

    int nargs = 0;
    // If it is a function, call it with self as first arg
    if (lua_isfunction(snapshot, -1)) {
        lua_pushvalue(snapshot, tableIdx);
        nargs = 1;
    }
    else {
        // check for __call metamethod on value
        if (!lua_getmetatable(snapshot, -1)) { lua_pop(snapshot, 2); return false; }
        lua_getfield(snapshot, -1, "__call");
        lua_remove(snapshot, -2); // remove metatable
        if (!lua_isfunction(snapshot, -1)) { lua_pop(snapshot, 2); return false; }
        // reorder: currently stack: ... table func __call
        // we want: ... func object (self)  => move __call before object
        lua_insert(snapshot, -2); // puts __call before object
        lua_pushvalue(snapshot, tableIdx); // self
        nargs = 2;
    }

    // now call with safe pcall. safe pcall will insert message handler under function and above args
    bool ok = SafePCall(snapshot, nargs, 0);
    // After pcall cleanup SafePCall removed handler; ensure table is popped if still present
    int top = lua_gettop(snapshot);
    if (top >= 1) lua_pop(snapshot, 1); // pop table if present

    return ok;
}

void Scripting::SetHostLogHandler(HostLogFn fn) {
    // store in atomic shared_ptr so Lua C binding can call it without locking
    auto sp = std::make_shared<HostLogFn>(std::move(fn));
    g_hostLoggerPtr.store(sp, std::memory_order_release);
}

void Scripting::SetFileSystemReadAllText(ReadAllTextFn fn) {
    auto sp = std::make_shared<ReadAllTextFn>(std::move(fn));
    g_fileReaderPtr.store(sp, std::memory_order_release);
}

std::string Scripting::SerializeInstanceToJson(int instanceRef) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_L || instanceRef == LUA_NOREF) return "{}";
    ScriptSerializer ss;
    return ss.SerializeInstanceToJson(g_L, instanceRef);
}

bool Scripting::DeserializeJsonToInstance(int instanceRef, const std::string& json) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_L || instanceRef == LUA_NOREF) return false;
    ScriptSerializer ss;
    return ss.DeserializeJsonToInstance(g_L, instanceRef, json);
}

void Scripting::RegisterInstancePreserveKeys(int instanceRef, const std::vector<std::string>& keys) {
    if (!g_statePreserver) g_statePreserver = std::make_unique<StatePreserver>();
    g_statePreserver->RegisterInstanceKeys(instanceRef, keys);
}

std::string Scripting::ExtractInstancePreserveState(int instanceRef) {
    if (!g_statePreserver || !g_L) return {};
    return g_statePreserver->ExtractState(g_L, instanceRef);
}

bool Scripting::ReinjectInstancePreserveState(int instanceRef, const std::string& json) {
    if (!g_statePreserver || !g_L) return false;
    return g_statePreserver->ReinjectState(g_L, instanceRef, json, nullptr);
}

void Scripting::EnableHotReload(bool enable) {
    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::EnableHotReload(%d) called (no-op in glue)", enable ? 1 : 0);
}

void Scripting::RequestReloadNow() {
    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::RequestReloadNow (no-op in glue)");
}

void Scripting::InitializeCoroutineScheduler() {
    if (!g_coroutineScheduler) g_coroutineScheduler = std::make_unique<CoroutineScheduler>();
    if (g_L) g_coroutineScheduler->Initialize(g_L);
}

void Scripting::ShutdownCoroutineScheduler() {
    if (g_coroutineScheduler) {
        g_coroutineScheduler->Shutdown();
        g_coroutineScheduler.reset();
    }
}
