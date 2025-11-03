// coroutineScheduler.cpp
//
// Implementation of CoroutineScheduler described in coroutineScheduler.h
//
// Notes / guarantees:
//  - All public methods expect to be called from the main thread that owns the lua_State.
//  - The code uses the Lua C API and assumes a Lua version where lua_resume / lua_yield
//    and lua_tothread / lua_xmove are available (Lua 5.2+ API shape).
//
//  - The scheduler stores coroutine threads as registry references in the main state
//    so that coroutines are not collected by GC while scheduled.
//
//  - Yield token parsing follows the convention in the header.

#include "CoroutineScheduler.h"
#include "ScriptingTypes.h"
#include "Logging.hpp"   // use ENGINE_PRINT for debug/warn/info
#include <cassert>
#include <cstring>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Scripting {

    namespace {

        // Portable wrapper for lua_resume across Lua versions.
        //
        // Lua versions:
        //  - Lua 5.1: int lua_resume(lua_State *co, int nargs);
        //  - Lua 5.2/5.3: int lua_resume(lua_State *co, lua_State *from, int nargs);
        //  - Lua 5.4+: int lua_resume(lua_State *co, lua_State *from, int nargs, int *nresults).
        //
        // We abstract differences here so the rest of the code can call resume_coroutine(...)
        static int resume_coroutine(lua_State* co, lua_State* from, int nargs, int* out_nresults = nullptr) 
        {
            #if defined(LUA_VERSION_NUM)
            #if LUA_VERSION_NUM >= 504
                // Lua 5.4+: provide nresults pointer
                int nres = 0;
                int status = lua_resume(co, from, nargs, &nres);
                if (out_nresults) *out_nresults = nres;
                return status;
            #elif LUA_VERSION_NUM >= 502
                // Lua 5.2 and 5.3
                (void)out_nresults;
                return lua_resume(co, from, nargs);
            #else
                // Lua 5.1: signature is lua_resume(co, nargs)
                (void)from;
                (void)out_nresults;
                return lua_resume(co, nargs);
            #endif
            #else
                // If LUA_VERSION_NUM is not defined, conservatively assume modern API (5.4)
                int nres = 0;
                int status = lua_resume(co, from, nargs, &nres);
                if (out_nresults) *out_nresults = nres;
                return status;
            #endif
        }

        // key used to store scheduler pointer in the Lua registry
        const char* kSchedulerRegistryKey = "CoroutineSchedulerPtr";

        // helper to fetch scheduler pointer from Lua registry (or nullptr)
        CoroutineScheduler* GetSchedulerFromLua(lua_State* L) {
            lua_getfield(L, LUA_REGISTRYINDEX, kSchedulerRegistryKey);
            CoroutineScheduler* s = nullptr;
            if (lua_islightuserdata(L, -1)) {
                s = reinterpret_cast<CoroutineScheduler*>(lua_touserdata(L, -1));
            }
            lua_pop(L, 1);
            return s;
        }

        // Small helper to check truthiness of Lua stack value at idx.
        static bool lua_is_truthy(lua_State* L, int idx) {
            int t = lua_type(L, idx);
            if (t == LUA_TBOOLEAN) return lua_toboolean(L, idx) != 0;
            if (t == LUA_TNUMBER) return lua_tonumber(L, idx) != 0.0;
            if (t == LUA_TSTRING) return lua_tostring(L, idx)[0] != '\0';
            return (t != LUA_TNIL);
        }
    } // namespace (anonymous)

    struct CoroutineScheduler::Entry {
        CoroutineId id = InvalidCoroutineId;
        int threadRef = LUA_NOREF; // registry ref for the thread object (in mainL registry)
        float waitSeconds = 0.0f;
        int waitFrames = 0;
        int untilFuncRef = LUA_NOREF; // optional predicate (in mainL registry)
        bool waitingUntil = false;
        bool finished = false;
    };

    //////////////////////////////////////////////////////////////////////////
    // Lua binding: StartCoroutine(func, ...)
    //
    // Called from Lua: it will create a new coroutine (lua_newthread), move the function and
    // arguments to it, and start it immediately (first resume).
    //
    // Returns: coroutine id (number)
    static int Lua_StartCoroutine(lua_State* L) {
        CoroutineScheduler* scheduler = GetSchedulerFromLua(L);
        if (!scheduler) {
            lua_pushnil(L);
            lua_pushstring(L, "CoroutineScheduler not registered");
            return 2;
        }
        int nargs = lua_gettop(L); // function + args
        CoroutineId id = scheduler->StartCoroutineFromStack(nargs);
        if (id == InvalidCoroutineId) {
            lua_pushnil(L);
            lua_pushstring(L, "failed to start coroutine");
            return 2;
        }
        lua_pushinteger(L, static_cast<lua_Integer>(id));
        return 1;
    }

    //////////////////////////////////////////////////////////////////////////
    // CoroutineScheduler implementation
    //////////////////////////////////////////////////////////////////////////

    CoroutineScheduler::CoroutineScheduler()
        : m_mainL(nullptr), m_nextId(1), m_running(false) {
    }

    CoroutineScheduler::~CoroutineScheduler() {
        Shutdown();
    }

    void CoroutineScheduler::Initialize(lua_State* mainL) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_running) return;
        m_mainL = mainL;
        // store pointer to scheduler in registry so static binding can find it
        lua_pushlightuserdata(m_mainL, reinterpret_cast<void*>(this));
        lua_setfield(m_mainL, LUA_REGISTRYINDEX, kSchedulerRegistryKey);

        RegisterBindings();
        m_running = true;
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "CoroutineScheduler initialized");
    }

    void CoroutineScheduler::RegisterBindings() {
        if (!m_mainL) return;
        // create global StartCoroutine binding
        lua_pushcfunction(m_mainL, &Lua_StartCoroutine);
        lua_setglobal(m_mainL, "StartCoroutine");
    }

    void CoroutineScheduler::Shutdown() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_running) return;

        // remove registry pointer
        if (m_mainL) {
            lua_pushnil(m_mainL);
            lua_setfield(m_mainL, LUA_REGISTRYINDEX, kSchedulerRegistryKey);
        }

        // Stop all coroutines and free refs
        StopAll();

        // remove StartCoroutine global (optional)
        if (m_mainL) {
            lua_pushnil(m_mainL);
            lua_setglobal(m_mainL, "StartCoroutine");
        }

        m_mainL = nullptr;
        m_running = false;
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "CoroutineScheduler shutdown");
    }

    bool CoroutineScheduler::IsRunning() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_running;
    }

    void CoroutineScheduler::StopAll() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_mainL) {
            m_coroutines.clear();
            return;
        }

        // Unref everything in main registry
        for (auto& e : m_coroutines) {
            if (e.threadRef != LUA_NOREF) {
                luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.threadRef);
                e.threadRef = LUA_NOREF;
            }
            if (e.untilFuncRef != LUA_NOREF) {
                luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.untilFuncRef);
                e.untilFuncRef = LUA_NOREF;
                e.waitingUntil = false;
            }
        }
        m_coroutines.clear();
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "CoroutineScheduler: stopped all coroutines");
    }

    bool CoroutineScheduler::StopCoroutine(CoroutineId id) {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (size_t i = 0; i < m_coroutines.size(); ++i) {
            if (m_coroutines[i].id == id) {
                // cleanup
                if (m_mainL) {
                    if (m_coroutines[i].threadRef != LUA_NOREF) {
                        luaL_unref(m_mainL, LUA_REGISTRYINDEX, m_coroutines[i].threadRef);
                    }
                    if (m_coroutines[i].untilFuncRef != LUA_NOREF) {
                        luaL_unref(m_mainL, LUA_REGISTRYINDEX, m_coroutines[i].untilFuncRef);
                    }
                }
                // erase entry
                m_coroutines.erase(m_coroutines.begin() + i);
                ENGINE_PRINT(EngineLogging::LogLevel::Info, "CoroutineScheduler: stopped coroutine ", id);
                return true;
            }
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    // Internal helper used by Lua_StartCoroutine binding
    //
    // Expects (func, ...args) on the main stack. It will:
    //  - create a new thread (lua_newthread) and push one on stack,
    //  - push copies of func+args, move them into the new thread (lua_xmove),
    //  - create a registry ref to the thread (so it won't be GC'd),
    //  - immediately resume the coroutine once (passing the args).
    // The function returns the new CoroutineId (or InvalidCoroutineId on failure).
    //////////////////////////////////////////////////////////////////////////
    CoroutineScheduler::CoroutineId CoroutineScheduler::StartCoroutineFromStack(int nargs) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_running || !m_mainL) return InvalidCoroutineId;
        if (nargs < 1) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "StartCoroutine: expected function as first argument");
            return InvalidCoroutineId;
        }

        // 1) create thread and get its stack index
        lua_State* L = m_mainL;
        int origTop = lua_gettop(L); // should equal nargs
        // create coroutine thread; this pushes the thread object onto L
        lua_State* co = lua_newthread(L);
        if (!co) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "StartCoroutine: lua_newthread failed");
            return InvalidCoroutineId;
        }
        int threadIdx = lua_gettop(L); // index where thread object sits on main stack

        // 2) push copies of (func + args) to the top of the main stack, then move them to 'co'
        for (int i = 1; i <= origTop; ++i) {
            lua_pushvalue(L, i);
        }
        // Move the copies (origTop values) to the coroutine 'co'
        lua_xmove(L, co, origTop);

        // 3) create a persistent registry ref for the thread so it won't be collected
        // Push a copy of the thread object onto the stack and ref it (luaL_ref pops it).
        lua_pushvalue(L, threadIdx);
        int threadRef = luaL_ref(L, LUA_REGISTRYINDEX);

        // Now remove the original thread object that remains on the stack at threadIdx.
        // After the luaL_ref pop above, the stack is unchanged except for that popped copy,
        // so the original thread is still valid at threadIdx. Remove it to keep stack clean.
        // (lua_remove shifts stack down)
        lua_remove(L, threadIdx);

        // 4) create scheduler entry
        Entry entry;
        entry.id = m_nextId++;
        entry.threadRef = threadRef;
        entry.waitSeconds = 0.0f;
        entry.waitFrames = 0;
        entry.untilFuncRef = LUA_NOREF;
        entry.waitingUntil = false;
        entry.finished = false;

        // 5) start the coroutine by resuming once (arg count is origTop - 1 -- function + args)
        int argCount = origTop - 1;
        int dummy_nres = 0;
        int status = resume_coroutine(co, L, argCount, &dummy_nres);
        if (status == LUA_OK) {
            // finished immediately
            // no need to keep registry ref
            if (entry.threadRef != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, entry.threadRef);
                entry.threadRef = LUA_NOREF;
            }
            entry.finished = true;
            // do not add to list; it already finished
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "StartCoroutine: coroutine finished immediately");
            return InvalidCoroutineId; // caller probably doesn't need id; but spec: return 0 to indicate no active coroutine
        }
        else if (status == LUA_YIELD) {
            // parse yield tokens to schedule semantics
            int yielded = lua_gettop(co);
            if (yielded >= 1 && lua_isstring(co, 1)) {
                const char* tag = lua_tostring(co, 1);
                if (tag && std::strcmp(tag, "wait_seconds") == 0 && yielded >= 2 && lua_isnumber(co, 2)) {
                    entry.waitSeconds = static_cast<float>(lua_tonumber(co, 2));
                }
                else if (tag && std::strcmp(tag, "wait_frames") == 0 && yielded >= 2 && lua_isinteger(co, 2)) {
                    entry.waitFrames = static_cast<int>(lua_tointeger(co, 2));
                }
                else if (tag && std::strcmp(tag, "wait_until") == 0 && yielded >= 2 && lua_isfunction(co, 2)) {
                    // move the predicate function from 'co' to the main state and store a registry ref
                    lua_pushvalue(co, 2);                 // copy predicate on 'co'
                    lua_xmove(co, L, 1);                  // move it to main state
                    entry.untilFuncRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops function
                    entry.waitingUntil = true;
                }
                else {
                    // unknown tag -- treat as plain yield (resume next tick)
                    entry.waitSeconds = 0.0f;
                }
            }
            else {
                // plain yield: resume next tick
                entry.waitSeconds = 0.0f;
            }

            // store entry
            m_coroutines.push_back(entry);
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "StartCoroutine: scheduled coroutine id ", entry.id);
            return entry.id;
        }
        else {
            // error
            const char* msg = lua_tostring(co, -1);
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "StartCoroutine: coroutine error: ", msg ? msg : "(no msg)");
            lua_pop(co, 1);
            // free thread ref
            if (entry.threadRef != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, entry.threadRef);
            }
            return InvalidCoroutineId;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Tick: iterate coroutines and resume those that are ready.
    //////////////////////////////////////////////////////////////////////////
    void CoroutineScheduler::Tick(float dtSeconds) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_running || !m_mainL) return;

        // iterate with index because we may erase finished entries
        size_t i = 0;
        while (i < m_coroutines.size()) {
            Entry& e = m_coroutines[i];
            // skip if already finished (shouldn't happen normally)
            if (e.finished) {
                cleanupEntry(i);
                continue;
            }

            bool shouldResume = false;
            // tick timers/frames
            if (e.waitingUntil) {
                // call predicate stored in main registry
                if (e.untilFuncRef != LUA_NOREF) {
                    // push the function
                    lua_rawgeti(m_mainL, LUA_REGISTRYINDEX, e.untilFuncRef);
                    // call it with 0 args, expect 1 result
                    if (lua_pcall(m_mainL, 0, 1, 0) != LUA_OK) {
                        const char* msg = lua_tostring(m_mainL, -1);
                        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CoroutineScheduler: wait_until predicate error: ", msg ? msg : "(no msg)");
                        lua_pop(m_mainL, 1);
                        // treat error as false and continue waiting
                        shouldResume = false;
                    }
                    else {
                        bool ok = lua_is_truthy(m_mainL, -1);
                        lua_pop(m_mainL, 1);
                        if (ok) shouldResume = true;
                    }
                }
                else {
                    // no predicate function; resume immediately
                    shouldResume = true;
                }
            }
            else if (e.waitFrames > 0) {
                e.waitFrames -= 1;
                if (e.waitFrames <= 0) shouldResume = true;
            }
            else if (e.waitSeconds > 0.0f) {
                e.waitSeconds -= dtSeconds;
                if (e.waitSeconds <= 0.0f) shouldResume = true;
            }
            else {
                // no wait condition -> resume immediately (yielded plain)
                shouldResume = true;
            }

            if (!shouldResume) {
                ++i;
                continue;
            }

            // get coroutine thread from registry and resume it
            lua_rawgeti(m_mainL, LUA_REGISTRYINDEX, e.threadRef);
            // convert the object on the stack to a lua_State* thread
            lua_State* co = lua_tothread(m_mainL, -1);
            lua_pop(m_mainL, 1); // pop thread object copy

            if (!co) {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "CoroutineScheduler: invalid thread for coroutine ", e.id);
                cleanupEntry(i);
                continue;
            }

            int dummy_nres = 0;
            int status = resume_coroutine(co, m_mainL, 0, &dummy_nres);
            if (status == LUA_OK) {
                // coroutine finished; cleanup (unref thread and predicate)
                if (e.threadRef != LUA_NOREF) {
                    luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.threadRef);
                    e.threadRef = LUA_NOREF;
                }
                if (e.untilFuncRef != LUA_NOREF) {
                    luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.untilFuncRef);
                    e.untilFuncRef = LUA_NOREF;
                }
                ENGINE_PRINT(EngineLogging::LogLevel::Info, "CoroutineScheduler: coroutine ", e.id," finished");
                // erase this entry
                m_coroutines.erase(m_coroutines.begin() + i);
                continue; // do not increment i (element removed)
            }
            else if (status == LUA_YIELD) {
                // parse yield tokens again to schedule next resume
                int yielded = lua_gettop(co);
                e.waitSeconds = 0.0f;
                e.waitFrames = 0;
                e.waitingUntil = false;
                if (e.untilFuncRef != LUA_NOREF) {
                    // we had a previous predicate; if not changed, it will remain registered.
                    // We leave it as-is; caller may yield a new predicate to replace it.
                    luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.untilFuncRef);
                    e.untilFuncRef = LUA_NOREF;
                }

                if (yielded >= 1 && lua_isstring(co, 1)) {
                    const char* tag = lua_tostring(co, 1);
                    if (tag && std::strcmp(tag, "wait_seconds") == 0 && yielded >= 2 && lua_isnumber(co, 2)) {
                        e.waitSeconds = static_cast<float>(lua_tonumber(co, 2));
                    }
                    else if (tag && std::strcmp(tag, "wait_frames") == 0 && yielded >= 2 && lua_isinteger(co, 2)) {
                        e.waitFrames = static_cast<int>(lua_tointeger(co, 2));
                    }
                    else if (tag && std::strcmp(tag, "wait_until") == 0 && yielded >= 2 && lua_isfunction(co, 2)) {
                        // move predicate function from co to main and ref it
                        lua_pushvalue(co, 2);
                        lua_xmove(co, m_mainL, 1);
                        e.untilFuncRef = luaL_ref(m_mainL, LUA_REGISTRYINDEX);
                        e.waitingUntil = true;
                    }
                    else {
                        // plain yield -> resume next tick
                        e.waitSeconds = 0.0f;
                    }
                }
                else {
                    // plain yield
                    e.waitSeconds = 0.0f;
                }

                // leave entry in list, increment to next
                ++i;
                continue;
            }
            else {
                // error
                const char* msg = lua_tostring(co, -1);
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "CoroutineScheduler: coroutine %u error: ", e.id, msg ? msg : "(no msg)");
                lua_pop(co, 1);
                // cleanup refs
                if (e.threadRef != LUA_NOREF) {
                    luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.threadRef);
                    e.threadRef = LUA_NOREF;
                }
                if (e.untilFuncRef != LUA_NOREF) {
                    luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.untilFuncRef);
                    e.untilFuncRef = LUA_NOREF;
                }
                // remove entry
                m_coroutines.erase(m_coroutines.begin() + i);
                continue;
            }
        } // while
    }

    void CoroutineScheduler::cleanupEntry(size_t index) {
        if (index >= m_coroutines.size()) return;
        Entry& e = m_coroutines[index];
        if (m_mainL) {
            if (e.threadRef != LUA_NOREF) {
                luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.threadRef);
                e.threadRef = LUA_NOREF;
            }
            if (e.untilFuncRef != LUA_NOREF) {
                luaL_unref(m_mainL, LUA_REGISTRYINDEX, e.untilFuncRef);
                e.untilFuncRef = LUA_NOREF;
            }
        }
        m_coroutines.erase(m_coroutines.begin() + index);
    }

} // namespace Scripting
