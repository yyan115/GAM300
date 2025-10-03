#pragma once
#include "EditorPanel.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>
#include <FileWatch.hpp>
#include "Utilities/GUID.hpp"

/**
 * @brief Unity-like Asset Browser panel for managing and viewing project assets.
 */
class AssetBrowserPanel : public EditorPanel {
public:
    AssetBrowserPanel();
    virtual ~AssetBrowserPanel();

    void OnImGuiRender() override;

    // Static method to get file path from fallback GUID (for Inspector use)
    static std::string GetFallbackGuidFilePath(const GUID_128& guid);

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
        Fonts,
        Materials
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
    bool isOpeningScene = false;
    AssetInfo selectedScene;
    std::string pendingNavigation;

    // Hot-reloading state
    std::atomic<bool> refreshPending{ false };
    std::unique_ptr<filewatch::FileWatch<std::string>> fileWatcher;

    // Rename state
    bool isRenaming{ false };
    char renameBuffer[256]{ 0 };
    GUID_128 renamingAsset;

    // Delete confirmation state
    bool showDeleteConfirmation{ false };
    AssetInfo assetToDelete;

    // Thumbnail cache for texture previews (GUID -> Texture ID)
    // Using uint32_t instead of GLuint to avoid OpenGL dependency in header
    std::unordered_map<uint64_t, uint32_t> thumbnailCache;
    static constexpr int THUMBNAIL_SIZE = 96;
    
    // Directory tree state
    std::unordered_set<std::string> expandedDirectories;
    bool needsTreeSync{ false };

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
    void ShowCreateAssetMenu();

    // Drag and drop
    void HandleDragAndDrop(const AssetInfo& asset);

    // File operations
    void DeleteAsset(const AssetInfo& asset);
    void ConfirmDeleteAsset();
    void RevealInExplorer(const AssetInfo& asset);
    void CopyAssetPath(const AssetInfo& asset);
    void RenameAsset(const AssetInfo& asset, const std::string& newName);

    // Asset creation
    void CreateNewMaterial();
    void CreateNewFolder();

    // Scene operations
    void CreateNewScene(const std::string& directory);
    void OpenScene(const AssetInfo& selectedScene);
    void ShowOpenSceneConfirmation();

    // Rename functionality
    void StartRenameAsset(const GUID_128& guid);
    void CancelRename();
    void ConfirmRename();

    // Utility methods
    std::string GetRelativePath(const std::string& fullPath) const;
    bool IsValidAssetFile(const std::string& extension) const;
    void EnsureDirectoryExists(const std::string& directory);

    // Tree rendering helper
    void RenderDirectoryNode(const std::filesystem::path& directory, const std::string& displayName);

    // Icon retrieval
    std::string GetAssetIcon(const AssetInfo& asset) const;

    // Thumbnail management (Unity-like)
    uint32_t GetOrCreateThumbnail(const GUID_128& guid, const std::string& assetPath);
    void ClearThumbnailCache();
    void RemoveThumbnailFromCache(const GUID_128& guid);
    
    // Directory tree helpers
    void EnsureDirectoryExpanded(const std::string& directoryPath);
    void SyncTreeWithCurrentDirectory();
};