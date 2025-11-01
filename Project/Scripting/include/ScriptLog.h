#pragma once
// ScriptLog.h
//
// Platform-abstracted logging interface for the scripting subsystem.
// Use ScriptLog::Logf or convenience macros (SLOGI/SLOGW/SLOGE).
//
// Threading notes:
//  - Logging functions are safe to call concurrently.
//  - SetBackend() should be called during initialization (before heavy multi-threaded logging) or with external synchronization.
//
// Platform helpers:
//  - EnsureWindowsBackend(bool attachConsole): helper that sets a Windows backend (OutputDebugString + stderr).
//  - EnsureAndroidBackend(const char* tag): helper that sets an Android backend (logcat) when compiled for Android.
//
// The API intentionally separates the "backend" (platform specific) from the
// logging entrypoints so backends can be replaced (for tests, for editor integration, etc).

#include <cstdarg>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Scripting {
    namespace Log {

        enum class Level : int {
            Info = 0,
            Warn = 1,
            Error = 2
        };

        // Abstract backend interface. Implement per-platform and call SetBackend() at startup.
        // Backend must be safe for concurrent Log() calls.
        struct Backend {
            virtual ~Backend() = default;
            virtual void Log(Level lvl, const char* msg) = 0;
        };

        // Set or replace the logging backend. Passing nullptr will revert to default stderr behavior.
        void SetBackend(std::shared_ptr<Backend> backend);

        // printf-style formatting entrypoint (thread-safe).
        void Logf(Level lvl, const char* fmt, ...);

        // Convenience macros
#define SLOGI(fmt, ...) ::Scripting::Log::Logf(::Scripting::Log::Level::Info, (fmt), ##__VA_ARGS__)
#define SLOGW(fmt, ...) ::Scripting::Log::Logf(::Scripting::Log::Level::Warn, (fmt), ##__VA_ARGS__)
#define SLOGE(fmt, ...) ::Scripting::Log::Logf(::Scripting::Log::Level::Error, (fmt), ##__VA_ARGS__)

// Platform helper functions: call these during platform initialization if desired.
// These helpers create and install a sensible backend for the platform.
        void EnsureWindowsBackend(bool attachConsole = false);         // Windows: OutputDebugString + stderr
        void EnsureAndroidBackend(const char* tag = "scripting");     // Android: __android_log_print (logcat)

    } // namespace Log
} // namespace Scripting
