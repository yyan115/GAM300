#pragma once
#include "EditorPanel.hpp"
#include "imgui.h"
#include <vector>
#include <string>

/**
 * @brief Panel that displays performance metrics like FPS, frame time graphs, and zone statistics
 */
class PerformancePanel : public EditorPanel {
public:
    PerformancePanel();
    virtual ~PerformancePanel() = default;

protected:
    void OnImGuiRender() override;

private:
    void RenderFrameTimeGraph();
    void RenderFpsGraph();
    void RenderZoneStatistics();
    void RenderZoneGraph(const std::string& zoneName);
    
    // Helper to get color based on timing
    ImVec4 GetTimingColor(double timeMs) const;
    
    // UI state (pascalCase for variables)
    bool showFrameTimeGraph = true;
    bool showFpsGraph = true;
    bool showZoneStats = true;
    float graphHeight = 80.0f;
    
    // Selected zone for detailed graph view
    std::string selectedZone;
};