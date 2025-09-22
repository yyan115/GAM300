#pragma once

#include <string>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <chrono>

// Cross-platform API export/import macros
#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
    // Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

namespace EngineLogging {
        
    // Log levels matching spdlog levels
    enum class LogLevel {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5
    };

    // Structure for queued log messages for GUI
    struct LogMessage {
        std::string text;
        LogLevel level;
        double timestamp;
        
        LogMessage(const std::string& message, LogLevel lvl)
            : text(message), level(lvl), timestamp(std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()) {}
    };

    // Thread-safe queue for GUI log messages
    class GuiLogQueue {
    public:
        void Push(const LogMessage& message);
        bool ENGINE_API TryPop(LogMessage& message);
        void Clear();
        size_t Size() const;

    private:
        mutable std::mutex mutex;
        std::queue<LogMessage> queue;
        static constexpr size_t MAX_QUEUE_SIZE = 1000;
    };

    // Initialize the logging system
    bool Initialize();
    
    // Shutdown the logging system
    void Shutdown();
    
    // Get the GUI log queue for the editor
    ENGINE_API GuiLogQueue& GetGuiLogQueue();
    
    // Logging functions
    void ENGINE_API LogTrace(const std::string& message);
    void ENGINE_API LogDebug(const std::string& message);
    void ENGINE_API LogInfo(const std::string& message);
    void ENGINE_API LogWarn(const std::string& message);
    void ENGINE_API LogError(const std::string& message);
    void ENGINE_API LogCritical(const std::string& message);
    
}

// Convenience macros for Engine logging
#define ENGINE_LOG_TRACE(msg)    EngineLogging::LogTrace(msg)
#define ENGINE_LOG_DEBUG(msg)    EngineLogging::LogDebug(msg)
#define ENGINE_LOG_INFO(msg)     EngineLogging::LogInfo(msg)
#define ENGINE_LOG_WARN(msg)     EngineLogging::LogWarn(msg)
#define ENGINE_LOG_ERROR(msg)    EngineLogging::LogError(msg)
#define ENGINE_LOG_CRITICAL(msg) EngineLogging::LogCritical(msg)