// src/Scripting.cpp  -- migrated to use ScriptingRuntime and ModuleLoader
#include "Scripting.h"
#include "ScriptingRuntime.h"
#include "ModuleLoader.h"
#include "ScriptFileSystem.h"
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
#include <filesystem>
#include <fstream>
using namespace Scripting;

namespace {

    // adapter object implementing IScriptFileSystem that forwards to ReadAllTextFn
    struct ReadAllTextAdapter : public IScriptFileSystem {
        ReadAllTextAdapter(std::atomic<std::shared_ptr<ReadAllTextFn>>& src)
            : m_src(src) {
        }

        // Read entire file contents. Prefer engine callback; fall back to host FS.
        bool ReadAllText(const std::string& path, std::string& out) override {
            auto sp = m_src.load(std::memory_order_acquire);
            if (sp && *sp) {
                try {
                    if ((*sp)(path, out)) return true;
                }
                catch (...) {
                    // swallow and fallback to host FS
                }
            }

            // Fallback: try reading from host filesystem
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs.good()) return false;
            std::ostringstream ss;
            ss << ifs.rdbuf();
            out = ss.str();
            return true;
        }

        // Check existence: prefer engine callback probe, else host FS check
        bool Exists(const std::string& path) override {
            auto sp = m_src.load(std::memory_order_acquire);
            if (sp && *sp) {
                try {
                    std::string tmp;
                    if ((*sp)(path, tmp)) return true;
                }
                catch (...) {}
            }
            try {
                return std::filesystem::exists(std::filesystem::path(path));
            }
            catch (...) {
                return false;
            }
        }

        // Last-write time (UTC seconds). Returns 0 on error / unknown.
        uint64_t LastWriteTimeUtc(const std::string& path) override {
            // If engine FS exposes a last-write-time API, you could call it here.
            // Fallback to host FS's last_write_time.
            try {
                auto ft = std::filesystem::last_write_time(std::filesystem::path(path));
                // convert to time_t-like uint64 seconds since epoch
                using namespace std::chrono;
                auto s = duration_cast<seconds>(ft.time_since_epoch()).count();
                if (s < 0) return 0;
                return static_cast<uint64_t>(s);
            }
            catch (...) {
                return 0;
            }
        }

        // List directory entries (files and directories) into out. Returns true on success.
        bool ListDirectory(const std::string& dir, std::vector<std::string>& out) override {
            out.clear();
            // If engine FS provided a directory listing API, call it here.
            // Fallback to host FS directory iteration.
            try {
                for (auto& p : std::filesystem::directory_iterator(std::filesystem::path(dir))) {
                    out.push_back(p.path().generic_string());
                }
                return true;
            }
            catch (...) {
                return false;
            }
        }

    private:
        std::atomic<std::shared_ptr<ReadAllTextFn>>& m_src;
    };


    // runtime + module loader singletons used by the glue adapter
    static std::unique_ptr<ScriptingRuntime> g_runtime;
    static std::unique_ptr<ModuleLoader> g_moduleLoader;
    static std::shared_ptr<ReadAllTextAdapter> g_fsAdapter;

    // atomics to store engine-provided callbacks (same pattern as previous glue)
    static std::atomic<std::shared_ptr<HostLogFn>> g_hostLoggerPtr{ nullptr };
    static std::atomic<std::shared_ptr<ReadAllTextFn>> g_fileReaderPtr{ nullptr };


    // Keep the same lightweight message handler and helper functions that the old glue used.
    // push a message handler *below* the function that will be called.
    static int PushMessageHandlerBelowFunction(lua_State* L, int nargs) {
        int funcIndex = lua_gettop(L) - nargs;
        if (funcIndex < 1) funcIndex = 1;
        lua_pushcfunction(L, [](lua_State* L) -> int {
            const char* msg = lua_tostring(L, 1);
            if (msg) luaL_traceback(L, L, msg, 1);
            else lua_pushliteral(L, "(error object is not a string)");
            return 1;
            });
        lua_insert(L, funcIndex);
        return funcIndex;
    }

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

} // anonymous

