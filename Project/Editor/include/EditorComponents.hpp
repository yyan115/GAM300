#pragma once
#include "imgui.h"
#include <string>

/**
 * @brief Centralized UI components and styles for the editor
 */
class EditorComponents {
public:

    // Button Colors (RGB 0-1 range)
    static constexpr ImVec4 DRAG_DROP_BUTTON_BG = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);          // Dark background
    static constexpr ImVec4 DRAG_DROP_BUTTON_HOVER = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);       // Same as bg - no hover effect
    static constexpr ImVec4 DRAG_DROP_BUTTON_ACTIVE = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);      // Same as bg
    static constexpr ImVec4 DRAG_DROP_BUTTON_BORDER = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);      // Default border color

    // Drag Highlight Border (RGB 0-255 range for ImDrawList)
    static constexpr int DRAG_HIGHLIGHT_R = 180;
    static constexpr int DRAG_HIGHLIGHT_G = 180;
    static constexpr int DRAG_HIGHLIGHT_B = 180;
    static constexpr int DRAG_HIGHLIGHT_A = 255;

    // Button Appearance
    static constexpr float DRAG_DROP_BUTTON_ROUNDING = 3.0f;           // Corner radius
    static constexpr float DRAG_DROP_BUTTON_PADDING_X = 8.0f;          // Horizontal padding
    static constexpr float DRAG_DROP_BUTTON_PADDING_Y = 2.0f;          // Vertical padding (controls height)
    static constexpr float DRAG_DROP_BUTTON_BORDER_SIZE = 1.0f;        // Border thickness
    static constexpr float DRAG_HIGHLIGHT_BORDER_THICKNESS = 3.0f;     // Drag overlay border thickness

    // ===== Public Methods =====

    /**
     * @brief Draws a drag-drop button
     * @param label The text to display on the button
     * @param width The width of the button (0 = auto-fit)
     * @return True if the button was visible and rendered
     */
    static bool DrawDragDropButton(const char* label, float width = 0.0f);

    /**
     * @brief Begins a drag-drop target with visual feedback
     * Call this after DrawDragDropButton, then check for payload and call EndDragDropTarget
     * @return True if dragging over this target
     */
    static bool BeginDragDropTarget();

    /**
     * @brief Ends the drag-drop target and restores styles
     * Call this after processing the drag-drop payload
     */
    static void EndDragDropTarget();

    /**
     * @brief Convenience function to draw a complete drag-drop slot
     * @param label Label text (e.g., "Texture:", "Material:")
     * @param displayText Text to show in the button (e.g., filename or "None (Texture)")
     * @param width Width of the button (0 = auto-fit)
     * @param tooltip Tooltip to show when hovering during drag
     * @return True if currently being dragged over (ready to accept payload)
     */
    static bool DrawDragDropSlot(const char* label, const std::string& displayText, float width, const char* tooltip);

private:
    // Helper to draw the custom highlight border
    static void DrawHighlightBorder();
};
