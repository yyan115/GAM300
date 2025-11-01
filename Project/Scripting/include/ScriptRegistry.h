// Central registry for live script instances and lookups.
// - Includes: APIs to enumerate live scripts, find instances by entity id, and broadcast events to script instances (e.g., OnSceneLoad, OnSceneUnload).
// - Contains: notes on weak-reference vs strong-reference lifecycle management so scripts are cleaned when owners die.
// - Use cases: used by editor to show live scripts, used by profiler to iterate scripts, used by scene manager.
