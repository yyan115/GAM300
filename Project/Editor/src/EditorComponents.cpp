#include "EditorComponents.hpp"

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
