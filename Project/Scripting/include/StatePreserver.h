// Helpers and policy for persisting selected script state across reloads.
// - Includes: API to register a table/keys for preservation, serialize/deserialize hooks, and the "persist scope" concept.
// - Contains: notes on what can/cannot be persisted safely (simple values, tables of primitives, asset handles) and how to map old userdata to new userdata.
// - Use cases: used by HotReloadManager when preserving critical game state across a reload.
