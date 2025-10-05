#include "Panels/PerformancePanel.hpp"
#include "imgui.h"
#include "WindowManager.hpp"
#include "TimeManager.hpp"
#include "EditorComponents.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Engine.h"
#include <algorithm>
#include <unordered_set>
#include <IconsFontAwesome6.h>

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
            profiler.ClearHistory();
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
    ImGui::TextDisabled("Scale: 0-60ms (Y-axis: 0, 20, 40, 60)");
    
    // Create graph with left margin for Y-axis labels
    ImGui::Dummy(ImVec2(40.0f, 0.0f)); // Left margin for Y-axis labels
    ImGui::SameLine();
    ImVec2 graphPos = ImGui::GetCursorScreenPos();
    float graphWidth = ImGui::GetContentRegionAvail().x - 40.0f; // Account for left margin
    float graphHeightPx = graphHeight * 1.5f; // Same height as zone graphs
    
    // Plot graph with histogram for better readability
    float scaleMax = 60.0f; // Fixed scale - better range for typical frame times
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
    ImGui::PlotHistogram("##FrameTime", 
        history.frameTimes.data(), 
        static_cast<int>(history.frameTimes.size()),
        static_cast<int>(history.currentIndex),
        "Frame Time (ms)",
        0.0f, 
        scaleMax,
        ImVec2(graphWidth, graphHeightPx));
    ImGui::PopStyleColor(2);
    
    // Add Y-axis scale indicators and gridlines (positioned to the left of graph)
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 textColor = IM_COL32(255, 255, 255, 180); // White with transparency
    ImU32 gridColor = IM_COL32(100, 100, 100, 60); // Light gray gridlines
    
    // Y-axis labels and gridlines at key positions (left side of graph)
    float y0 = graphPos.y + graphHeightPx - (0.0f / scaleMax) * graphHeightPx;
    float y20 = graphPos.y + graphHeightPx - (20.0f / scaleMax) * graphHeightPx;
    float y40 = graphPos.y + graphHeightPx - (40.0f / scaleMax) * graphHeightPx;
    float y60 = graphPos.y + graphHeightPx - (60.0f / scaleMax) * graphHeightPx;
    
    // Draw gridlines across the graph
    drawList->AddLine(ImVec2(graphPos.x, y20), ImVec2(graphPos.x + graphWidth, y20), gridColor, 1.0f);
    drawList->AddLine(ImVec2(graphPos.x, y40), ImVec2(graphPos.x + graphWidth, y40), gridColor, 1.0f);
    drawList->AddLine(ImVec2(graphPos.x, y60), ImVec2(graphPos.x + graphWidth, y60), gridColor, 1.0f);
    
    // Draw Y-axis labels (positioned to the left, no background rectangles)
    drawList->AddText(ImVec2(graphPos.x - 35, y0 - 9), textColor, "0");
    drawList->AddText(ImVec2(graphPos.x - 40, y20 - 9), textColor, "20");
    drawList->AddText(ImVec2(graphPos.x - 40, y40 - 9), textColor, "40");
    drawList->AddText(ImVec2(graphPos.x - 40, y60 - 9), textColor, "60");
    
    // Performance indicators legend
    ImGui::Spacing();
    ImGui::TextDisabled("Performance Guide:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Good (<16ms)");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), " | Monitor (<35ms)");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), " | Critical (>35ms)");
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
    ImGui::TextDisabled("Scale: 0-700 FPS (Y-axis: 0, 200, 400, 600)");
    
    // Create graph with left margin for Y-axis labels
    ImGui::Dummy(ImVec2(40.0f, 0.0f)); // Left margin for Y-axis labels
    ImGui::SameLine();
    ImVec2 graphPos = ImGui::GetCursorScreenPos();
    float graphWidth = ImGui::GetContentRegionAvail().x - 40.0f; // Account for left margin
    float graphHeightPx = graphHeight * 1.5f; // Same height as zone graphs
    
    // Plot graph with histogram for better readability
    float scaleMax = 700.0f; // Fixed scale - accommodates high FPS
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 1.0f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
    ImGui::PlotHistogram("##FPS", 
        history.fpsHistory.data(), 
        static_cast<int>(history.fpsHistory.size()),
        static_cast<int>(history.currentIndex),
        "FPS",
        0.0f, 
        scaleMax,
        ImVec2(graphWidth, graphHeightPx));
    ImGui::PopStyleColor(2);
    
    // Add Y-axis scale indicators and gridlines (positioned to the left of graph)
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 textColor = IM_COL32(255, 255, 255, 180); // White with transparency
    ImU32 gridColor = IM_COL32(100, 100, 100, 60); // Light gray gridlines
    
    // Y-axis labels and gridlines at key positions (left side of graph)
    float y0 = graphPos.y + graphHeightPx - (0.0f / scaleMax) * graphHeightPx;
    float y200 = graphPos.y + graphHeightPx - (200.0f / scaleMax) * graphHeightPx;
    float y400 = graphPos.y + graphHeightPx - (400.0f / scaleMax) * graphHeightPx;
    float y600 = graphPos.y + graphHeightPx - (600.0f / scaleMax) * graphHeightPx;
    
    // Draw gridlines across the graph
    drawList->AddLine(ImVec2(graphPos.x, y200), ImVec2(graphPos.x + graphWidth, y200), gridColor, 1.0f);
    drawList->AddLine(ImVec2(graphPos.x, y400), ImVec2(graphPos.x + graphWidth, y400), gridColor, 1.0f);
    drawList->AddLine(ImVec2(graphPos.x, y600), ImVec2(graphPos.x + graphWidth, y600), gridColor, 1.0f);
    
    // Draw Y-axis labels (positioned to the left, no background rectangles)
    drawList->AddText(ImVec2(graphPos.x - 35, y0 - 9), textColor, "0");
    drawList->AddText(ImVec2(graphPos.x - 43, y200 - 9), textColor, "200");
    drawList->AddText(ImVec2(graphPos.x - 43, y400 - 9), textColor, "400");
    drawList->AddText(ImVec2(graphPos.x - 43, y600 - 9), textColor, "600");
    
    // FPS guide
    ImGui::Spacing();
    ImGui::TextDisabled("FPS Guide:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ">60 FPS");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), " | 30-60 FPS");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), " | <30 FPS");
}

