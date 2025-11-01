// Lightweight value types & handles used across the scripting system.
// - Should include: typedefs/structs for ScriptHandle/ScriptInstanceID, ScriptLoadOptions, ReloadPolicy enum, and simple PODs used by API.
// - Contains: small helpers for comparing handles, invalid handle sentinel, and comments explaining ownership semantics.
// - Use cases: used by engine serialization, editor, and script manager to reference script instances without exposing lua_State.
