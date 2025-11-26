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

#ifdef ANDROID
    // Android doesn't support std::atomic<std::shared_ptr<>>, use mutex instead
    template<typename T>
    class AtomicSharedPtr {
    public:
        AtomicSharedPtr(std::shared_ptr<T> ptr = nullptr) : m_ptr(std::move(ptr)) {}

        std::shared_ptr<T> load() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_ptr;
        }

        void store(std::shared_ptr<T> ptr) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ptr = std::move(ptr);
        }

    private:
        mutable std::mutex m_mutex;
        std::shared_ptr<T> m_ptr;
    };
#else
    // Windows/Desktop: use std::atomic<std::shared_ptr<>>
    template<typename T>
    using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;
#endif

    // adapter object implementing IScriptFileSystem that forwards to ReadAllTextFn
    struct ReadAllTextAdapter : public IScriptFileSystem {
        ReadAllTextAdapter(AtomicSharedPtr<ReadAllTextFn>& src)
            : m_src(src) {
        }

        // Read entire file contents. Prefer engine callback; fall back to host FS.
    bool ReadAllText(const std::string& path, std::string& out) override {
            ENGINE_LOG_INFO("Scripting: Reading file: " + path);

#ifdef ANDROID
            auto sp = m_src.load();
#else
            auto sp = m_src.load(std::memory_order_acquire);
#endif

            if (sp && *sp) {
                try {
                    if ((*sp)(path, out)) return true;
                }
                catch (...) {
                    ENGINE_LOG_INFO("Custom loader failed, falling back to filesystem");
                }
            }

            // Fallback: try reading from host filesystem
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) {
                ENGINE_LOG_INFO("Failed to open file: " + path);
                return false;
            }

            // More efficient reading for both platforms
            ifs.seekg(0, std::ios::end);
            std::streamsize size = ifs.tellg();
            if (size < 0) return false;

            ifs.seekg(0, std::ios::beg);
            out.resize(static_cast<size_t>(size));

            if (size > 0 && !ifs.read(&out[0], size)) {
                return false;
            }

            return true;
        }

        // Check existence: prefer engine callback probe, else host FS check
        bool Exists(const std::string& path) override {
#ifdef ANDROID
            auto sp = m_src.load();
#else
            auto sp = m_src.load(std::memory_order_acquire);
#endif
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
        AtomicSharedPtr<ReadAllTextFn>& m_src;
    };


    // runtime + module loader singletons used by the glue adapter
    static std::unique_ptr<ScriptingRuntime> g_runtime;
    static std::shared_ptr<ReadAllTextAdapter> g_fsAdapter;

    // atomics to store engine-provided callbacks (same pattern as previous glue)
    static AtomicSharedPtr<HostLogFn> g_hostLoggerPtr{ nullptr };
    static AtomicSharedPtr<ReadAllTextFn> g_fileReaderPtr{ nullptr };
    static AtomicSharedPtr<HostGetComponentFn> g_hostGetComponentPtr{ nullptr };

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
#ifdef ANDROID
        auto readerShared = g_fileReaderPtr.load();
#else
        auto readerShared = g_fileReaderPtr.load(std::memory_order_acquire);
#endif
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

    g_fsAdapter = std::make_shared<ReadAllTextAdapter>(g_fileReaderPtr);

    Scripting::ScriptingConfig cfg;
    cfg.createNewVM = opts.createNewVM;
    cfg.openLibs = opts.openLibs;

    g_runtime = std::make_unique<ScriptingRuntime>();

    if (!g_runtime->Initialize(cfg, std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter), nullptr))
    {
        g_runtime.reset();
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Scripting::Init - ScriptingRuntime initialization failed");
        return false;
    }

    // Forward host logger if already provided
#ifdef ANDROID
    auto hostSp = g_hostLoggerPtr.load();
#else
    auto hostSp = g_hostLoggerPtr.load(std::memory_order_acquire);
#endif
    if (hostSp && *hostSp) {
        g_runtime->SetHostLogHandler([hostSp](const std::string& s) {
            try { (*hostSp)(s); }
            catch (...) {}
            });
    }

    //OUTDATED DONE IN SCRIPTINGRUNTIME
    //// Configure module loader search paths (runtime owns the module loader)
    //if (auto* loader = g_runtime->GetModuleLoader()) {
    //    loader->AddSearchPath("Resources/Scripts/?.lua");
    //    loader->AddSearchPath("Resources/Scripts/?/init.lua");
    //    loader->AddSearchPath("Resources/Scripts/extension/?.lua");
    //}

    // Redirect Lua print
    lua_State* L = g_runtime->GetLuaState();
    if (L) {
        lua_pushcfunction(L, [](lua_State* L) -> int {
            int n = lua_gettop(L);
            std::string msg;
            for (int i = 1; i <= n; i++) {
                if (i > 1) msg += "\t";
                const char* s = lua_tostring(L, i);
                msg += (s ? s : "(non-string)");
            }
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "[LUA] ", msg);
            return 0;
            });
        lua_setglobal(L, "print");
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::Init complete");
    return true;
}

