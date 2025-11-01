// Public-ish API to request hot reloads and query reload status.
// - Includes: HotReloadManager class declaration, configuration struct (watch polling interval, enabled), and events/callbacks for reload success/failure.
// - Contains: comments explaining synchronous vs asynchronous reload behaviors and thread-safety expectations.
// - Use cases: engine/editor will call RequestReload(), or HotReloadManager will trigger reload on file change. Also used by unit tests to simulate reloads.
