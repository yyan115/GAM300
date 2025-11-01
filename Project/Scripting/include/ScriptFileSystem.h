// Cross-platform file access for scripts/assets.
// - Includes: ReadFileToString, Exists, GetLastWriteTime, and ListDirectory functions. Platform-specific behaviors documented (APK vs filesystem).
// - Contains: note on path normalization, UTF-8 handling, and long-path support on Windows.
// - Use cases: used by ScriptingRuntime and HotReloadManager to load scripts and query timestamps.
