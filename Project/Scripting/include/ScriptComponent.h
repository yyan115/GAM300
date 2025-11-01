// ScriptComponent public interface used by engine entities/components.
// - Includes: API for attaching a script file to an entity, life-cycle binding (Awake/Start/Update/OnDisable/etc.), and accessors to script fields from native code.
// - Contains: comments describing how ScriptComponent caches function refs and the expected threading model (main thread only).
// - Use cases: used by the entity/component system, scene serialization, and editor inspector.
