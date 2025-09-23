#pragma once

#include "EditorPanel.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

/**
 * @brief Panel containing play, pause, and stop buttons for game control.
 *
 * This panel provides a resizable interface for controlling game state,
 * separate from the main menu bar.
 */
class PlayControlPanel : public EditorPanel {
public:
    PlayControlPanel();
    virtual ~PlayControlPanel() = default;

    /**
     * @brief Render the play control panel's ImGui content.
     */
    void OnImGuiRender() override;

public:
    // Gizmo state accessors for ScenePanel
    ImGuizmo::OPERATION GetGizmoOperation() const { return gizmoOperation; }
    void SetGizmoOperation(ImGuizmo::OPERATION op) { gizmoOperation = op; hasToolSelected = true; }
    bool IsNormalPanMode() const { return isNormalPanMode; }
    void SetNormalPanMode(bool mode) { isNormalPanMode = mode; hasToolSelected = mode; }
    bool HasToolSelected() const { return hasToolSelected; }

private:
    // Transform tool state
    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
    bool isNormalPanMode = false;
    bool hasToolSelected = false; // Track if any tool is actively selected
    
    void RenderTransformTools();
};