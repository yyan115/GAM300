#include "pch.h"
#include "Panels/PlayControlPanel.hpp"
#include "EditorState.hpp"
#include "GUIManager.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"

PlayControlPanel::PlayControlPanel()
    : EditorPanel("Play Controls", true) {
    // Initialize with no tool selected by default
}

void PlayControlPanel::OnImGuiRender() {
    // Get viewport and set fixed position/size for toolbar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float menuBarHeight = ImGui::GetFrameHeight();

    // Position it at the top center, below the menu bar - extend height to cover separator
    ImVec2 toolbarPos = ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight - 1.0f);
    ImVec2 toolbarSize = ImVec2(viewport->Size.x, ImGui::GetFrameHeight() + 18.0f);

    ImGui::SetNextWindowPos(toolbarPos);
    ImGui::SetNextWindowSize(toolbarSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    // Set window padding and rounding for better spacing
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##PlayControlsToolbar", nullptr, flags)) {
        EditorState& editorState = EditorState::GetInstance();

        // Render transform tools on the left
        RenderTransformTools();
        
        // Add spacing and center play controls
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(40.0f, 0.0f)); // Increased spacing
        ImGui::SameLine();

        // Get available space after transform tools
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        
        // Calculate button group width
        float playButtonWidth = 80.0f;
        float stopButtonWidth = 80.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float stateTextWidth = 80.0f;
        float totalButtonWidth = playButtonWidth + stopButtonWidth + stateTextWidth + spacing;

        // Get the current cursor position for consistent baseline
        ImVec2 startPos = ImGui::GetCursorPos();
        
        // Center the button group in available space
        float centerOffset = (availableSize.x - totalButtonWidth) * 0.4f;
        if (centerOffset > 0) {
            ImGui::SetCursorPosX(startPos.x + centerOffset);
        }
        
        // Fixed height for all play/stop buttons
        float buttonHeight = 30.0f;
        
        // Set consistent vertical position for all buttons
        float toolbarHeight = toolbarSize.y; // Use the actual toolbar height we defined
        float centerY = (toolbarHeight - buttonHeight) * 0.5f;
        ImGui::SetCursorPosY(centerY);

        // Make buttons with good padding and ensure consistent alignment
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f)); // Center text

        // Store the Y position before drawing buttons for consistent alignment
        float buttonY = ImGui::GetCursorPosY();
        
        // Play/Pause button
        if (editorState.IsEditMode() || editorState.IsPaused()) {
            if (ImGui::Button(ICON_FA_PLAY " Play", ImVec2(80.0f, buttonHeight))) {
                editorState.Play();
                // Auto-focus the Game panel when play is pressed
                auto gamePanel = GUIManager::GetPanelManager().GetPanel("Game");
                if (gamePanel) {
                    gamePanel->SetOpen(true);
                    ImGui::SetWindowFocus("Game");
                }
            }
        } else {
            if (ImGui::Button(ICON_FA_PAUSE " Pause", ImVec2(80.0f, buttonHeight))) {
                editorState.Pause();
            }
        }

        ImGui::SameLine();
        
        // Ensure Stop button is at exactly the same Y position as Play button
        ImGui::SetCursorPosY(buttonY);

        // Stop button
        if (ImGui::Button(ICON_FA_STOP " Stop", ImVec2(80.0f, buttonHeight))) {
            editorState.Stop();
            // Auto-switch to Scene panel when stopping
            auto scenePanel = GUIManager::GetPanelManager().GetPanel("Scene");
            if (scenePanel) {
                scenePanel->SetOpen(true);
                ImGui::SetWindowFocus("Scene");
            }
        }

        ImGui::SameLine();

        // State indicator
        const char* stateText = editorState.IsEditMode() ? "EDIT" :
                               editorState.IsPlayMode() ? "PLAY" :
                               "PAUSED";
        ImGui::TextColored(editorState.IsPlayMode() ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) :
                          editorState.IsPaused() ? ImVec4(1.0f, 0.6f, 0.0f, 1.0f) :
                          ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " | %s", stateText);

        ImGui::PopStyleVar(2); // FramePadding and ButtonTextAlign
    }
    ImGui::End();
    ImGui::PopStyleVar(3); // WindowPadding, WindowRounding, WindowBorderSize
}

