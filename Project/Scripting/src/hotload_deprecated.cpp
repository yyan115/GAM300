// hotload.cpp
#include "Scripting.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <windows.h>

static std::string g_scriptPath;
static lua_State* g_L = nullptr;
static std::atomic<bool> g_reloadRequested{ false };
static std::atomic<bool> g_watcherStop{ false };
static std::thread g_watcherThread;
static std::mutex g_stateMutex; // protects g_L during reloads on main thread

namespace Scripting {

    static int l_cpp_print(lua_State* L) {
        const char* s = luaL_optstring(L, 1, "");
        std::cout << "[lua] " << s << "\n";
        return 0;
    }

    static void register_functions(lua_State* L) {
        lua_pushcfunction(L, l_cpp_print);
        lua_setglobal(L, "cpp_print");
        // add more bindings here
    }

    // Helper: get absolute path (ANSI)
    static std::string GetAbsolutePathA(const std::string& path) {
        DWORD needed = GetFullPathNameA(path.c_str(), 0, nullptr, nullptr);
        if (needed == 0) return path;
        std::vector<char> buf(needed);
        DWORD out = GetFullPathNameA(path.c_str(), needed, buf.data(), nullptr);
        if (out == 0) return path;
        return std::string(buf.data(), out);
    }

    // Helper: get last write time as uint64 (FILETIME)
    static bool GetFileLastWriteTimeUtc(const std::string& path, uint64_t& outTime) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) return false;
        uint64_t ft = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
            static_cast<uint64_t>(fad.ftLastWriteTime.dwLowDateTime);
        outTime = ft;
        return true;
    }

    // Convert wide filename from FILE_NOTIFY_INFORMATION to UTF-8 std::string
    static std::string WideToUtf8(const std::wstring& w) {
        if (w.empty()) return std::string();
        // get required buffer size (bytes)
        int bytes = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return std::string();
        std::vector<char> buf(bytes);
        int written = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), buf.data(), bytes, nullptr, nullptr);
        if (written <= 0) return std::string();
        return std::string(buf.data(), written);
    }

    // watcher thread: watch the directory containing scriptPath using ReadDirectoryChangesW
    static void watcher_thread_func(const std::string& scriptPath) {
        // get absolute script and split directory and filename
        std::string fullScript = GetAbsolutePathA(scriptPath);
        std::string dir;
        std::string scriptFileName;
        size_t pos = fullScript.find_last_of("\\/");
        if (pos == std::string::npos) {
            dir = ".";
            scriptFileName = fullScript;
        }
        else {
            dir = fullScript.substr(0, pos);
            scriptFileName = fullScript.substr(pos + 1);
        }

        // initial last write time
        uint64_t lastWrite = 0;
        GetFileLastWriteTimeUtc(fullScript, lastWrite);

        // open directory handle
        HANDLE hDir = CreateFileA(
            dir.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );

        if (hDir == INVALID_HANDLE_VALUE) {
            // fallback to polling if CreateFileA fails
            std::cerr << "[Scripting] watcher: CreateFileA failed, falling back to polling\n";
            while (!g_watcherStop.load()) {
                uint64_t now = 0;
                if (GetFileLastWriteTimeUtc(fullScript, now) && now != lastWrite) {
                    lastWrite = now;
                    g_reloadRequested.store(true);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            return;
        }

        const DWORD bufferSize = 4096;
        std::vector<BYTE> buffer(bufferSize);
        DWORD bytesReturned = 0;

        while (!g_watcherStop.load()) {
            BOOL ok = ReadDirectoryChangesW(
                hDir,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                FALSE, // don't watch subdirs
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE,
                &bytesReturned,
                NULL,
                NULL
            );

            if (!ok) {
                // if failed, break and fallback to polling behavior
                std::cerr << "[Scripting] watcher: ReadDirectoryChangesW failed\n";
                break;
            }

            if (bytesReturned == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            // iterate events
            FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
            bool request = false;
            while (true) {
                // build filename out of wide char buffer (not null-terminated)
                std::wstring wname(fni->FileName, fni->FileNameLength / sizeof(WCHAR));
                std::string nameUtf8 = WideToUtf8(wname);

                // compare to target filename (case-insensitive)
                // On Windows filenames are case-insensitive, so use _stricmp
                if (_stricmp(nameUtf8.c_str(), scriptFileName.c_str()) == 0) {
                    // double-check timestamp to avoid duplicate events
                    uint64_t now = 0;
                    std::string fullChangedPath = dir + "\\" + nameUtf8;
                    if (GetFileLastWriteTimeUtc(fullChangedPath, now) && now != lastWrite) {
                        lastWrite = now;
                        request = true;
                    }
                    else {
                        // even if timestamp not changed, request reload conservatively
                        request = true;
                    }
                }

                if (fni->NextEntryOffset == 0) break;
                fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(((BYTE*)fni) + fni->NextEntryOffset);
            }

            if (request) g_reloadRequested.store(true);
        }

        CloseHandle(hDir);

        // As a fallback, keep doing polling until stop requested (robustness)
        while (!g_watcherStop.load()) {
            uint64_t now = 0;
            if (GetFileLastWriteTimeUtc(fullScript, now) && now != lastWrite) {
                lastWrite = now;
                g_reloadRequested.store(true);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    static bool create_lua_state(lua_State*& out) {
        lua_State* L = luaL_newstate();
        if (!L) return false;
        luaL_openlibs(L);
        register_functions(L);
        out = L;
        return true;
    }

    static bool run_script(lua_State* L, const std::string& script) {
        int err = luaL_dofile(L, script.c_str());
        if (err != LUA_OK) {
            const char* msg = lua_tostring(L, -1);
            std::cerr << "[Scripting] Error running " << script << ": " << (msg ? msg : "(no msg)") << "\n";
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    static void do_reload_inner() {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        lua_State* newL = nullptr;
        if (!create_lua_state(newL)) {
            std::cerr << "[Scripting] create_lua_state failed\n";
            return;
        }
        if (!run_script(newL, g_scriptPath)) {
            lua_close(newL);
            return;
        }

        // call optional on_reload in new script
        lua_getglobal(newL, "on_reload");
        if (lua_isfunction(newL, -1)) {
            if (lua_pcall(newL, 0, 0, 0) != LUA_OK) {
                const char* msg = lua_tostring(newL, -1);
                std::cerr << "[Scripting] on_reload error: " << (msg ? msg : "(no msg)") << "\n";
                lua_pop(newL, 1);
            }
        }
        else {
            lua_pop(newL, 1);
        }

        if (g_L) lua_close(g_L);
        g_L = newL;
        std::cout << "[Scripting] Reloaded script: " << g_scriptPath << "\n";
    }

    static void do_reload() {
        do_reload_inner();
    }

    bool Initialize(const std::string& scriptPath) {
        g_scriptPath = GetAbsolutePathA(scriptPath);
        g_L = nullptr;
        if (!create_lua_state(g_L)) {
            std::cerr << "[Scripting] Failed to create lua state\n";
            return false;
        }
        // initial run
        run_script(g_L, g_scriptPath);

        // start watcher thread
        g_watcherStop.store(false);
        g_watcherThread = std::thread([scriptPath]() { watcher_thread_func(scriptPath); });
        g_watcherThread.detach(); // uses atomic stop flag for shutdown
        return true;
    }

    void Shutdown() {
        g_watcherStop.store(true);
        // give watcher a moment to exit
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            if (g_L) {
                lua_close(g_L);
                g_L = nullptr;
            }
        }
    }

    lua_State* GetLuaState() {
        return g_L;
    }

    void Tick(float dt) {
        if (g_reloadRequested.exchange(false)) {
            do_reload();
        }

        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!g_L) return;

        lua_getglobal(g_L, "update");
        if (!lua_isfunction(g_L, -1)) {
            lua_pop(g_L, 1);
            return;
        }
        lua_pushnumber(g_L, static_cast<lua_Number>(dt));
        if (lua_pcall(g_L, 1, 0, 0) != LUA_OK) {
            const char* msg = lua_tostring(g_L, -1);
            std::cerr << "[Scripting] Error calling update: " << (msg ? msg : "(no msg)") << "\n";
            lua_pop(g_L, 1);
        }
    }

    void RequestReload() {
        g_reloadRequested.store(true);
    }

} // namespace Scripting
