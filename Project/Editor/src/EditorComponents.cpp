#include "EditorComponents.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"

bool EditorComponents::DrawDragDropButton(const char* label, float width) {
    // Push Unity-style appearance
    ImGui::PushStyleColor(ImGuiCol_Button, DRAG_DROP_BUTTON_BG);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DRAG_DROP_BUTTON_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DRAG_DROP_BUTTON_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, DRAG_DROP_BUTTON_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DRAG_DROP_BUTTON_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(DRAG_DROP_BUTTON_PADDING_X, DRAG_DROP_BUTTON_PADDING_Y));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, DRAG_DROP_BUTTON_BORDER_SIZE);

    bool result = ImGui::Button(label, ImVec2(width, 0.0f)); // 0 height = auto-fit to text

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    return result;
}

bool EditorComponents::BeginDragDropTarget() {
    if (ImGui::BeginDragDropTarget()) {
        // Override ImGui's default yellow drag-drop highlight
        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Make default highlight invisible

        // Draw custom gray border overlay when dragging over
        DrawHighlightBorder();

        return true;
    }
    return false;
}

void EditorComponents::EndDragDropTarget() {
    ImGui::PopStyleColor(); // Pop DragDropTarget color
    ImGui::EndDragDropTarget();
}

bool EditorComponents::DrawDragDropSlot(const char* label, const std::string& displayText, float width, const char* tooltip) {
    ImGui::Text("%s", label);
    ImGui::SameLine();

    // Draw the button
    DrawDragDropButton(displayText.c_str(), width);

    // Check for drag-drop
    if (BeginDragDropTarget()) {
        if (tooltip) {
            ImGui::SetTooltip("%s", tooltip);
        }
        return true; // Caller should check for payload and call EndDragDropTarget()
    }

    return false;
}

bool EditorComponents::DrawScaleSlider(const char* label, float* value, float min, float max, float sliderWidth) {
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, SLIDER_BG);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, SLIDER_BG);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, SLIDER_BG);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, SLIDER_GRAB);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, SLIDER_GRAB_ACTIVE);

    // Draw label
    ImGui::Text("%s", label);
    ImGui::SameLine();

    // Draw slider with proper linear behavior
    ImGui::SetNextItemWidth(sliderWidth);
    char sliderID[64];
    snprintf(sliderID, sizeof(sliderID), "##%sSlider", label);
    bool changed = ImGui::SliderFloat(sliderID, value, min, max, "%.2f", ImGuiSliderFlags_None);

    // Draw value display on same line
    ImGui::SameLine();
    ImGui::Text("%.1fx", *value);

    ImGui::PopStyleColor(5);

    return changed;
}

void EditorComponents::PushComboColors() {
    ImGui::PushStyleColor(ImGuiCol_Header, COMBO_HEADER);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COMBO_HEADER_HOVERED);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, COMBO_HEADER_ACTIVE);
}

void EditorComponents::PopComboColors() {
    ImGui::PopStyleColor(3);
}

void EditorComponents::DrawHighlightBorder() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    draw_list->AddRect(
        p_min,
        p_max,
        IM_COL32(DRAG_HIGHLIGHT_R, DRAG_HIGHLIGHT_G, DRAG_HIGHLIGHT_B, DRAG_HIGHLIGHT_A),
        DRAG_DROP_BUTTON_ROUNDING,
        0,
        DRAG_HIGHLIGHT_BORDER_THICKNESS
    );
}

bool EditorComponents::DrawPlayButton(bool isPlaying, float buttonWidth) {
    ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ?
        ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

    bool clicked = ImGui::Button(ICON_FA_PLAY " Play", ImVec2(buttonWidth, 0));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Play");
    }

    ImGui::PopStyleColor(3);
    return clicked;
}

bool EditorComponents::DrawPauseButton(bool isPaused, float buttonWidth) {
    ImGui::PushStyleColor(ImGuiCol_Button, isPaused ?
        ImVec4(0.6f, 0.5f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.6f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.4f, 0.1f, 1.0f));

    bool clicked = ImGui::Button(ICON_FA_PAUSE " Pause", ImVec2(buttonWidth, 0));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Pause");
    }

    ImGui::PopStyleColor(3);
    return clicked;
}

bool EditorComponents::DrawStopButton(float buttonWidth) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

    float width = buttonWidth > 0.0f ? buttonWidth : ImGui::GetContentRegionAvail().x;
    bool clicked = ImGui::Button(ICON_FA_STOP " Stop", ImVec2(width, 0));

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Stop");
    }

    ImGui::PopStyleColor(3);
    return clicked;
}
