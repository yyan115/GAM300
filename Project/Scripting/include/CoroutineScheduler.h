#pragma once
// CoroutineScheduler.h
//
// A simple coroutine scheduler for Lua coroutines (lua_State threads).
// - Provides StartCoroutine(...) binding for Lua and a lightweight C++ API.
// - Scheduling tokens (yield conventions) are documented below.
//
// Yield convention (from Lua):
//  coroutine.yield("wait_seconds", number) -- wait N seconds
//  coroutine.yield("wait_frames", integer) -- wait N frames (Tick invocations)
//  coroutine.yield("wait_until", function) -- call function() each Tick; resume when truthy
//  coroutine.yield(...) -- any other yield is treated as a plain yield and the coroutine
//                         will be resumed on the next Tick.
//
// Lifetime & threading:
//  - Scheduler must be initialized with the main lua_State* and used on the same thread
//    that owns that lua_State (i.e. the main thread).
//  - Coroutines are kept alive via registry references in the main Lua state.
//  - When a coroutine finishes (returns) or is stopped, all registry refs created for it are released.
//
// Usage (Lua):
//   co = StartCoroutine(function()
//       print("begin")
//       coroutine.yield("wait_seconds", 0.5)
//       print("after half second")
//   end, arg1, arg2, ...)
//
// Usage (C++):
//   Scheduler.RegisterBindings(); // installs StartCoroutine in Lua global
//   Scheduler.Initialize(mainL);
//   Scheduler.Tick(dt);
//

#include <vector>
#include <mutex>
#include <cstdint>

extern "C" {
    struct lua_State;
}

namespace Scripting {

    class CoroutineScheduler {
    public:
        using CoroutineId = uint32_t;

        CoroutineScheduler();
        ~CoroutineScheduler();

        // Initialize the scheduler with the main Lua state (must be called on the main thread).
        // RegisterBindings() is optional but convenient to call from Initialize().
        void Initialize(lua_State* mainL);

        // Remove bindings and clean up. After Shutdown the scheduler may be re-initialized.
        void Shutdown();

        // Registers Lua bindings (StartCoroutine). Safe to call after Initialize().
        void RegisterBindings();

        // Tick the scheduler (main-thread). Advance timers and resume coroutines when ready.
        void Tick(float dtSeconds);

        // Stop all coroutines and clear internal state.
        void StopAll();

        // Stop a particular coroutine (if it exists). Returns true if it was found and stopped.
        bool StopCoroutine(CoroutineId id);

        // Start a coroutine from the current contents of the main Lua stack.
        // Expects (func, ...args) on the main stack and returns the new CoroutineId,
        // or InvalidCoroutineId on error. This is intentionally public so Lua bindings
        // (which are free functions) can call it.
        CoroutineId StartCoroutineFromStack(int nargs);

        // Query helpers
        bool IsRunning() const;

    private:
        // non-copyable
        CoroutineScheduler(const CoroutineScheduler&) = delete;
        CoroutineScheduler& operator=(const CoroutineScheduler&) = delete;

        struct Entry;
        void cleanupEntry(size_t index);

        lua_State* m_mainL = nullptr;
        std::vector<Entry> m_coroutines;
        CoroutineId m_nextId;
        bool m_running;
        mutable std::mutex m_mutex;
    };

} // namespace Scripting
