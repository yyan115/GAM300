// API for starting/stopping/resuming Lua coroutines from C++.
// - Includes: StartCoroutine, StopCoroutine, WaitForSeconds/WaitForFrames types described, and hooks for yield semantics.
// - Contains: doc about lifetime of coroutine objects and how scheduler ticks them each frame.
// - Use cases: exposed to Lua as StartCoroutine, used by scripts to run co-operative async tasks.
