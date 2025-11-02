#pragma once
// ScriptFileSystem.h
//
// Cross-platform file access for scripts/assets.
//
// Primary responsibilities:
//  - ReadAllText(path, out)
//  - Exists(path)
//  - LastWriteTimeUtc(path) -> opaque 64-bit timestamp (0 if unavailable)
//  - ListDirectory(path, outEntries)  (non-recursive)
//
// Notes & platform behavior:
//  - Paths are UTF-8 encoded std::string. Implementations should convert to wide APIs
//    on Windows and to UTF-8 aware APIs on other platforms.
//  - On Android, code running inside an APK may need a custom FileSystem that reads from
//    the APK asset manager. The Win implementation below is for native filesystem access.
//  - On Windows, long path support is recommended: prepend "\\?\" to absolute paths when
//    dealing with paths longer than MAX_PATH. Implementations should document their long-path
//    behavior.
//  - All functions should be thread-safe for reading. Implementations are expected to be safe to call
//    from multiple threads concurrently (reads only).
//
// Use cases:
//  - Used by the runtime to load scripts, by HotReloadManager to check last-write times and
//    watch for filesystem changes, and by editor/tools to list script directories.

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace Scripting {

    // Minimal file system abstraction to make testing and platform ports easier.
    // Implementations should be thread-safe for reads.
    struct IScriptFileSystem {
        virtual ~IScriptFileSystem() = default;

        // Read text from disk. Returns true and fills 'out' on success.
        virtual bool ReadAllText(const std::string& path, std::string& out) = 0;

        // Return true if file exists (or can be opened for reading).
        virtual bool Exists(const std::string& path) = 0;

        // Last write time as an opaque integer (monotonic file timestamp), 0 if not available.
        virtual uint64_t LastWriteTimeUtc(const std::string& path) = 0;

        // Non-recursive directory listing. Returns true on success and fills outEntries.
        virtual bool ListDirectory(const std::string& path, std::vector<std::string>& outEntries) = 0;
    };

    // Convenience factory (platform-specific) to create a reasonable default file system.
    // On Windows this returns an implementation that uses Win32 APIs. If you prefer to
    // construct your own FS implementation, implement IScriptFileSystem and pass it into
    // ScriptingRuntime::Initialize.
    std::unique_ptr<IScriptFileSystem> CreateDefaultFileSystem();

} // namespace Scripting
