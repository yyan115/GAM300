#pragma once
#include "EditorPanel.hpp"
#include "Utilities/GUID.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include "imgui.h"

// Include FileWatch for hot-reloading
#include "FileWatch.hpp"

/**
 * @brief Unity-like Asset Browser panel for managing and viewing project assets.
 */
class AssetBrowserPanel : public EditorPanel {
public:
    AssetBrowserPanel();
    virtual ~AssetBrowserPanel();

    void OnImGuiRender() override;

private:
    // Asset information structure
    struct AssetInfo {
        std::string filePath;
        std::string fileName;
        std::string extension;
        GUID_128 guid;
        bool isDirectory;
        std::filesystem::file_time_type lastWriteTime;

        AssetInfo() = default;
        AssetInfo(const std::string& path, const GUID_128& g, bool isDir);
    };

    // Asset type enumeration for filtering
    enum class AssetType {
        All,
        Textures,
        Models,
        Shaders,
        Audio,
        Fonts
    };

    // UI state
    std::string currentDirectory;
    std::string rootAssetDirectory;
    std::vector<std::string> pathBreadcrumbs;
    std::string searchQuery;
    AssetType selectedAssetType;
    std::vector<AssetInfo> currentAssets;
    std::unordered_set<GUID_128> selectedAssets;
    GUID_128 lastSelectedAsset;
    GUID_128 renamingAsset;
    std::string renameBuffer;

    // Hot-reloading state
    std::atomic<bool> refreshPending{ false };
    std::unique_ptr<filewatch::FileWatch<std::string>> fileWatcher;

    // UI methods
    void RenderToolbar();
    void RenderFolderTree();
    void RenderAssetGrid();

    // Asset management
    void RefreshAssets();
    void NavigateToDirectory(const std::string& directory);
    void UpdateBreadcrumbs();
    bool PassesFilter(const AssetInfo& asset) const;
    AssetType GetAssetTypeFromExtension(const std::string& extension) const;

    // Hot-reloading methods
    void InitializeFileWatcher();
    void OnFileChanged(const std::string& filePath, const filewatch::Event& event);
    void ProcessFileChange(const std::string& relativePath, const filewatch::Event& event);
    void QueueRefresh();

    // Selection management
    void SelectAsset(const GUID_128& guid, bool multiSelect = false);
    void ClearSelection();
    bool IsAssetSelected(const GUID_128& guid) const;

    // Context menu
    void ShowAssetContextMenu(const AssetInfo& asset);

    // Drag and drop
    void HandleDragAndDrop(const AssetInfo& asset);

    // File operations
    void DeleteAsset(const AssetInfo& asset);
    void RevealInExplorer(const AssetInfo& asset);
    void CopyAssetPath(const AssetInfo& asset);
    void CreateNewScene(const std::string& directory);
    void RenameAsset(const AssetInfo& asset, const std::string& newName);

    // Utility methods
    std::string GetRelativePath(const std::string& fullPath) const;
    bool IsValidAssetFile(const std::string& extension) const;
    void EnsureDirectoryExists(const std::string& directory);

    // Tree rendering helper
    void RenderDirectoryNode(const std::filesystem::path& directory, const std::string& displayName);
};