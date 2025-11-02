// HotReloadManager.cpp
//
// Implementation of HotReloadManager. This module creates a joinable watcher thread
// that monitors specified paths and enqueues change events. The actual reload work
// must be performed by the main thread when Poll() returns events.
//
// Windows implementation uses ReadDirectoryChangesW where possible, otherwise falls back
// to polling with timestamps via IScriptFileSystem::LastWriteTimeUtc().
// HotReloadManager.cpp
//
// Implementation of HotReloadManager with corrected ownership and encapsulation.

#include "HotReloadManager.h"
#include "ScriptFileSystem.h"
#include "ScriptingRuntime.h"   // for IScriptFileSystem definition
#include "Logging.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <vector>
#include <cassert>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Scripting {

    struct HotReloadManager::Impl {
        Impl() : running(false), fs(nullptr) {}
        ~Impl() { StopInternal(); }

        bool StartInternal(const HotReloadConfig& cfg, IScriptFileSystem* fsPtr) {
            std::lock_guard<std::mutex> lk(mutex);
            if (running.load()) {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "HotReloadManager::Start called while already running");
                return false;
            }
            config = cfg;

            // Ownership/assignment fix:
            if (fsPtr) {
                ownedFs.reset(); // we do not own the passed pointer
                fs = fsPtr;
            }
            else {
                ownedFs = CreateDefaultFileSystem();
                if (!ownedFs) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "HotReloadManager: CreateDefaultFileSystem failed");
                    fs = nullptr;
                    return false;
                }
                fs = ownedFs.get();
            }

            // record initial timestamps
            lastWriteMap.clear();
            for (const auto& p : config.paths) {
                uint64_t ts = fs->LastWriteTimeUtc(p);
                lastWriteMap[p] = ts;
            }

            running.store(true);
            workerThread = std::thread(&Impl::ThreadMain, this);
            return true;
        }

        void StopInternal() {
            {
                std::lock_guard<std::mutex> lk(mutex);
                if (!running.load()) return;
                running.store(false);
                cv.notify_all(); // wake any waiters on cv
            }

#ifdef _WIN32
            // If the worker is blocked in synchronous ReadDirectoryChangesW we need to
            // cancel its blocking I/O so it can wake up and exit. CancelSynchronousIo
            // requests cancellation of all blocking I/O issued by the specified thread.
            // It's safe to call here because workerThread is in the same process.
            if (workerThread.joinable()) {
                BOOL canceled = CancelSynchronousIo(workerThread.native_handle());
                if (!canceled) {
                    // Not fatal — just log; worker thread may still wake on events or timeouts.
                    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "HotReloadManager: CancelSynchronousIo failed (err=", GetLastError(),"%u).");
                }
            }
#endif

            // Join the thread, but be defensive: if join blocks unexpectedly, we try a few times.
            if (workerThread.joinable()) {
                // Preferred: wait for the worker to exit normally.
                // If join somehow blocks (very unlikely after cancellation), we do a timed retry loop.
                // NOTE: std::thread::join has no timeout; we emulate a watchdog by yielding/sleeping
                // and checking if joinable cleared (worker must clear it internally when exiting).
                // The common case will succeed on the first join().
                try {
                    workerThread.join();
                }
                catch (...) {
                    // On platforms where join can throw (rare), log and try a fallback.
                    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "HotReloadManager: exception while joining worker thread; attempting fallback.");
                    for (int i = 0; i < 100 && workerThread.joinable(); ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    if (workerThread.joinable()) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "HotReloadManager: worker thread did not exit after stop request.");
                    }
                    else {
                        // if it exited during our fallback wait, nothing left to do
                    }
                }
            }

            // clear queue
            {
                std::lock_guard<std::mutex> qlk(queueMutex);
                while (!eventQueue.empty()) eventQueue.pop();
            }
        }


        void RequestReload(const std::string& reason) {
            HotReloadEvent ev;
            ev.path = reason.empty() ? std::string("manual") : reason;
            ev.timestamp = 0;
            {
                std::lock_guard<std::mutex> lk(queueMutex);
                eventQueue.push(ev);
            }
            // wake main thread if needed (not strictly necessary)
            cv.notify_all();
        }

        // PollEvents returns all pending events, and also invokes the callback (if set)
        // on the caller's thread prior to returning the events.
        std::vector<HotReloadEvent> PollEvents() {
            std::vector<HotReloadEvent> out;
            {
                std::lock_guard<std::mutex> lk(queueMutex);
                while (!eventQueue.empty()) {
                    out.push_back(eventQueue.front());
                    eventQueue.pop();
                }
            }

            // Call callback on caller thread while holding a lock to protect callback pointer.
            ChangeCallback cbCopy;
            {
                std::lock_guard<std::mutex> lk(mutex);
                cbCopy = callback;
            }
            if (cbCopy) {
                for (const auto& ev : out) {
                    // Important: callback runs on caller thread (commonly main thread).
                    try {
                        cbCopy(ev);
                    }
                    catch (...) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "HotReloadManager callback threw an exception");
                    }
                }
            }
            return out;
        }

        void SetCallback(ChangeCallback cb) {
            std::lock_guard<std::mutex> lk(mutex);
            callback = std::move(cb);
        }

        bool IsRunning() const { return running.load(); }

    private:
        void enqueueEvent(const HotReloadEvent& ev) {
            std::lock_guard<std::mutex> lk(queueMutex);
            eventQueue.push(ev);
        }

        void ThreadMain() {
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "HotReloadManager: watcher thread started");