void Scripting::Shutdown() {
    // REMOVED: Don't flush g_moduleLoader since we deleted it
    // The runtime will handle its own ModuleLoader cleanup

    if (g_runtime) {
        g_runtime->Shutdown();
        g_runtime.reset();
    }
    g_fsAdapter.reset();

    // clear callbacks
#ifdef ANDROID
    g_hostLoggerPtr.store(nullptr);
    g_fileReaderPtr.store(nullptr);
#else
    g_hostLoggerPtr.store(nullptr, std::memory_order_release);
    g_fileReaderPtr.store(nullptr, std::memory_order_release);
#endif

    ENGINE_PRINT(EngineLogging::LogLevel::Info, "Scripting::Shutdown - done");
}

void Scripting::SetFileSystemReadAllText(ReadAllTextFn fn) {
    auto sp = std::make_shared<ReadAllTextFn>(std::move(fn));
#ifdef ANDROID
    g_fileReaderPtr.store(sp);
#else
    g_fileReaderPtr.store(sp, std::memory_order_release);
#endif

    // (re)create adapter and hand to ModuleLoader and Runtime if present
    g_fsAdapter = std::make_shared<ReadAllTextAdapter>(g_fileReaderPtr);

    if (g_runtime) {
        // Update module loader to use the new adapter
        if (auto* loader = g_runtime->GetModuleLoader()) {
            try {
                loader->Initialize(std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter));
            }
            catch (...) {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "Scripting::SetFileSystemReadAllText: ModuleLoader::Initialize threw");
            }
        }

        // Also inform the runtime itself so it can update its m_fsShared/m_fs and reinstall searcher
        try {
            g_runtime->SetFileSystem(std::static_pointer_cast<IScriptFileSystem>(g_fsAdapter));
        }
        catch (...) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "Scripting::SetFileSystemReadAllText: g_runtime->SetFileSystem threw");
        }
    }
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
    // There isn't a public SetLuaState on ScriptingRuntime in the header � we can't call it.
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

    int base = lua_gettop(L); // remember stack top for clean restore

    // push registry value
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef); // +1 -> instance-or-wrapper
    if (lua_gettop(L) == base) { // nothing pushed?
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CallInstanceFunction: registry ref push failed");
        return false;
    }

    // allow both tables and userdata wrappers (some Component wrappers return userdata)
    int instIndex = lua_gettop(L);
    const char* instType = luaL_typename(L, instIndex);
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "CallInstanceFunction: instanceRef=", instanceRef, " pushed type=", instType);

    // Attempt normal field lookup first (this uses __index metamethods)
    lua_getfield(L, instIndex, funcName.c_str()); // +1 -> field or nil
    if (lua_isnil(L, -1)) {
        // maybe this is a wrapper table that put the real object under "_returned"
        lua_pop(L, 1); // pop nil
        if (lua_istable(L, instIndex)) {
            lua_getfield(L, instIndex, "_returned"); // +1
            if (!lua_isnil(L, -1)) {
                // try to get the method from _returned
                int retIdx = lua_gettop(L);
                const char* retType = luaL_typename(L, retIdx);
                ENGINE_PRINT(EngineLogging::LogLevel::Debug, "CallInstanceFunction: wrapper has _returned type=", retType);
                lua_getfield(L, retIdx, funcName.c_str()); // +1
                // If still nil, fall through to error handling below.
            }
            else {
                // no _returned; leave stack as is (we'll clean up)
                lua_pop(L, 1); // pop _returned nil
            }
        }
    }

    // now -1 should be the candidate callable (function or callable-object) or nil
    if (lua_isnil(L, -1)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CallInstanceFunction: method '", funcName.c_str(), "' not found on instanceRef=", instanceRef);
        lua_settop(L, base);
        return false;
    }

    int nargs = 0;
    // If it's a plain function, push self (the appropriate self value)
    if (lua_isfunction(L, -1)) {
        // determine which 'self' we should pass:
        // - if we looked up the method directly on the original instance, pass that
        // - if we looked up on _returned, pass _returned
        // Find the proper "self" on stack: search from top down for a non-function value we pushed earlier
        // Stack shapes we can have:
        // 1) base..., instance, function
        // 2) base..., instance, _returned, function
        // We'll push the nearest non-function value below the function as self.
        int funcPos = lua_gettop(L);
        int candidateSelfPos = funcPos - 1;
        if (candidateSelfPos >= base + 1) {
            lua_pushvalue(L, candidateSelfPos); // push self
            nargs = 1;
        }
        else {
            // as a fallback push the original instance
            lua_pushvalue(L, instIndex);
            nargs = 1;
        }
    }
    else {
        // not a function object; try to find a __call metamethod on the candidate object
        if (!lua_getmetatable(L, -1)) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CallInstanceFunction: method candidate not callable and has no metatable");
            lua_settop(L, base);
            return false;
        }
        // metatable now on top, get __call
        lua_getfield(L, -1, "__call"); // +1
        if (!lua_isfunction(L, -1)) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CallInstanceFunction: field has metatable but no __call");
            lua_settop(L, base);
            return false;
        }
        // stack now: ..., instance, [maybe _returned], candidate, metatable, __call
        // rearrange to: ..., __call, candidate, self
        lua_remove(L, -2);               // remove metatable -> ..., candidate, __call
        lua_insert(L, -2);               // move __call below candidate -> ..., __call, candidate
        // choose self (prefer candidate's parent if present). We want to pass (candidate, self)
        // push self (original instance or _returned if present)
        // find the first non-function below top (candidate is at -2 now)
        int top = lua_gettop(L);
        int candidatePos = top - 1;
        int selfPos = candidatePos - 1;
        if (selfPos >= base + 1) {
            lua_pushvalue(L, selfPos);
        }
        else {
            lua_pushvalue(L, instIndex);
        }
        nargs = 2;
    }

    // push message handler below function (we pass nargs computed above)
    int msgh = PushMessageHandlerBelowFunction(L, nargs);
    int status = lua_pcall(L, nargs, 0, msgh);

    if (status != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "CallInstanceFunction: lua_pcall error: ", err ? err : "(null)");
        // pop error
        lua_pop(L, 1);
    }

    // restore stack cleanly
    lua_settop(L, base);
    return status == LUA_OK;
}


