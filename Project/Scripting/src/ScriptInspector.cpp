// Implementation of the editor reflection glue.
// - Includes: code that reads a script’s environment table or special __editor metadata and exposes fields to the editor UI.
// - Contains: caching strategies to avoid repeated Lua inspection, and how to push updated values back into running script instances.
