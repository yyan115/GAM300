// Forward declaration for WindowManager
class WindowManager;

#pragma once

#include <memory>
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
	static Entity GetSelectedEntity() { return selectedEntity; }

	/**
	 * @brief Set the currently selected entity.
	 * @param entity The entity to select, or static_cast<Entity>(-1) to deselect.
	 */
	static void SetSelectedEntity(Entity entity) { selectedEntity = entity; selectedAsset = GUID_128{0, 0}; }

	/**
	 * @brief Get the currently selected asset GUID.
	 * @return The selected asset GUID, or {0, 0} if none selected.
	 */
	static GUID_128 GetSelectedAsset() { return selectedAsset; }

	/**
	 * @brief Set the currently selected asset.
	 * @param assetGuid The asset GUID to select.
	 */
	static void SetSelectedAsset(const GUID_128& assetGuid) { selectedAsset = assetGuid; selectedEntity = static_cast<Entity>(-1); }

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

	static std::unique_ptr<PanelManager> panelManager;
	static bool dockspaceInitialized;
	static Entity selectedEntity;
	static GUID_128 selectedAsset;
};