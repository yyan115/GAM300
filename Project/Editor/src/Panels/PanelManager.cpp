#include "Panels/PanelManager.hpp"
#include <algorithm>
#include "pch.h"

void PanelManager::RegisterPanel(std::shared_ptr<EditorPanel> panel) {
    assert(!panel->GetName().empty() && "Panel name cannot be empty");
    
    if (panel && !HasPanel(panel->GetName())) {
        panels.push_back(panel);
        panelMap[panel->GetName()] = panel;
    }
}

void PanelManager::UnregisterPanel(const std::string& panelName) {
    assert(!panelName.empty() && "Panel name cannot be empty");
    
    auto it = panelMap.find(panelName);
    if (it != panelMap.end()) {
        // Remove from vector
        panels.erase(
            std::remove_if(panels.begin(), panels.end(),
                [&panelName](const std::shared_ptr<EditorPanel>& panel) {
                    return panel && panel->GetName() == panelName;
                }),
            panels.end());
        
        // Remove from map
        panelMap.erase(it);
    }
}

std::shared_ptr<EditorPanel> PanelManager::GetPanel(const std::string& panelName) {
    assert(!panelName.empty() && "Panel name cannot be empty");
    
    auto it = panelMap.find(panelName);
    return (it != panelMap.end()) ? it->second : nullptr;
}

void PanelManager::RenderOpenPanels() {
    for (auto& panel : panels) {
        assert(panel != nullptr && "Panel in manager should not be null");
        
        if (panel && panel->IsOpen()) {
            panel->OnImGuiRender();
        }
    }
}

void PanelManager::TogglePanel(const std::string& panelName) {
    assert(!panelName.empty() && "Panel name cannot be empty");
    
    auto panel = GetPanel(panelName);
    if (panel) {
        panel->ToggleOpen();
    }
}

bool PanelManager::HasPanel(const std::string& panelName) const {
    assert(!panelName.empty() && "Panel name cannot be empty");
    
    return panelMap.find(panelName) != panelMap.end();
}