#ifdef _WIN32
            // Build list of directories to watch
            struct DirWatch { HANDLE hDir = INVALID_HANDLE_VALUE; std::wstring wpath; std::string pathUtf8; };
            std::vector<DirWatch> dirWatches;
            for (const auto& p : config.paths) {
                std::string full = p;
                size_t pos = full.find_last_of("\\/");
                std::string dir = (pos == std::string::npos) ? "." : full.substr(0, pos);
                std::wstring wdir = Utf8ToWide(dir);
                bool found = false;
                for (auto& dw : dirWatches) if (dw.wpath == wdir) { found = true; break; }
                if (found) continue;

                HANDLE h = CreateFileW(wdir.c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    nullptr);
                DirWatch d;
                d.hDir = (h == INVALID_HANDLE_VALUE) ? INVALID_HANDLE_VALUE : h;
                d.wpath = wdir;
                d.pathUtf8 = WideToUtf8(wdir);
                dirWatches.push_back(d);
            }

            const DWORD bufferSize = 64 * 1024;
            std::vector<BYTE> buffer(bufferSize);

            while (running.load()) {
                bool anyOk = false;
                for (auto& dw : dirWatches) {
                    if (dw.hDir == INVALID_HANDLE_VALUE) continue;
                    DWORD bytesReturned = 0;
                    BOOL ok = ReadDirectoryChangesW(
                        dw.hDir,
                        buffer.data(),
                        static_cast<DWORD>(buffer.size()),
                        FALSE,
                        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE,
                        &bytesReturned,
                        nullptr,
                        nullptr
                    );
                    if (!ok) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "ReadDirectoryChangesW failed for ", dw.pathUtf8.c_str());
                        CloseHandle(dw.hDir);
                        dw.hDir = INVALID_HANDLE_VALUE;
                        continue;
                    }
                    anyOk = true;
                    if (bytesReturned == 0) continue;

                    FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
                    while (true) {
                        std::wstring wname(fni->FileName, fni->FileNameLength / sizeof(WCHAR));
                        std::string nameUtf8 = WideToUtf8(wname);

                        // compose full path
                        std::string fullChanged = dw.pathUtf8;
                        if (!fullChanged.empty()) {
                            char last = fullChanged.back();
                            if (last != '\\' && last != '/') fullChanged.push_back('\\');
                        }
                        fullChanged += nameUtf8;

                        uint64_t nowTs = fs->LastWriteTimeUtc(fullChanged);
                        uint64_t prevTs = 0;
                        {
                            std::lock_guard<std::mutex> lk(mutex);
                            auto it = lastWriteMap.find(fullChanged);
                            prevTs = (it == lastWriteMap.end()) ? 0 : it->second;
                            lastWriteMap[fullChanged] = nowTs;
                        }
                        HotReloadEvent ev;
                        ev.path = fullChanged;
                        ev.timestamp = nowTs;
                        enqueueEvent(ev);

                        if (fni->NextEntryOffset == 0) break;
                        fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(((BYTE*)fni) + fni->NextEntryOffset);
                    }
                }

                if (!anyOk) {
                    // fallback to polling each configured path
                    {
                        std::lock_guard<std::mutex> lk(mutex);
                        for (const auto& p : config.paths) {
                            uint64_t ts = fs->LastWriteTimeUtc(p);
                            uint64_t prev = lastWriteMap[p];
                            if (ts != 0 && ts != prev) {
                                lastWriteMap[p] = ts;
                                HotReloadEvent ev;
                                ev.path = p;
                                ev.timestamp = ts;
                                enqueueEvent(ev);
                            }
                        }
                    }
                    std::unique_lock<std::mutex> ul(cvMutex);
                    cv.wait_for(ul, std::chrono::milliseconds(config.pollIntervalMs), [this]() { return !running.load(); });
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }

            for (auto& dw : dirWatches) {
                if (dw.hDir != INVALID_HANDLE_VALUE) CloseHandle(dw.hDir);
            }

