// Implementation of state extraction & re-injection during hot reload.
// - Includes: functions to walk Lua tables, convert basic types to a serializable C++ representation, and reapply them to the new state.
// - Contains: mapping systems for userdata identity reconciliation, and best-effort warnings for incompatible changes.
// - Use cases: called internally by HotReloadManager during reload with option to preserve state.
