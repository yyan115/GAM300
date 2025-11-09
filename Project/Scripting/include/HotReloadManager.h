#pragma once
// HotReloadManager.h
//
// Public-ish API to request hot reloads and query reload status.
//
// Responsibilities:
//  - Watch files or directories for changes and signal reload requests (does NOT perform reload).
//  - Provide a joinable, well-behaved watcher thread and a minimal API for the main thread
//    to poll and consume reload requests.
//  - Provide configurable debounce period and polling interval (for platforms without native watchers).
//
// Threading / safety notes:
//  - The watcher thread performs only filesystem queries and sets atomic flags / enqueues events.
//  - The main thread must call Poll() or QueryAndClear() to consume events and to perform reload actions,
//    ensuring no Lua operations are performed from the watcher thread.
//  - Call Start() on main thread to create the watcher and Stop() on main thread to join it.
//  - RequestReload() is thread-safe and will schedule a reload on the main thread.

#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <memory>

namespace Scripting {

    // Forward-declare the IScriptFileSystem pointer type so callers of this header do not need
    // to include the full ScriptingRuntime header.
    struct IScriptFileSystem;

    struct HotReloadConfig {
        std::vector<std::string> paths;
        unsigned int pollIntervalMs = 250;
        unsigned int debounceMs = 200;
        bool enabled = true;
    };

    struct HotReloadEvent {
        std::string path;
        uint64_t timestamp = 0;
    };

    class HotReloadManager {
    public:
        using ChangeCallback = std::function<void(const HotReloadEvent&)>;

        HotReloadManager();
        ~HotReloadManager();

        // Initialize the watcher. If 'fs' is non-null the manager will use it but will NOT take ownership.
        // If 'fs' is null the manager will create and own a default filesystem instance.
        bool Start(const HotReloadConfig& cfg, IScriptFileSystem* fs = nullptr);

        // Stop the watcher and join the thread. Safe to call multiple times.
        void Stop();

        // Request a reload manually (thread-safe).
        void RequestReload(const std::string& reason = std::string());

        // Poll for events on the main thread; returns list of HotReloadEvent since the last Poll.
        // Poll() will also invoke any registered change callback on the calling thread for convenience.
        std::vector<HotReloadEvent> Poll();

        // Register a callback that will be invoked when Poll() encounters events.
        void SetChangeCallback(ChangeCallback cb);

        bool IsRunning() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Scripting