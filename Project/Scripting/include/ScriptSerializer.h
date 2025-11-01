// API for serializing selected script state to engine save format.
// - Includes: contract for serializable value types, hooks for scripts to provide custom serializer, and versioning metadata.
// - Contains: explanations of what is safe to serialize (primitives, asset references) and what isn't (live closures).
// - Use cases: called by scene saver, prefab exporter, and savegame system.
