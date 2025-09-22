#pragma once

#include <string>

/**
 * @brief Abstract base class for all editor panels in the GUI system.
 * 
 * Each panel represents a specific UI component (e.g., Scene Hierarchy, Inspector, Console).
 * Panels manage their own ImGui state and rendering logic.
 */
class EditorPanel {
public:
    EditorPanel(const std::string& panelName, bool isOpenByDefault = true);
    virtual ~EditorPanel() = default;

    /**
     * @brief Pure virtual method that derived panels must implement to render their ImGui content.
     */
    virtual void OnImGuiRender() = 0;

    /**
     * @brief Get the display name of this panel.
     * @return The panel's name as a string.
     */
    const std::string& GetName() const { return name; }

    /**
     * @brief Check if this panel is currently open/visible.
     * @return True if the panel should be rendered, false otherwise.
     */
    bool IsOpen() const { return isOpen; }

    /**
     * @brief Set the visibility state of this panel.
     * @param isOpen True to show the panel, false to hide it.
     */
    void SetOpen(bool CheckOpen) { this->isOpen = CheckOpen; }

    /**
     * @brief Toggle the visibility state of this panel.
     */
    void ToggleOpen() { isOpen = !isOpen; }

protected:
    std::string name;
    bool isOpen;
};