// Watches scripts (platform-specific watchers) and coordinates safe reloads.
// - Includes: implementation of a watcher thread or platform-specific file watchers, debouncing, timestamp checks, and the logic that orchestrates a reload on the main thread.
// - Contains: code to call into ScriptingRuntime to create new state, optionally call state-preservation helpers, and swap states atomically with mutexes.
// - Use cases: invoked when scripts change on disk (developer workflow), or when editor triggers a reload.
