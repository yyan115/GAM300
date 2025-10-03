#include "Panels/ConsolePanel.hpp"
#include "imgui.h"
#include <algorithm>
#include "pch.h"

// Include Engine logging to access the GUI queue
#include "Logging.hpp"

ConsolePanel::ConsolePanel() 
    : EditorPanel("Console", true) {
    ENGINE_LOG_INFO("Console initializing...");
}

void ConsolePanel::OnImGuiRender() {
    // Drain Engine log queue FIRST, before any ImGui rendering, to ensure new messages appear immediately
    DrainEngineLogQueue();
    
    if (ImGui::Begin(name.c_str(), &isOpen)) {
        // Filter checkboxes
        ImGui::Checkbox("Info", &showInfo); ImGui::SameLine();
        ImGui::Checkbox("Warnings", &showWarnings); ImGui::SameLine();
        ImGui::Checkbox("Errors", &showErrors); ImGui::SameLine();
        ImGui::Checkbox("Auto Scroll", &autoScroll);

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            Clear();
        }

        ImGui::Separator();

        // Console output area
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

        // Display log entries
        for (const auto& entry : logEntries) {
            bool show = false;
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white

            switch (entry.level) {
                case 0: // Info/Debug/Trace
                    show = showInfo;
                    color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Light gray
                    break;
                case 1: // Warning
                    show = showWarnings;
                    color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
                    break;
                case 2: // Error/Critical
                    show = showErrors;
                    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Red
                    break;
            }

            if (show) {
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                
                // Format timestamp to readable time
                std::time_t timeT = static_cast<std::time_t>(entry.timestamp);
                std::tm timeInfo;
                char timeBuffer[100];
                
                // Cross-platform localtime handling
#ifdef _WIN32
                localtime_s(&timeInfo, &timeT);
#else
                localtime_r(&timeT, &timeInfo);
#endif

                std::strftime(timeBuffer, sizeof(timeBuffer), "%m/%d/%Y %I:%M:%S %p", &timeInfo);
                
                //Trace = 0,
                //    Debug = 1,
                //    Info = 2,
                //    Warn = 3,
                //    Error = 4,
                //    Critical = 5


                // Add timestamp and level prefix
                const char* levelStr = (entry.level == 0) ? "[INFO]" : 
                                      (entry.level == 1) ? "[WARN]" : "[ERROR]";
                ImGui::Text("[%s] %s %s", timeBuffer, levelStr, entry.message.c_str());
                
                ImGui::PopStyleColor();
            }
        }

        // Auto-scroll to bottom
        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        // Command input area
        ImGui::Separator();
        static char inputBuffer[256] = "";
        ImGui::PushItemWidth(-80);
        if (ImGui::InputText("##ConsoleInput", inputBuffer, sizeof(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (strlen(inputBuffer) > 0) {
                AddLog(std::string("Command: ") + inputBuffer, 0);
                // TODO: Process command
                memset(inputBuffer, 0, sizeof(inputBuffer));
            }
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            if (strlen(inputBuffer) > 0) {
                AddLog(std::string("Command: ") + inputBuffer, 0);
                // TODO: Process command
                memset(inputBuffer, 0, sizeof(inputBuffer));
            }
        }
    }
    ImGui::End();
}

void ConsolePanel::DrainEngineLogQueue() {
    // Get the GUI log queue from the Engine
    EngineLogging::GuiLogQueue& logQueue = EngineLogging::GetGuiLogQueue();

    EngineLogging::LogMessage message("", EngineLogging::LogLevel::Info);

    // Drain all pending messages from the queue
    while (logQueue.TryPop(message)) {
        // Create log entry
        LogEntry entry;
        entry.message = message.text;
        entry.level = ConvertEngineLogLevel(message.level);
        entry.timestamp = message.timestamp;

        logEntries.push_back(entry);

        // Keep only the most recent entries
        if (logEntries.size() > static_cast<size_t>(MAX_LOG_ENTRIES)) {
            logEntries.erase(logEntries.begin());
        }
    }
}

int ConsolePanel::ConvertEngineLogLevel(EngineLogging::LogLevel level) {
    switch (level) {
        case EngineLogging::LogLevel::Trace:
        case EngineLogging::LogLevel::Debug:
        case EngineLogging::LogLevel::Info:
            return 0; // Info
        case EngineLogging::LogLevel::Warn:
            return 1; // Warning
        case EngineLogging::LogLevel::Error:
        case EngineLogging::LogLevel::Critical:
            return 2; // Error
        default:
            return 0; // Default to Info
    }
}

void ConsolePanel::AddLog(const std::string& message, int level) {
    LogEntry entry;
    entry.message = message;
    entry.level = level;
    entry.timestamp = ImGui::GetTime();

    logEntries.push_back(entry);

    // Keep only the most recent entries
    if (logEntries.size() > static_cast<size_t>(MAX_LOG_ENTRIES)) {
        logEntries.erase(logEntries.begin());
    }
}

void ConsolePanel::Clear() {
    logEntries.clear();
}