// Platform-abstracted logging interface for scripting subsystem.
// - Includes: Log(level, format, ...), convenience macros, and explanation of which logging sink to use per platform (stdout, OutputDebugString, Android logcat).
// - Contains: buffering/flush semantics documentation and integration with engine logger if available.
// - Use cases: used everywhere in scripting code instead of std::cout/cerr directly.
