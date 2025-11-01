// Implementation of error formatting & stacktrace capture.
// - Includes: push an error handler function on the stack before pcall, call debug.traceback, and format file/line entries.
// - Contains: optional mapping for precompiled code to original sources if you want source maps.
