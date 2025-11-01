// Windows-specific implementation of ScriptFileSystem.
// - Includes: wrappers for GetFullPathName, GetFileAttributesEx, and safe read functions with binary/text modes.
// - Contains: UTF-8 <-> wide conversion helpers and long-path handling comment blocks.
// - Use cases: compiled on Windows build only; used by ScriptingRuntime.
