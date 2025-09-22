#include "pch.h"
#include "Logging.hpp"

#include "spdlog/spdlog.h" 
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/pattern_formatter.h"

#include <filesystem>

namespace EngineLogging {
       
    // Custom GUI sink that pushes to thread-safe queue
    class GuiSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        explicit GuiSink(GuiLogQueue& queue) : guiQueue(queue) {}

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            // Convert spdlog level to our LogLevel enum
            LogLevel level;
            switch (msg.level) {
                case spdlog::level::trace:    level = LogLevel::Trace; break;
                case spdlog::level::debug:    level = LogLevel::Debug; break;
                case spdlog::level::info:     level = LogLevel::Info; break;
                case spdlog::level::warn:     level = LogLevel::Warn; break;
                case spdlog::level::err:      level = LogLevel::Error; break;
                case spdlog::level::critical: level = LogLevel::Critical; break;
                default:                      level = LogLevel::Info; break;
            }
            
            // Format the message
            std::string message = fmt::to_string(msg.payload);
            assert(!message.empty() && "Log message should not be empty");
            
            // Push to GUI queue
            guiQueue.Push(LogMessage(message, level));
        }

        void flush_() override {
            // Nothing to flush for GUI sink
        }

    private:
        GuiLogQueue& guiQueue;
    };

    // Static instances
    static std::shared_ptr<spdlog::logger> logger;

    static GuiLogQueue guiLogQueue;
    static bool initialized = false;

    // GuiLogQueue implementation
    void GuiLogQueue::Push(const LogMessage& message) {
        assert(!message.text.empty() && "Log message text cannot be empty");
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Remove old messages if queue is full
        while (queue.size() >= MAX_QUEUE_SIZE) {
            assert(!queue.empty() && "Queue should not be empty when size >= MAX_QUEUE_SIZE");
            queue.pop();
        }
        
        queue.push(message);
        assert(queue.size() <= MAX_QUEUE_SIZE && "Queue size should not exceed maximum");
    }

    bool GuiLogQueue::TryPop(LogMessage& message) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        
        message = queue.front();
        assert(!message.text.empty() && "Popped message should not be empty");
        queue.pop();
        return true;
    }

    void GuiLogQueue::Clear() {
        std::lock_guard<std::mutex> lock(mutex);
        std::queue<LogMessage> empty;
        queue.swap(empty);
        assert(queue.empty() && "Queue should be empty after clear");
    }

    size_t GuiLogQueue::Size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }

    // Logging system functions
    bool Initialize() {
        if (initialized) {
            return true;
        }

        try {
            // Create logs directory if it doesn't exist
            std::filesystem::create_directories("logs");
            assert(std::filesystem::exists("logs") && "Logs directory should exist after creation");

            // Create sinks
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            assert(console_sink != nullptr && "Console sink creation failed");
            console_sink->set_level(spdlog::level::trace);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/engine.log", true);
            assert(file_sink != nullptr && "File sink creation failed");
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            auto gui_sink = std::make_shared<GuiSink>(guiLogQueue);
            assert(gui_sink != nullptr && "GUI sink creation failed");
            gui_sink->set_level(spdlog::level::trace);
            gui_sink->set_pattern("%v");

            // Create logger with multiple sinks
            std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink, gui_sink };
            assert(!sinks.empty() && "Sinks vector should not be empty");
            
            logger = std::make_shared<spdlog::logger>("engine", sinks.begin(), sinks.end());
            assert(logger != nullptr && "Logger creation failed");
            
            logger->set_level(spdlog::level::trace);
            logger->flush_on(spdlog::level::warn);

            // Register as default logger
            spdlog::set_default_logger(logger);

            initialized = true;
            
            // Log initialization message
            LogInfo("Engine logging system initialized");
            
            return true;
        }
        catch (const std::exception& ex) {
            std::cerr << "Failed to initialize logging system: " << ex.what() << std::endl;
            initialized = true; // Set to true anyway to avoid repeated attempts
            return false;
        }
    }

    void Shutdown() {
        if (!initialized) {
            return;
        }

        LogInfo("Shutting down logging system");
        
        if (logger) {
            logger->flush();
            logger.reset();
        }
        
        spdlog::shutdown();
        guiLogQueue.Clear();
        initialized = false;
    }

    GuiLogQueue& GetGuiLogQueue() {
        assert(initialized && "Logging system must be initialized before accessing GUI queue");
        return guiLogQueue;
    }

    // Internal helper for logging
    void LogInternal(LogLevel level, const std::string& message) {
        assert(!message.empty() && "Log message cannot be empty");
        
        if (!initialized) {
            return;
        }

        assert(logger != nullptr && "Logger should be valid when initialized");
        
        if (logger) {
            switch (level) {
                case LogLevel::Trace:    logger->trace(message); break;
                case LogLevel::Debug:    logger->debug(message); break;
                case LogLevel::Info:     logger->info(message); break;
                case LogLevel::Warn:     logger->warn(message); break;
                case LogLevel::Error:    logger->error(message); break;
                case LogLevel::Critical: logger->critical(message); break;
            }
        }
    }

    // Public logging functions
    void LogTrace(const std::string& message) {
        LogInternal(LogLevel::Trace, message);
    }

    void LogDebug(const std::string& message) {
        LogInternal(LogLevel::Debug, message);
    }

    void LogInfo(const std::string& message) {
        LogInternal(LogLevel::Info, message);
    }

    void LogWarn(const std::string& message) {
        LogInternal(LogLevel::Warn, message);
    }

    void LogError(const std::string& message) {
        LogInternal(LogLevel::Error, message);
    }

    void LogCritical(const std::string& message) {
        LogInternal(LogLevel::Critical, message);
    }

}