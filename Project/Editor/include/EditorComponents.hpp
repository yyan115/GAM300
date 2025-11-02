#pragma once
#include "imgui.h"
#include <string>

/**
 * @brief Centralized UI components and styles for the editor
 */
class EditorComponents {
public:

    // ===== Unity-Style Panel Background Colors =====
    // Asset Browser/Project panel - Medium grey (matches Unity)
    static constexpr ImVec4 PANEL_BG_ASSET_BROWSER = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);

    // Performance/utility panels - Same as Inspector (matches Unity)
    static constexpr ImVec4 PANEL_BG_UTILITY = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

    // Hierarchy panel - Same as Inspector/Performance for consistency
    static constexpr ImVec4 PANEL_BG_HIERARCHY = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

    // Inspector panel - MEDIUM-LIGHT grey
    static constexpr ImVec4 PANEL_BG_INSPECTOR = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

    // Scene/Game panels - Darker than Asset Browser
    static constexpr ImVec4 PANEL_BG_VIEWPORT = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

    // Scene header (darker than hierarchy for visible contrast)
    static constexpr ImVec4 PANEL_BG_SCENE_HEADER = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

    // ===== Unity-Style Slider Colors =====
    static constexpr ImVec4 SLIDER_BG = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);           // Dark background
    static constexpr ImVec4 SLIDER_GRAB = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);         // Grey grab handle
    static constexpr ImVec4 SLIDER_GRAB_ACTIVE = ImVec4(0.60f, 0.60f, 0.60f, 1.0f); // Lighter when active

    // ===== Unity-Style Dropdown/Combo Colors =====
    static constexpr ImVec4 COMBO_HEADER = ImVec4(0.22f, 0.37f, 0.56f, 1.0f);         // Unity grey-blue selection
    static constexpr ImVec4 COMBO_HEADER_HOVERED = ImVec4(0.30f, 0.30f, 0.30f, 1.0f); // Subtle grey hover
    static constexpr ImVec4 COMBO_HEADER_ACTIVE = ImVec4(0.22f, 0.37f, 0.56f, 1.0f);  // Match selected

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

    /**
     * @brief Draws a Unity-style scale slider with label and value display
     * @param label Label text (e.g., "Scale")
     * @param value Pointer to the scale value
     * @param min Minimum value
     * @param max Maximum value
     * @param sliderWidth Width of the slider (default 100)
     * @return True if value was changed
     */
    static bool DrawScaleSlider(const char* label, float* value, float min = 0.1f, float max = 2.0f, float sliderWidth = 100.0f);

    /**
     * @brief Push Unity-style combo/dropdown colors
     */
    static void PushComboColors();

    /**
     * @brief Pop Unity-style combo/dropdown colors
     */
    static void PopComboColors();

    /**
     * @brief Draws a Play button with green styling when active
     * @param isPlaying Whether playback is currently active
     * @param buttonWidth Width of the button
     * @return True if button was clicked
     */
    static bool DrawPlayButton(bool isPlaying, float buttonWidth);

    /**
     * @brief Draws a Pause button with orange styling when paused
     * @param isPaused Whether playback is currently paused
     * @param buttonWidth Width of the button
     * @return True if button was clicked
     */
    static bool DrawPauseButton(bool isPaused, float buttonWidth);

    /**
     * @brief Draws a Stop button with red styling
     * @param buttonWidth Width of the button (0 = full width)
     * @return True if button was clicked
     */
    static bool DrawStopButton(float buttonWidth = 0.0f);

private:
    // Helper to draw the custom highlight border
    static void DrawHighlightBorder();
};
