#pragma once
// ScriptFileSystem.h
//
// Cross-platform file access for scripts/assets.
//
// Primary responsibilities:
//  - ReadFileToString(path, out)
//  - Exists(path)
//  - GetLastWriteTimeUtc(path) -> opaque 64-bit timestamp (0 if unavailable)
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
//  - All functions are thread-safe for reading. Implementations are expected to be safe to call
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

	// Forward-declared interface originally defined in ScriptingRuntime.h; if your build
	// organizes headers differently, include ScriptingRuntime.h before this header.
	struct IScriptFileSystem;

	// Convenience factory (platform-specific) to create a reasonable default file system.
	// On Windows this returns an implementation that uses Win32 APIs. If you prefer to
	// construct your own FS implementation, implement IScriptFileSystem and pass it into
	// ScriptingRuntime::Initialize.
	std::unique_ptr<IScriptFileSystem> CreateDefaultFileSystem();

} // namespace Scripting