void PerformancePanel::RenderZoneGraph(const std::string& zoneName) {
    auto& profiler = PerformanceProfiler::GetInstance();
    const auto& zoneStats = profiler.GetZoneStatistics();
    
    auto it = zoneStats.find(zoneName);
    if (it == zoneStats.end()) {
        ImGui::TextDisabled("Zone not found: %s", zoneName.c_str());
        return;
    }
    
    const auto& data = it->second;
    
    if (data.history.empty()) {
        ImGui::TextDisabled("No history data available for this zone yet");
        return;
    }
    
    // Header for selected zone
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
    ImGui::Text("Zone Performance: %s", zoneName.c_str());
    ImGui::PopStyleColor();
    
    // Statistics
    ImGui::Text("Avg: %.3f ms | Min: %.3f ms | Max: %.3f ms | Samples: %u", 
                data.avgTime, data.minTime, data.maxTime, data.sampleCount);
    
    // Calculate scale for graph
    float scaleMax = std::max<float>(static_cast<float>(data.maxTime * 1.1f), 1.0f); // Minimum 1ms scale
    
    // Plot the zone's history
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.8f, 0.5f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
    ImGui::PlotLines("##ZoneHistory", 
        data.history.data(), 
        static_cast<int>(data.history.size()),
        static_cast<int>(data.historyIndex),
        zoneName.c_str(),
        0.0f, 
        scaleMax,
        ImVec2(0, graphHeight * 1.5f));
    ImGui::PopStyleColor(2);
    
    // Show average line reference
    ImGui::TextDisabled("Reference: Average is at %.3f ms", data.avgTime);
}

