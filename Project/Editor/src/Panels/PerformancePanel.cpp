#include "Panels/PerformancePanel.hpp"
#include "imgui.h"
#include "WindowManager.hpp"
#include "TimeManager.hpp"
#include "EditorComponents.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include <algorithm>

PerformancePanel::PerformancePanel()
    : EditorPanel("Performance", false) {
}

void PerformancePanel::OnImGuiRender() {
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_UTILITY);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_UTILITY);

    if (ImGui::Begin(name.c_str(), &isOpen)) {
        auto& profiler = PerformanceProfiler::GetInstance();
        
        // Main metrics header with emphasis
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("=== Performance Monitor ===");
        ImGui::PopStyleColor();
        
        // Current metrics
        double currentFps = TimeManager::GetFps();
        double frameTime = TimeManager::GetDeltaTime() * 1000.0;
        
        // FPS with color coding
        ImVec4 fpsColor = currentFps >= 60.0 ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                         currentFps >= 30.0 ? ImVec4(1.0f, 0.65f, 0.0f, 1.0f) :
                         ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        ImGui::Text("Current FPS:");
        ImGui::SameLine();
        ImGui::TextColored(fpsColor, "%.1f", currentFps);
        
        // Frame time with color coding
        ImVec4 frameTimeColor = GetTimingColor(frameTime);
        ImGui::Text("Frame Time:");
        ImGui::SameLine();
        ImGui::TextColored(frameTimeColor, "%.3f ms", frameTime);
        
        ImGui::Separator();
        
        // Profiling controls
        bool profilingEnabled = profiler.IsProfilingEnabled();
        if (ImGui::Checkbox("Enable Profiling", &profilingEnabled)) {
            profiler.EnableProfiling(profilingEnabled);
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle frame and zone profiling");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Clear History")) {
            profiler.ClearHistory();
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset all profiling statistics and graphs");
        }
        
        // Frame history info
        ImGui::SameLine();
        const auto& history = profiler.GetFrameHistory();
        ImGui::TextDisabled("(Frames: %zu/%zu)", history.frameTimes.size(), history.maxFrames);
        
        ImGui::Separator();
        
        // Collapsible sections with default open
        if (ImGui::CollapsingHeader("Frame Time Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderFrameTimeGraph();
        }
        
        if (ImGui::CollapsingHeader("FPS Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderFpsGraph();
        }
        
        if (ImGui::CollapsingHeader("Zone Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderZoneStatistics();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
}

void PerformancePanel::RenderFrameTimeGraph() {
    auto& profiler = PerformanceProfiler::GetInstance();
    const auto& history = profiler.GetFrameHistory();
    
    if (history.frameTimes.empty()) {
        ImGui::TextDisabled("No frame data available");
        ImGui::Text("Tip: Make sure the profiler is running and collecting data");
        return;
    }
    
    // Calculate statistics
    float minFrameTime = *std::min_element(history.frameTimes.begin(), history.frameTimes.end());
    float maxFrameTime = *std::max_element(history.frameTimes.begin(), history.frameTimes.end());
    float avgFrameTime = 0.0f;
    for (float ft : history.frameTimes) {
        avgFrameTime += ft;
    }
    avgFrameTime /= history.frameTimes.size();
    
    // Display stats with color coding
    ImGui::Text("Frame Time Statistics:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Min: %.2f ms", minFrameTime);
    ImGui::SameLine();
    ImVec4 avgColor = GetTimingColor(avgFrameTime);
    ImGui::TextColored(avgColor, " | Avg: %.2f ms", avgFrameTime);
    ImGui::SameLine();
    ImVec4 maxColor = GetTimingColor(maxFrameTime);
    ImGui::TextColored(maxColor, " | Max: %.2f ms", maxFrameTime);
    
    // Target frame times for reference
    ImGui::TextDisabled("Targets: 16.67ms (60 FPS) | 33.33ms (30 FPS)");
    
    // Plot graph with better scaling
    float scaleMax = std::max(maxFrameTime * 1.1f, 20.0f); // Minimum 20ms scale
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
    ImGui::PlotLines("##FrameTime", 
        history.frameTimes.data(), 
        static_cast<int>(history.frameTimes.size()),
        static_cast<int>(history.currentIndex),
        "Frame Time (ms)",
        0.0f, 
        scaleMax,
        ImVec2(0, graphHeight * 1.5f)); // Taller graph
    ImGui::PopStyleColor(2);
    
    // Performance indicators legend
    ImGui::Spacing();
    ImGui::TextDisabled("Performance Guide:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "\u2588 Good (<1ms)");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "\u2588 Monitor (1-5ms)");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "\u2588 Critical (>5ms)");
}

void PerformancePanel::RenderFpsGraph() {
    auto& profiler = PerformanceProfiler::GetInstance();
    const auto& history = profiler.GetFrameHistory();
    
    if (history.fpsHistory.empty()) {
        ImGui::TextDisabled("No FPS data available");
        ImGui::Text("Tip: Frame profiling data will appear after a few frames");
        return;
    }
    
    // Calculate statistics
    float minFps = *std::min_element(history.fpsHistory.begin(), history.fpsHistory.end());
    float maxFps = *std::max_element(history.fpsHistory.begin(), history.fpsHistory.end());
    float avgFps = 0.0f;
    for (float fps : history.fpsHistory) {
        avgFps += fps;
    }
    avgFps /= history.fpsHistory.size();
    
    // Display stats with color coding
    ImGui::Text("FPS Statistics:");
    ImGui::SameLine();
    ImVec4 minColor = minFps < 30.0f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : 
                      minFps < 60.0f ? ImVec4(1.0f, 0.65f, 0.0f, 1.0f) : 
                      ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImGui::TextColored(minColor, "Min: %.1f", minFps);
    ImGui::SameLine();
    ImVec4 avgColor = avgFps < 30.0f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : 
                      avgFps < 60.0f ? ImVec4(1.0f, 0.65f, 0.0f, 1.0f) : 
                      ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImGui::TextColored(avgColor, " | Avg: %.1f", avgFps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), " | Max: %.1f", maxFps);
    
    // FPS targets
    ImGui::TextDisabled("Targets: 60 FPS (smooth) | 30 FPS (acceptable)");
    
    // Plot graph with better styling
    float scaleMax = std::max(maxFps * 1.1f, 120.0f); // Show up to 120 FPS minimum
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f, 1.0f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
    ImGui::PlotLines("##FPS", 
        history.fpsHistory.data(), 
        static_cast<int>(history.fpsHistory.size()),
        static_cast<int>(history.currentIndex),
        "FPS",
        0.0f, 
        scaleMax,
        ImVec2(0, graphHeight * 1.5f)); // Taller graph
    ImGui::PopStyleColor(2);
    
    // FPS guide
    ImGui::Spacing();
    ImGui::TextDisabled("FPS Guide:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "\u2588 >60 FPS");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "\u2588 30-60 FPS");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "\u2588 <30 FPS");
}

void PerformancePanel::RenderZoneStatistics() {
    auto& profiler = PerformanceProfiler::GetInstance();
    const auto& zoneStats = profiler.GetZoneStatistics();
    
    // Debug info
    ImGui::Text("Total Zones: %zu | Profiling: %s", zoneStats.size(), 
                profiler.IsProfilingEnabled() ? "ON" : "OFF");
    
    if (zoneStats.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("No profiling zones recorded yet");
        ImGui::Spacing();
        ImGui::Text("Troubleshooting:");
        ImGui::BulletText("Ensure PROFILE_FUNCTION() or PROFILE_SCOPE() macros are used in code");
        ImGui::BulletText("Check that BeginFrame()/EndFrame() are called in main loop");
        ImGui::BulletText("Verify profiling is enabled (checkbox above)");
        ImGui::BulletText("Run the application for a few frames to collect data");
        return;
    }
    
    ImGui::Separator();
    ImGui::Text("Click column headers to sort");
    
    // Enhanced table with better styling
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | 
                           ImGuiTableFlags_RowBg | 
                           ImGuiTableFlags_Resizable | 
                           ImGuiTableFlags_ScrollY |
                           ImGuiTableFlags_Sortable |
                           ImGuiTableFlags_SizingStretchProp;
    
    if (ImGui::BeginTable("ZoneStats", 5, flags, ImVec2(0, 300))) {
        // Setup columns with better widths
        ImGui::TableSetupColumn("Zone Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Min (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Samples", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();
        
        // Sort zones by average time (descending)
        std::vector<std::pair<std::string, ZoneTimingData>> sortedZones(zoneStats.begin(), zoneStats.end());
        std::sort(sortedZones.begin(), sortedZones.end(), 
            [](const auto& a, const auto& b) {
                return a.second.avgTime > b.second.avgTime;
            });
        
        // Display each zone with enhanced styling
        int rowNum = 0;
        for (const auto& [zoneName, data] : sortedZones) {
            ImGui::TableNextRow();
            
            // Row background color based on performance
            ImVec4 rowColor = data.avgTime > 5.0 ? ImVec4(0.3f, 0.1f, 0.1f, 0.5f) :
                             data.avgTime > 1.0 ? ImVec4(0.3f, 0.2f, 0.1f, 0.3f) :
                             ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            if (rowColor.w > 0.0f) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowColor));
            }
            
            // Zone name with icon based on performance
            ImGui::TableNextColumn();
            const char* icon = data.avgTime > 5.0 ? "\u26A0" :  // Warning
                              data.avgTime > 1.0 ? "\u25CF" :  // Circle
                              "\u2713";  // Check
            ImVec4 iconColor = GetTimingColor(data.avgTime);
            ImGui::TextColored(iconColor, "%s", icon);
            ImGui::SameLine();
            ImGui::Text("%s", zoneName.c_str());
            
            // Average time with bold formatting for high values
            ImGui::TableNextColumn();
            ImVec4 avgColor = GetTimingColor(data.avgTime);
            if (data.avgTime > 5.0) {
                ImGui::PushStyleColor(ImGuiCol_Text, avgColor);
                ImGui::Text("%.3f", data.avgTime);
                ImGui::PopStyleColor();
            } else {
                ImGui::TextColored(avgColor, "%.3f", data.avgTime);
            }
            
            // Min time
            ImGui::TableNextColumn();
            ImVec4 minColor = GetTimingColor(data.minTime);
            ImGui::TextColored(minColor, "%.3f", data.minTime);
            
            // Max time with highlight if significantly different from avg
            ImGui::TableNextColumn();
            ImVec4 maxColor = GetTimingColor(data.maxTime);
            if (data.maxTime > data.avgTime * 2.0) {
                // Highlight spikes
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("%.3f (!)", data.maxTime);
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Warning: Max is %.1fx average (potential spike)", data.maxTime / data.avgTime);
                }
            } else {
                ImGui::TextColored(maxColor, "%.3f", data.maxTime);
            }
            
            // Sample count
            ImGui::TableNextColumn();
            ImGui::Text("%u", data.sampleCount);
            
            rowNum++;
        }
        
        ImGui::EndTable();
    }
    
    // Summary statistics
    ImGui::Spacing();
    ImGui::Separator();
    double totalAvgTime = 0.0;
    for (const auto& [name, data] : zoneStats) {
        totalAvgTime += data.avgTime;
    }
    ImGui::Text("Total measured time per frame: %.3f ms", totalAvgTime);
    ImGui::SameLine();
    ImGui::TextDisabled("(sum of all zone averages)");
}

ImVec4 PerformancePanel::GetTimingColor(double timeMs) const {
    // Color coding: green < 1ms, orange < 5ms, red >= 5ms
    if (timeMs < 1.0) {
        return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
    } else if (timeMs < 5.0) {
        return ImVec4(1.0f, 0.65f, 0.0f, 1.0f); // Orange
    } else {
        return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    }
}