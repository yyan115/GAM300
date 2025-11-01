// Module/require loader abstraction to resolve script module names to asset paths and handle caching.
// - Includes: API to register loaders, virtual or callback-based resolver, and functions to clear package.loaded entries on reload.
// - Contains: doc on search paths, precompiled bytecode handling, and module versioning metadata if needed.
// - Use cases: used by ScriptingRuntime when loading modules via `require` and by HotReloadManager to invalidate module caches.