// ------------------------- Public API implementations (delegate to ScriptingRuntime) -------------------------

bool Scripting::Init(const InitOptions& opts) {
    if (g_runtime) return true;

    // create adapter that forwards to the ReadAllTextFn atomic
    g_fsAdapter = std::make_shared<ReadAllTextAdapter>(g_fileReaderPtr);

    // build config
    Scripting::ScriptingConfig cfg;
    cfg.createNewVM = opts.createNewVM;
    cfg.openLibs = opts.openLibs;
    // cfg.mainScriptPath left empty by default; caller can pass main script via other means

    g_runtime = std::make_unique<ScriptingRuntime>();

    // initialize runtime with the adapter
    if (!g_runtime->Initialize(cfg, std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter), /*logger*/ nullptr)) 
    {
        g_runtime.reset();
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Scripting::Init - ScriptingRuntime initialization failed");
        return false;
    }

    // forward host logger if already provided by engine
    auto hostSp = g_hostLoggerPtr.load(std::memory_order_acquire);
    if (hostSp && *hostSp) {
        g_runtime->SetHostLogHandler([hostSp](const std::string& s) {
            try { (*hostSp)(s); }
            catch (...) {}
            });
    }

    // create module loader and install it into the runtime's lua_State
    g_moduleLoader = std::make_unique<ModuleLoader>();
    g_moduleLoader->Initialize(std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter));
    // sensible defaults (you can add more via engine config)
    g_moduleLoader->AddSearchPath("Resources/Scripts/?.lua");
    g_moduleLoader->AddSearchPath("Resources/Scripts/?/init.lua");
    g_moduleLoader->AddSearchPath("Resources/extensions/?.lua");
    // install into the runtime lua state. Must be done after runtime created and libs opened.
    lua_State* L = g_runtime->GetLuaState();
    if (L) {
        g_moduleLoader->InstallLuaSearcher(L, -1);
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::Init (via ScriptingRuntime) - done");
    return true;
}

void Scripting::Shutdown() {
    if (g_moduleLoader) {
        // flush modules from current state before we destroy runtime
        if (g_runtime) {
            lua_State* L = g_runtime->GetLuaState();
            if (L) g_moduleLoader->FlushAll(L);
        }
        g_moduleLoader.reset();
    }

    if (g_runtime) {
        g_runtime->Shutdown();
        g_runtime.reset();
    }
    g_fsAdapter.reset();

    // clear callbacks
    g_hostLoggerPtr.store(nullptr, std::memory_order_release);
    g_fileReaderPtr.store(nullptr, std::memory_order_release);

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::Shutdown - done");
}

void Scripting::SetLuaState(lua_State* L) {
    // If user provides a raw lua_State, hand it to runtime (runtime will not own it)
    if (!g_runtime) {
        g_runtime = std::make_unique<ScriptingRuntime>();
        ScriptingConfig cfg;
        cfg.createNewVM = false;
        cfg.openLibs = false;
        // initialize without creating a new VM; runtime will accept provided L via SetLuaState below
        if (!g_runtime->Initialize(cfg, std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter), nullptr)) {
            // If Initialize fails with createNewVM = false, we still proceed to SetLuaState below as best-effort
        }
    }
    g_runtime->SetHostLogHandler(nullptr); // reset; will set below if we have host logger
    g_runtime->FullCollectGarbage(); // harmless no-op if no L

    // set provided L on runtime
    g_runtime->SetHostLogHandler(nullptr); // cleared
    // Use runtime's SetHostLogHandler to reset later; we need to call SetLuaState directly on runtime object.
    g_runtime->SetHostLogHandler(nullptr);
    // There isn't a public SetLuaState on ScriptingRuntime in the header — we can't call it.
    // Instead call Scripting::SetLuaState below (public API) to set the global VM used by glue layer.
    // NOTE: We'll fall back to runtime->Initialize with createNewVM=false alternative above.
    // Simpler: just call Scripting::Shutdown() then Scripting::Init() with createNewVM=false in caller.
    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "SetLuaState called: prefer calling Init(createNewVM=false) or use ScriptingRuntime directly");
}