void PerformancePanel::RenderZoneStatistics() {
    auto& profiler = PerformanceProfiler::GetInstance();
    const auto& zoneStats = profiler.GetZoneStatistics();
    
    // Get ECS system names for auto-highlighting
    std::unordered_set<std::string> ecsSystemNames;
	auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    if (auto* systemManager = ecsManager.GetSystemManager()) {
        for (const auto& [typeName, system] : systemManager->GetAllSystems()) {
            if (system) {
                // Get clean system name from the system itself
                std::string systemName = system->GetSystemName();
                ecsSystemNames.insert(systemName);
            }
        }
    }
    
    // Debug info
    ImGui::Text("Total Zones: %zu | ECS Systems: %zu | Profiling: %s", 
                zoneStats.size(), ecsSystemNames.size(),
                profiler.IsProfilingEnabled() ? "ON" : "OFF");
    
    if (zoneStats.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("No profiling zones recorded yet");
        ImGui::Spacing();
        ImGui::Text("Troubleshooting:");
        ImGui::BulletText("Add PROFILE_FUNCTION() at the start of functions to track");
        ImGui::BulletText("Add PROFILE_SCOPE(\"name\") for specific code sections");
        ImGui::BulletText("BeginFrame()/EndFrame() calls are now automatic");
        ImGui::BulletText("Run the application for a few frames to collect data");
        return;
    }
    
    ImGui::Separator();
    ImGui::Text("Click a zone name to see its graph | Click column headers to sort");
    
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
        for (const auto& [zoneName, data] : sortedZones) {
            ImGui::TableNextRow();
            
            // Check if this is an ECS system (highlight differently)
            bool isEcsSystem = false;
            for (const auto& sysName : ecsSystemNames) {
                if (zoneName.find(sysName) != std::string::npos) {
                    isEcsSystem = true;
                    break;
                }
            }
            
            // Row background color based on performance and type
            ImVec4 rowColor;
            if (isEcsSystem) {
                // ECS systems get blue tint
                rowColor = data.avgTime > 5.0 ? ImVec4(0.3f, 0.1f, 0.2f, 0.5f) :
                          data.avgTime > 1.0 ? ImVec4(0.2f, 0.2f, 0.3f, 0.3f) :
                          ImVec4(0.1f, 0.1f, 0.2f, 0.2f);
            } else {
                rowColor = data.avgTime > 5.0 ? ImVec4(0.3f, 0.1f, 0.1f, 0.5f) :
                          data.avgTime > 1.0 ? ImVec4(0.3f, 0.2f, 0.1f, 0.3f) :
                          ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            }
            
            if (rowColor.w > 0.0f) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(rowColor));
            }
            
            // Zone name with icon based on type and performance
            ImGui::TableNextColumn();
            
            const char* icon;
            if (isEcsSystem) {
                icon = ICON_FA_GEAR;  // Gear for ECS systems
            } else {
                icon = data.avgTime > 5.0 ? ICON_FA_TRIANGLE_EXCLAMATION :  // Warning
                      data.avgTime > 1.0 ? ICON_FA_CIRCLE :  // Circle
                      ICON_FA_CHECK;  // Check
            }
            
            ImVec4 iconColor = isEcsSystem ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : GetTimingColor(data.avgTime);
            ImGui::TextColored(iconColor, "%s", icon);
            ImGui::SameLine();
            
            // Make zone name clickable to show graph
            bool isSelected = (selectedZone == zoneName);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            }
            
            if (ImGui::Selectable(zoneName.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                selectedZone = (selectedZone == zoneName) ? "" : zoneName;
            }
            
            if (isSelected) {
                ImGui::PopStyleColor();
            }
            
            if (ImGui::IsItemHovered()) {
                if (isEcsSystem) {
                    ImGui::SetTooltip("ECS System - Click to see performance graph");
                } else {
                    ImGui::SetTooltip("Click to see performance graph");
                }
            }
            
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
        }
        
        ImGui::EndTable();
    }
    
    // Show graph for selected zone
    if (!selectedZone.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        RenderZoneGraph(selectedZone);
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
    
    // ECS systems summary
    if (!ecsSystemNames.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "| %s = ECS System", ICON_FA_GEAR);
    }
}

ImVec4 PerformancePanel::GetTimingColor(double timeMs) const {
    // Color coding: green < 16ms, orange < 35ms, red >= 35ms
    if (timeMs < 16.0) {
        return ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
    } else if (timeMs < 35.0) {
        return ImVec4(1.0f, 0.65f, 0.0f, 1.0f); // Orange
    } else {
        return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    }
}