void PlayControlPanel::RenderTransformTools() {
    // Transform tool buttons with Unity-style layout and highlighting
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f)); // Increased padding
    
    float toolButtonHeight = 30.0f; // Same height as play/stop buttons
    
    // Center transform tools vertically in the toolbar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float menuBarHeight = ImGui::GetFrameHeight();
    float toolbarHeight = ImGui::GetFrameHeight() + 18.0f; // Same as toolbar size
    float centerY = (toolbarHeight - toolButtonHeight) * 0.5f;
    ImGui::SetCursorPosY(centerY);
    
    // Normal/Pan tool (Hand icon)
    bool isHandActive = (hasToolSelected && isNormalPanMode);
    if (isHandActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f)); // Highlighted
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    }
    if (ImGui::Button(ICON_FA_HAND " Q", ImVec2(60.0f, toolButtonHeight))) { // Increased width
        // Toggle pan mode - if already in pan mode, deselect all tools
        if (hasToolSelected && isNormalPanMode) {
            hasToolSelected = false;
            isNormalPanMode = false;
        } else {
            hasToolSelected = true;
            isNormalPanMode = true;
        }
    }
    if (isHandActive) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::SameLine();
    
    // Move tool (Arrows icon)
    bool isMoveActive = (hasToolSelected && !isNormalPanMode && gizmoOperation == ImGuizmo::TRANSLATE);
    if (isMoveActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f)); // Highlighted
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    }
    if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " W", ImVec2(60.0f, toolButtonHeight))) { // Increased width
        // Toggle translate mode - if already in translate mode, deselect all tools
        if (hasToolSelected && !isNormalPanMode && gizmoOperation == ImGuizmo::TRANSLATE) {
            hasToolSelected = false;
            isNormalPanMode = false;
        } else {
            hasToolSelected = true;
            isNormalPanMode = false;
            gizmoOperation = ImGuizmo::TRANSLATE;
        }
    }
    if (isMoveActive) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::SameLine();
    
    // Rotate tool (Rotate icon)
    bool isRotateActive = (hasToolSelected && !isNormalPanMode && gizmoOperation == ImGuizmo::ROTATE);
    if (isRotateActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f)); // Highlighted
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    }
    if (ImGui::Button(ICON_FA_ROTATE " E", ImVec2(60.0f, toolButtonHeight))) { // Increased width
        // Toggle rotate mode - if already in rotate mode, deselect all tools
        if (hasToolSelected && !isNormalPanMode && gizmoOperation == ImGuizmo::ROTATE) {
            hasToolSelected = false;
            isNormalPanMode = false;
        } else {
            hasToolSelected = true;
            isNormalPanMode = false;
            gizmoOperation = ImGuizmo::ROTATE;
        }
    }
    if (isRotateActive) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::SameLine();
    
    // Scale tool (Scale icon)
    bool isScaleActive = (hasToolSelected && !isNormalPanMode && gizmoOperation == ImGuizmo::SCALE);
    if (isScaleActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f)); // Highlighted
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    }
    if (ImGui::Button(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER " R", ImVec2(60.0f, toolButtonHeight))) { // Increased width
        // Toggle scale mode - if already in scale mode, deselect all tools
        if (hasToolSelected && !isNormalPanMode && gizmoOperation == ImGuizmo::SCALE) {
            hasToolSelected = false;
            isNormalPanMode = false;
        } else {
            hasToolSelected = true;
            isNormalPanMode = false;
            gizmoOperation = ImGuizmo::SCALE;
        }
    }
    if (isScaleActive) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::PopStyleVar(); // FramePadding
}