lua_State* Scripting::GetLuaState() {
    if (!g_runtime) return nullptr;
    return g_runtime->GetLuaState();
}

void Scripting::Tick(float dtSeconds) {
    if (!g_runtime) return;
    g_runtime->Tick(dtSeconds);
}

int Scripting::CreateInstanceFromFile(const std::string& scriptPath) {
    if (!g_runtime) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CreateInstanceFromFile: runtime not initialized");
        return LUA_NOREF;
    }
    lua_State* L = g_runtime->GetLuaState();
    if (!L) return LUA_NOREF;

    int loadStatus = LoadScriptToTop(L, scriptPath);
    if (loadStatus != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "CreateInstanceFromFile - load: ", msg ? msg : "(no msg)"," (script=", scriptPath.c_str(),")");
        lua_pop(L, 1);
        return LUA_NOREF;
    }

    int msghIndex = PushMessageHandlerBelowFunction(L, 0);
    int pcallStatus = lua_pcall(L, 0, 1, msghIndex);
    if (pcallStatus != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "CreateInstanceFromFile - runtime error:", msg ? msg : "(no msg)", " (script=", scriptPath.c_str(), ")");
        lua_pop(L, 1);
        int top = lua_gettop(L);
        if (msghIndex >= 1 && msghIndex <= top) lua_remove(L, msghIndex);
        return LUA_NOREF;
    }
    int top = lua_gettop(L);
    if (msghIndex >= 1 && msghIndex <= top) lua_remove(L, msghIndex);

    if (!lua_istable(L, -1)) {
        lua_newtable(L);
        lua_pushliteral(L, "_returned");
        lua_pushvalue(L, -2);
        lua_settable(L, -3);
        lua_remove(L, -2);
    }
    int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops table
    return ref;
}

void Scripting::DestroyInstance(int instanceRef) {
    if (!g_runtime) return;
    lua_State* L = g_runtime->GetLuaState();
    if (!L) return;
    if (instanceRef == LUA_NOREF) return;
    luaL_unref(L, LUA_REGISTRYINDEX, instanceRef);
}

bool Scripting::IsValidInstance(int instanceRef) {
    return instanceRef != LUA_NOREF;
}

bool Scripting::CallInstanceFunction(int instanceRef, const std::string& funcName) {
    if (!g_runtime) return false;
    lua_State* L = g_runtime->GetLuaState();
    if (!L) return false;
    if (instanceRef == LUA_NOREF) return false;

    // push table
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
    int tableIdx = lua_gettop(L);

    // get field
    lua_getfield(L, tableIdx, funcName.c_str());
    if (lua_isnil(L, -1)) { lua_pop(L, 2); return false; }

    int nargs = 0;
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, tableIdx);
        nargs = 1;
    }
    else {
        if (!lua_getmetatable(L, -1)) { lua_pop(L, 2); return false; }
        lua_getfield(L, -1, "__call");
        lua_remove(L, -2); // remove metatable
        if (!lua_isfunction(L, -1)) { lua_pop(L, 2); return false; }
        lua_insert(L, -2);
        lua_pushvalue(L, tableIdx);
        nargs = 2;
    }

    int msgh = PushMessageHandlerBelowFunction(L, nargs);
    int status = lua_pcall(L, nargs, 0, msgh);
    int top = lua_gettop(L);
    if (msgh >= 1 && msgh <= top) lua_remove(L, msgh);
    if (top >= 1) lua_pop(L, 1); // pop table if present

    return status == LUA_OK;
}