#else
            while (running.load()) {
                {
                    std::lock_guard<std::mutex> lk(mutex);
                    for (const auto& p : config.paths) {
                        uint64_t ts = fs->LastWriteTimeUtc(p);
                        uint64_t prev = lastWriteMap[p];
                        if (ts != 0 && ts != prev) {
                            lastWriteMap[p] = ts;
                            HotReloadEvent ev;
                            ev.path = p;
                            ev.timestamp = ts;
                            enqueueEvent(ev);
                        }
                    }
                }
                std::unique_lock<std::mutex> ul(cvMutex);
                cv.wait_for(ul, std::chrono::milliseconds(config.pollIntervalMs), [this]() { return !running.load(); });
            }
#endif

            ENGINE_PRINT(EngineLogging::LogLevel::Info, "HotReloadManager: watcher thread exiting");
        }

#ifdef _WIN32
        static std::wstring Utf8ToWide(const std::string& s) {
            if (s.empty()) return std::wstring();
            int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
            if (needed <= 0) return std::wstring();
            std::wstring out;
            out.resize(needed);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], needed);
            return out;
        }
        static std::string WideToUtf8(const std::wstring& w) {
            if (w.empty()) return std::string();
            int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            if (needed <= 0) return std::string();
            std::string out;
            out.resize(needed);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], needed, nullptr, nullptr);
            return out;
        }
#endif

        HotReloadConfig config;
        std::unique_ptr<IScriptFileSystem> ownedFs; // if we created one
        IScriptFileSystem* fs;                      // not-owned pointer to FS in use

        std::thread workerThread;
        std::atomic<bool> running;
        std::mutex mutex; // guards config and lastWriteMap and callback
        std::condition_variable cv;
        std::mutex cvMutex;

        // event queue and its lock
        std::queue<HotReloadEvent> eventQueue;
        std::mutex queueMutex;

        ChangeCallback callback;

        std::unordered_map<std::string, uint64_t> lastWriteMap;
    };

    ////////////////////////////////////////////////////////////////////////////////
    // HotReloadManager public wrapper
    ////////////////////////////////////////////////////////////////////////////////

    HotReloadManager::HotReloadManager() : m_impl(new Impl()) {}
    HotReloadManager::~HotReloadManager() { Stop(); }

    bool HotReloadManager::Start(const HotReloadConfig& cfg, IScriptFileSystem* fs) {
        if (!m_impl) return false;
        return m_impl->StartInternal(cfg, fs);
    }

    void HotReloadManager::Stop() {
        if (!m_impl) return;
        m_impl->StopInternal();
    }

    void HotReloadManager::RequestReload(const std::string& reason) {
        if (!m_impl) return;
        m_impl->RequestReload(reason);
    }

    std::vector<HotReloadEvent> HotReloadManager::Poll() {
        if (!m_impl) return {};
        return m_impl->PollEvents();
    }

    void HotReloadManager::SetChangeCallback(ChangeCallback cb) {
        if (!m_impl) return;
        m_impl->SetCallback(std::move(cb));
    }

    bool HotReloadManager::IsRunning() const {
        if (!m_impl) return false;
        return m_impl->IsRunning();
    }

} // namespace Scripting
