#include "Panels/PerformancePanel.hpp"
#include "imgui.h"
#include "WindowManager.hpp"
#include "TimeManager.hpp"
#include "EditorComponents.hpp"

PerformancePanel::PerformancePanel()
    : EditorPanel("Performance", false) {
}

void PerformancePanel::OnImGuiRender() {
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_UTILITY);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_UTILITY);

    if (ImGui::Begin(name.c_str(), &isOpen)) {
        ImGui::Text("FPS: %.1f", TimeManager::GetFps());
        ImGui::Text("Delta Time: %.3f ms", TimeManager::GetDeltaTime() * 1000.0);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
}