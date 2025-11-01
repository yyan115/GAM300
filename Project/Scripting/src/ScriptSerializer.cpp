// Implementation of serializer that converts Lua table -> JSON/binary and vice versa.
// - Includes: recursive conversion of allowed types, cycle detection, and registry of custom serializer functions for specific script types.
// - Contains: versioning/upgrade helpers for backward compatibility.