void Scripting::SetHostLogHandler(HostLogFn fn) {
    auto sp = std::make_shared<HostLogFn>(std::move(fn));
    g_hostLoggerPtr.store(sp, std::memory_order_release);
    if (g_runtime) {
        g_runtime->SetHostLogHandler([sp](const std::string& s) {
            try { (*sp)(s); }
            catch (...) {}
            });
    }
}

void Scripting::SetFileSystemReadAllText(ReadAllTextFn fn) {
    auto sp = std::make_shared<ReadAllTextFn>(std::move(fn));
    g_fileReaderPtr.store(sp, std::memory_order_release);

    // (re)create adapter and hand to ModuleLoader and Runtime if present
    g_fsAdapter = std::make_shared<ReadAllTextAdapter>(g_fileReaderPtr);
    if (g_moduleLoader) {
        g_moduleLoader->Initialize(std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter));
        
    }
    // Also update runtime's FS pointer if runtime exists (so runtime and module loader share ownership)
    if (g_runtime) 
    {
        // reinitialize runtime's stored FS to new adapter (keep lua state)
        // simplest: store shared ptr into runtime (if you expose a method), or re-call Initialize with same VM.
        // For now: assume FS changes before initialization; if you support runtime FS swap at runtime,
        // implement a runtime->SetFileSystem(std::shared_ptr<IScriptFileSystem>) function.
      
    }
    // The runtime already took g_fsAdapter at Initialize. If you change FS at runtime,
    // you may want to re-init ModuleLoader and/or re-install searcher. ModuleLoader::Initialize
    // above sets the m_fs pointer so new lookups will use the new reader.
}

std::string Scripting::SerializeInstanceToJson(int instanceRef) {
    if (!g_runtime) return "{}";
    lua_State* L = g_runtime->GetLuaState();
    if (!L || instanceRef == LUA_NOREF) return "{}";
    ScriptSerializer ss;
    return ss.SerializeInstanceToJson(L, instanceRef);
}

bool Scripting::DeserializeJsonToInstance(int instanceRef, const std::string& json) {
    if (!g_runtime) return false;
    lua_State* L = g_runtime->GetLuaState();
    if (!L || instanceRef == LUA_NOREF) return false;
    ScriptSerializer ss;
    return ss.DeserializeJsonToInstance(L, instanceRef, json);
}

void Scripting::RegisterInstancePreserveKeys(int instanceRef, const std::vector<std::string>& keys) {
    static std::unique_ptr<StatePreserver> s_preserver;
    if (!s_preserver) s_preserver = std::make_unique<StatePreserver>();
    s_preserver->RegisterInstanceKeys(instanceRef, keys);
}

std::string Scripting::ExtractInstancePreserveState(int instanceRef) {
    static std::unique_ptr<StatePreserver> s_preserver;
    if (!s_preserver || !g_runtime) return {};
    lua_State* L = g_runtime->GetLuaState();
    if (!L) return {};
    return s_preserver->ExtractState(L, instanceRef);
}

bool Scripting::ReinjectInstancePreserveState(int instanceRef, const std::string& json) {
    static std::unique_ptr<StatePreserver> s_preserver;
    if (!s_preserver || !g_runtime) return false;
    lua_State* L = g_runtime->GetLuaState();
    if (!L) return false;
    return s_preserver->ReinjectState(L, instanceRef, json, nullptr);
}

void Scripting::EnableHotReload(bool enable) {
    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::EnableHotReload(", enable ? 1 : 0,") called (forward to runtime)");
    // The glue itself doesn't track this flag; if you want runtime-level toggles expose them on ScriptingRuntime.
}

void Scripting::RequestReloadNow() {
    if (g_runtime) g_runtime->RequestReload();
}

void Scripting::InitializeCoroutineScheduler() {
    if (!g_runtime) return;
    // ScriptingRuntime already created and initialized coroutine scheduler in Initialize()
    // but expose this for compatibility (no-op).
}

void Scripting::ShutdownCoroutineScheduler() {
    if (!g_runtime) return;
    // runtime will manage coroutine scheduler lifecycle
}
