// Windows logging implementation.
// - Includes: OutputDebugString and console attach helpers; options to also write to file when running headless.
// - Contains: how to initialize a console (AllocConsole) in debug builds and how to detect running under a debugger.
// - Use cases: used by ScriptingRuntime and other scripting files to emit messages.
