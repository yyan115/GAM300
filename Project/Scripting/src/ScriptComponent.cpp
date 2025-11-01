// Implementation of ScriptComponent: creating per-instance env, storing registry refs, calling lifecycle functions.
// - Includes: creation of a per-script environment table, resolving functions (Awake/Start/Update), luaL_ref management for function refs, and cleanup on destroy.
// - Contains: patterns for lazy-init (call Awake once), Start-once semantics, and safe removal when native object is destroyed.
// - Use cases: called each frame by the engine to run Update/FixedUpdate/LateUpdate for attached scripts.
