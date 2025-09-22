#include "Panels/PlayControlPanel.hpp"
#include "EditorState.hpp"
#include "GUIManager.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"

PlayControlPanel::PlayControlPanel()
    : EditorPanel("Play Controls", true) {
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

        // Center the buttons within the toolbar
        ImVec2 windowSize = ImGui::GetContentRegionAvail();

        // Calculate actual button group width more accurately
        float playButtonWidth = 80.0f;
        float stopButtonWidth = 70.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x * 2; // Space between buttons
        float stateTextWidth = 60.0f; // Estimated width for " | EDIT"
        float totalButtonWidth = playButtonWidth + stopButtonWidth + stateTextWidth + spacing;

        float centerX = (windowSize.x - totalButtonWidth) * 0.5f;
        if (centerX > 0) {
            ImGui::SetCursorPosX(centerX);
        }

        // Add some vertical centering too
        float centerY = (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) * 0.5f;
        if (centerY > 0) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + centerY);
        }

        // Make buttons with good padding
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 4.0f));

        // Play/Pause button
        if (editorState.IsEditMode() || editorState.IsPaused()) {
            if (ImGui::Button(ICON_FA_PLAY " Play", ImVec2(80.0f, 0.0f))) {
                editorState.Play();
                // Auto-focus the Game panel when play is pressed
                auto gamePanel = GUIManager::GetPanelManager().GetPanel("Game");
                if (gamePanel) {
                    gamePanel->SetOpen(true);
                    ImGui::SetWindowFocus("Game");
                }
            }
        } else {
            if (ImGui::Button(ICON_FA_PAUSE " Pause", ImVec2(80.0f, 0.0f))) {
                editorState.Pause();
            }
        }

        ImGui::SameLine();

        // Stop button
        if (ImGui::Button(ICON_FA_STOP " Stop", ImVec2(70.0f, 0.0f))) {
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

        ImGui::PopStyleVar(); // FramePadding
    }
    ImGui::End();
    ImGui::PopStyleVar(3); // WindowPadding, WindowRounding, WindowBorderSize
}