void Scripting::SetHostLogHandler(HostLogFn fn) {
    auto sp = std::make_shared<HostLogFn>(std::move(fn));
#ifdef ANDROID
    g_hostLoggerPtr.store(sp);
#else
    g_hostLoggerPtr.store(sp, std::memory_order_release);
#endif
    if (g_runtime) {
        g_runtime->SetHostLogHandler([sp](const std::string& s) {
            try { (*sp)(s); }
            catch (...) {}
            });
    }
}

void Scripting::SetHostGetComponentHandler(HostGetComponentFn fn) {
    auto sp = std::make_shared<HostGetComponentFn>(std::move(fn));
#ifdef ANDROID
    g_hostGetComponentPtr.store(sp);
#else
    g_hostGetComponentPtr.store(sp, std::memory_order_release);
#endif
    if (g_runtime) {
        // forward to runtime (runtime exposes setter implemented below)
        g_runtime->SetHostGetComponentHandler([sp](lua_State* L, uint32_t entityId, const std::string& compName) -> bool {
            try { if (sp && *sp) return (*sp)(L, entityId, compName); }
            catch (...) {}
            return false;
            });
    }
}

// Bind instance table (registry ref) to an entity id by setting 'entityId' and attaching GetComponent method.
// Returns false on error.
bool Scripting::BindInstanceToEntity(int instanceRef, uint32_t entityId) {
    if (!g_runtime) return false;
    lua_State* L = g_runtime->GetLuaState();
    if (!L) return false;
    if (instanceRef == LUA_NOREF) return false;

    // push instance table
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef); // +1
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    // set instance.entityId = entityId
    lua_pushinteger(L, static_cast<lua_Integer>(entityId));
    lua_setfield(L, -2, "entityId");

    // call the runtime-provided helper __engine_bind_instance_helpers(instance)
    // The helper is installed at runtime init in register_core_bindings (see ScriptingRuntime).
    lua_getglobal(L, "__engine_bind_instance_helpers"); // pushes function or nil
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, -2); // push instance as arg
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "BindInstanceToEntity: __engine_bind_instance_helpers failed: ", err ? err : "(no msg)");
            lua_pop(L, 1); // pop error
            lua_pop(L, 1); // pop instance
            return false;
        }
    }
    else {
        lua_pop(L, 1); // pop non-function
        // fallback: create method in-place
        const char* helper =
            "local function __tmp_bind(inst)\n"
            "  function inst:GetComponent(name)\n"
            "    return GetComponent(inst.entityId, name)\n"
            "  end\n"
            "end\n"
            "__tmp_bind";
        if (luaL_loadstring(L, helper) != LUA_OK) {
            lua_pop(L, 1); // pop instance
            return false;
        }
        // stack: instance, function
        lua_pushvalue(L, -1); // duplicate function to call
        lua_pushvalue(L, -3); // instance (duplicate of original instance pushed earlier)
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "BindInstanceToEntity fallback helper failed: ", err ? err : "(no msg)");
            lua_pop(L, 1);
            lua_pop(L, 1);
            return false;
        }
    }

    lua_pop(L, 1); // pop instance
    return true;
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
