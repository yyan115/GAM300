// Forward declaration for WindowManager
class WindowManager;

#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include "Panels/PanelManager.hpp"
#include <ECS/Entity.hpp>
#include "Utilities/GUID.hpp"

/**
 * @brief Main GUI management class for the editor.
 * 
 * The GUIManager serves as the editor "layer" that handles:
 * - Global ImGui setup and teardown
 * - Central dockspace layout creation
 * - Panel registration and management delegation
 * - Multi-viewport rendering coordination
 */
class GUIManager {
public:
	/**
	 * @brief Initialize the GUI system.
	 * Sets up ImGui context, configures docking and viewports, and registers default panels.
	 */
	static void Initialize();

	/**
	 * @brief Render the GUI system.
	 * Creates the main dockspace and renders all open panels.
	 */
	static void Render();

	/**
	 * @brief Clean up and exit the GUI system.
	 * Shuts down ImGui and cleans up resources.
	 */
	static void Exit();

	/**
	 * @brief Get the panel manager instance.
	 * @return Reference to the panel manager for external panel operations.
	 */
	static PanelManager& GetPanelManager() { return *panelManager; }

	/**
	 * @brief Get the currently selected entity.
	 * @return The selected entity ID, or static_cast<Entity>(-1) if none selected.
	 */
	static Entity GetSelectedEntity() { return selectedEntities.empty() ? static_cast<Entity>(-1) : selectedEntities[0]; }

	/**
	 * @brief Get the currently selected entities.
	 * @return Vector of selected entity IDs.
	 */
	static const std::vector<Entity>& GetSelectedEntities() { return selectedEntities; }

	/**
	 * @brief Check if an entity is selected.
	 * @param entity The entity to check.
	 * @return True if the entity is selected.
	 */
	static bool IsEntitySelected(Entity entity) { return std::find(selectedEntities.begin(), selectedEntities.end(), entity) != selectedEntities.end(); }

	/**
	 * @brief Set the currently selected entity.
	 * @param entity The entity to select, or static_cast<Entity>(-1) to deselect.
	 */
	static void SetSelectedEntity(Entity entity) { selectedEntities = entity == static_cast<Entity>(-1) ? std::vector<Entity>() : std::vector<Entity>{entity}; selectedAsset = GUID_128{0, 0}; }

	/**
	 * @brief Set the currently selected entities.
	 * @param entities The entities to select.
	 */
	static void SetSelectedEntities(const std::vector<Entity>& entities);

	/**
	 * @brief Add an entity to the selection.
	 * @param entity The entity to add.
	 */
	static void AddSelectedEntity(Entity entity);

	/**
	 * @brief Remove an entity from the selection.
	 * @param entity The entity to remove.
	 */
	static void RemoveSelectedEntity(Entity entity);

	/**
	 * @brief Clear all selected entities.
	 */
	static void ClearSelectedEntities();

	/**
	 * @brief Get the currently selected asset GUID.
	 * @return The selected asset GUID, or {0, 0} if none selected.
	 */
	static GUID_128 GetSelectedAsset() { return selectedAsset; }

	/**
	 * @brief Set the currently selected asset.
	 * @param assetGuid The asset GUID to select.
	 */
	static void SetSelectedAsset(const GUID_128& assetGuid) { selectedAsset = assetGuid; selectedEntities.clear(); }

private:
	/**
	 * @brief Set up the default editor panels.
	 * Registers and configures the core editor panels (Hierarchy, Inspector, Console, etc.).
	 */
	static void SetupDefaultPanels();

	/**
	 * @brief Create and configure the main editor dockspace.
	 * Sets up a Unity-like layout with docked windows.
	 */
	static void CreateDockspace();

	/**
	 * @brief Render the main menu bar.
	 * Provides access to panel toggles and editor functions.
	 */
	static void RenderMenuBar();

	static void CreateEditorTheme();

	/**
	 * @brief Opens a file dialog to select a scene file.
	 * @return The selected file path, or empty string if cancelled.
	 */
	static std::string OpenSceneFileDialog();

	static void HandleKeyboardShortcuts();
	static void ShowNotification(const std::string& message, float duration = 2.0f);
	static void RenderNotification();

	static std::unique_ptr<PanelManager> panelManager;
	static bool dockspaceInitialized;
	static std::vector<Entity> selectedEntities;
	static GUID_128 selectedAsset;

	static std::string notificationMessage;
	static float notificationTimer;
};