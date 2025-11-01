// Coroutine scheduling implementation.
// - Includes: data structures (vector of active coroutines), resume semantics, yield token parsing, and time-based scheduling.
// - Contains: per-frame resume code, integration with Tick(dt), and robust error handling for coroutine errors.
// - Use cases: invoked each frame from Scripting::Tick to advance waiting coroutines.
