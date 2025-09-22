#pragma once

#include "EditorPanel.hpp"
#include "pch.h"

// Forward declaration to avoid including the full Engine logging header
namespace EngineLogging {
    enum class LogLevel;
    struct LogMessage;
    class GuiLogQueue;
}

/**
 * @brief Console panel for displaying logs, warnings, and errors.
 * 
 * This panel provides a centralized location for viewing system messages,
 * debug output, and error reports, similar to Unity's Console window.
 */
class ConsolePanel : public EditorPanel {
public:
    ConsolePanel();
    virtual ~ConsolePanel() = default;

    /**
     * @brief Render the console panel's ImGui content.
     */
    void OnImGuiRender() override;

    /**
     * @brief Add a log message to the console.
     * @param message The message to add.
     * @param level The severity level (0=Info, 1=Warning, 2=Error).
     */
    void AddLog(const std::string& message, int level = 0);

    /**
     * @brief Clear all console messages.
     */
    void Clear();

private:
    struct LogEntry {
        std::string message;
        int level = 0; // 0=Info, 1=Warning, 2=Error
        double timestamp = 0.0f;
    };

    /**
     * @brief Drain messages from the Engine logging queue and add them to the console.
     */
    void DrainEngineLogQueue();

    /**
     * @brief Convert Engine log level to console panel level.
     */
    int ConvertEngineLogLevel(EngineLogging::LogLevel level);

    std::vector<LogEntry> logEntries;
    bool showInfo = true;
    bool showWarnings = true;
    bool showErrors = true;
    bool autoScroll = true;
    static const int MAX_LOG_ENTRIES = 1000;
};