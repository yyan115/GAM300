// ScriptFileSystem_win.cpp
//
// Windows-specific implementation of the script file system.
// Implements CreateDefaultFileSystem() which returns an IScriptFileSystem using Win32 APIs.
//
// Key features:
//  - UTF-8 <-> wide conversion helpers
//  - GetFullPathNameW usage for full absolute paths
//  - GetFileAttributesExW for last-write time
//  - CreateFileW + ReadFile for robust read-all-data semantics
//  - ListDirectory using FindFirstFileW / FindNextFileW
//
// Build: compile this file only on Windows (_WIN32 defined).

#include "ScriptFileSystem.h"
#include "ScriptingRuntime.h" // for IScriptFileSystem
#include "Logging.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sys/stat.h>
#endif

#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <algorithm>

namespace Scripting {
    namespace {

        // Helper: convert UTF-8 std::string -> wide (UTF-16) std::wstring
        static std::wstring Utf8ToWide(const std::string& utf8) {
#ifdef _WIN32
            if (utf8.empty()) return std::wstring();
            int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
            if (needed <= 0) return std::wstring();
            std::wstring out;
            out.resize(needed);
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], needed);
            return out;
#else
            // Non-windows: assume input is already UTF-8 and platform APIs accept UTF-8 strings.
            return std::wstring(utf8.begin(), utf8.end());
#endif
        }

        // Helper: convert wide -> UTF-8
        static std::string WideToUtf8(const std::wstring& w) {
#ifdef _WIN32
            if (w.empty()) return std::string();
            int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            if (needed <= 0) return std::string();
            std::string out;
            out.resize(needed);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], needed, nullptr, nullptr);
            return out;
#else
            return std::string(w.begin(), w.end());
#endif
        }

        // Normalize path to absolute. Returns empty string on failure.
        static std::string NormalizePathUtf8(const std::string& pathUtf8) {
#ifdef _WIN32
            std::wstring wpath = Utf8ToWide(pathUtf8);
            if (wpath.empty()) return std::string();

            // Get required buffer size (characters)
            DWORD required = GetFullPathNameW(wpath.c_str(), 0, nullptr, nullptr);
            if (required == 0) return std::string();
            std::vector<wchar_t> buf(required);
            DWORD written = GetFullPathNameW(wpath.c_str(), required, buf.data(), nullptr);
            if (written == 0 || written >= required) {
                return std::string();
            }
            std::wstring full(buf.data(), written);

            // For long path support, you may want to prepend "\\?\" for local absolute paths.
            // But doing so requires the path to be absolute and not contain relative segments.
            // We leave it as-is here; comment explaining how you might extend this:
            // if (full.size() >= MAX_PATH) full = std::wstring(L"\\\\?\\") + full;

            return WideToUtf8(full);
#else
            // POSIX: attempt to use realpath; fallback to input
            char resolved[4096];
            if (realpath(pathUtf8.c_str(), resolved)) {
                return std::string(resolved);
            }
            return pathUtf8;
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Windows file system implementation
        ////////////////////////////////////////////////////////////////////////////////

        class WinFileSystem : public IScriptFileSystem {
        public:
            WinFileSystem() = default;
            ~WinFileSystem() override = default;

            bool ReadAllText(const std::string& path, std::string& out) override {
#ifdef _WIN32
                std::string normalized = NormalizePathUtf8(path);
                std::wstring wpath = Utf8ToWide(normalized.empty() ? path : normalized);

                // Open file for read (binary)
                HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h == INVALID_HANDLE_VALUE) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "ReadAllText: CreateFileW failed for '%s' (err=%u)", path.c_str(), GetLastError());
                    return false;
                }

                LARGE_INTEGER size = {};
                if (!GetFileSizeEx(h, &size)) {
                    CloseHandle(h);
                    return false;
                }
                if (size.QuadPart > (1LL << 31)) {
                    // too large for our simple loader
                    CloseHandle(h);
                    return false;
                }

                std::string buffer;
                buffer.resize(static_cast<size_t>(size.QuadPart));
                DWORD read = 0;
                if (size.QuadPart > 0) {
                    if (!ReadFile(h, &buffer[0], static_cast<DWORD>(buffer.size()), &read, nullptr)) {
                        CloseHandle(h);
                        return false;
                    }
                }
                CloseHandle(h);

                // If the file is UTF-8 with BOM, strip BOM.
                if (buffer.size() >= 3 && static_cast<unsigned char>(buffer[0]) == 0xEF &&
                    static_cast<unsigned char>(buffer[1]) == 0xBB &&
                    static_cast<unsigned char>(buffer[2]) == 0xBF) {
                    out.assign(buffer.begin() + 3, buffer.end());
                }
                else {
                    out.swap(buffer);
                }
                return true;
#else
                (void)path; (void)out;
                return false;
#endif
            }

            bool Exists(const std::string& path) override {
#ifdef _WIN32
                std::string normalized = NormalizePathUtf8(path);
                std::wstring wpath = Utf8ToWide(normalized.empty() ? path : normalized);
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fad)) return false;
                return true;
#else
                (void)path;
                return false;
#endif
            }

            uint64_t LastWriteTimeUtc(const std::string& path) override {
#ifdef _WIN32
                std::string normalized = NormalizePathUtf8(path);
                std::wstring wpath = Utf8ToWide(normalized.empty() ? path : normalized);

                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fad)) return 0;
                // FILETIME to uint64 (100-nanosecond intervals since 1601)
                uint64_t high = static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime);
                uint64_t low = static_cast<uint64_t>(fad.ftLastWriteTime.dwLowDateTime);
                uint64_t filetime = (high << 32) | low;
                return filetime;
#else
                (void)path;
                return 0;
#endif
            }

            bool ListDirectory(const std::string& path, std::vector<std::string>& outEntries) {
#ifdef _WIN32
                outEntries.clear();
                std::string normalized = NormalizePathUtf8(path);
                std::wstring wpath = Utf8ToWide(normalized.empty() ? path : normalized);

                // Append \* to enumerate
                std::wstring pattern = wpath;
                if (!pattern.empty()) {
                    wchar_t last = pattern.back();
                    if (last != L'\\' && last != L'/') pattern.push_back(L'\\');
                }
                pattern.append(L"*");

                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
                if (h == INVALID_HANDLE_VALUE) {
                    return false;
                }
                do {
                    std::wstring wname(fd.cFileName);
                    if (wname == L"." || wname == L"..") continue;
                    outEntries.push_back(WideToUtf8(wname));
                } while (FindNextFileW(h, &fd));
                FindClose(h);
                return true;
#else
                (void)path; (void)outEntries;
                return false;
#endif
            }
        };

    } // namespace (anonymous)

    ////////////////////////////////////////////////////////////////////////////////
    // Factory
    ////////////////////////////////////////////////////////////////////////////////

    std::unique_ptr<IScriptFileSystem> CreateDefaultFileSystem() {
#ifdef _WIN32
        return std::unique_ptr<IScriptFileSystem>(new WinFileSystem());
#else
        // Non-Windows: you should implement a platform-specific factory
        return nullptr;
#endif
    }

} // namespace Scripting
