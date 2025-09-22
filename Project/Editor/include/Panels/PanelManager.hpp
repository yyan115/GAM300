#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "EditorPanel.hpp"

/**
 * @brief Manages all editor panels in the GUI system.
 * 
 * The PanelManager stores, organizes, and provides access to all registered editor panels.
 * It handles panel registration, retrieval, and batch operations like rendering all open panels.
 */
class PanelManager {
public:
    PanelManager() = default;
    ~PanelManager() = default;

    /**
     * @brief Register a new panel with the manager.
     * @param panel Shared pointer to the panel to register.
     */
    void RegisterPanel(std::shared_ptr<EditorPanel> panel);

    /**
     * @brief Unregister a panel by name.
     * @param panelName The name of the panel to remove.
     */
    void UnregisterPanel(const std::string& panelName);

    /**
     * @brief Get a panel by name.
     * @param panelName The name of the panel to retrieve.
     * @return Shared pointer to the panel, or nullptr if not found.
     */
    std::shared_ptr<EditorPanel> GetPanel(const std::string& panelName);

    /**
     * @brief Render all open panels by calling their OnImGuiRender() methods.
     */
    void RenderOpenPanels();

    /**
     * @brief Get all registered panels.
     * @return Vector of all registered panels.
     */
    const std::vector<std::shared_ptr<EditorPanel>>& GetAllPanels() const { return panels; }

    /**
     * @brief Toggle the visibility of a panel by name.
     * @param panelName The name of the panel to toggle.
     */
    void TogglePanel(const std::string& panelName);

    /**
     * @brief Check if a panel with the given name exists.
     * @param panelName The name of the panel to check.
     * @return True if the panel exists, false otherwise.
     */
    bool HasPanel(const std::string& panelName) const;

private:
    std::vector<std::shared_ptr<EditorPanel>> panels;
    std::unordered_map<std::string, std::shared_ptr<EditorPanel>> panelMap;
};