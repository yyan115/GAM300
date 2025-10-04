#include "pch.h"
#include "Panels/AssetBrowserPanel.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Graphics/Texture.h"
#include "Graphics/Material.hpp"
#include "GUIManager.hpp"
#include "EditorComponents.hpp"
#include "Prefab.hpp"
#include "PrefabIO.hpp"
#include "PrefabComponent.hpp"
#include "Reflection/ReflectionBase.hpp"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/NameComponent.hpp"
#include "Logging.hpp"
#include "Scene/SceneManager.hpp"
#include "Panels/PrefabEditorPanel.hpp"
#include "Utilities/GUID.hpp"
#include <IconsFontAwesome6.h>
#include <FileWatch.hpp>

// Global drag-drop state for cross-window material dragging
GUID_128 DraggedMaterialGuid = {0, 0};
std::string DraggedMaterialPath;

// Global drag-drop state for cross-window model dragging
GUID_128 DraggedModelGuid = {0, 0};
std::string DraggedModelPath;

// Global drag-drop state for cross-window audio dragging
GUID_128 DraggedAudioGuid = {0, 0};
std::string DraggedAudioPath;

// Global drag-drop state for cross-window font dragging
GUID_128 DraggedFontGuid = {0, 0};
std::string DraggedFontPath;

// Global fallback GUID to file path mapping for assets without proper meta files
static std::unordered_map<uint64_t, std::string> FallbackGuidToPath;

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// Thumbnail/grid
static constexpr float THUMBNAILBASESIZE = 96.0f;
static constexpr float THUMBNAILMINSIZE = 48.0f;
static constexpr float THUMBNAILPADDING = 8.0f;
static constexpr float LABELHEIGHT = 18.0f;

// Helper: add a component T to the JSON if the entity has it.
// JSON layout:  { "<TypeName>": { "type": "...", "data": ... } }
template <typename T>
static void AddIfHas(ECSManager& ecs, Entity e, rapidjson::Document& doc, const char* jsonKey)
{
    if (!ecs.HasComponent<T>(e)) return;

    const T& comp = ecs.GetComponent<T>(e);

    // Use your reflection to serialize to text, then parse into a Value.
    std::stringstream ss;
    TypeResolver<T>::Get()->Serialize(&comp, ss);

    rapidjson::Document tmp;
    tmp.Parse(ss.str().c_str());              // {"type":"...","data":...}

    auto& alloc = doc.GetAllocator();
    rapidjson::Value key(jsonKey, alloc);
    rapidjson::Value val(tmp, alloc);         // deep-copy into doc’s allocator

    doc.AddMember(key, val, alloc);
}

static std::filesystem::path MakeUniquePath(std::filesystem::path p)
{
    using namespace std::filesystem;
    if (!exists(p)) return p;
    path stem = p.stem();
    path ext = p.extension();
    path dir = p.parent_path();
    int i = 1;
    for (;; ++i) {
        path cand = dir / (stem.string() + " (" + std::to_string(i) + ")" + ext.string());
        if (!exists(cand)) return cand;
    }
}

AssetBrowserPanel::AssetInfo::AssetInfo(const std::string& path, const GUID_128& g, bool isDir)
    : filePath(path), guid(g), isDirectory(isDir) {
    fileName = std::filesystem::path(path).filename().string();
    extension = std::filesystem::path(path).extension().string();
}

AssetBrowserPanel::AssetBrowserPanel()
    : EditorPanel("Asset Browser", true)
    , currentDirectory(AssetManager::GetInstance().GetRootAssetDirectory())
    , rootAssetDirectory(AssetManager::GetInstance().GetRootAssetDirectory())
    , selectedAssetType(AssetType::All)
{
    // Initialize default GUID for untracked assets
    lastSelectedAsset = GUID_128{ 0, 0 };

    // Ensure assets directory exists
    EnsureDirectoryExists(rootAssetDirectory);

    // Initialize file watcher for hot-reloading
    InitializeFileWatcher();
    
    // Always expand Resources folder by default (Unity behavior)
    expandedDirectories.insert(rootAssetDirectory);
}

AssetBrowserPanel::~AssetBrowserPanel() {
    // Clean up thumbnail cache
    ClearThumbnailCache();
    // FileWatch destructor will handle cleanup automatically
}

void AssetBrowserPanel::InitializeFileWatcher() {
    try {
        // Create file watcher for the Resources directory
        fileWatcher = std::make_unique<filewatch::FileWatch<std::string>>(
            rootAssetDirectory,
            [this](const std::string& path, const filewatch::Event& event) {
                OnFileChanged(path, event);
            }
        );
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Failed to initialize file watcher: ", e.what(), "\n");
    }
}

void AssetBrowserPanel::OnFileChanged(const std::string& filePath, const filewatch::Event& event) {
    // Process file change on a separate thread-safe context
    ProcessFileChange(filePath, event);
}

void AssetBrowserPanel::ProcessFileChange(const std::string& relativePath, const filewatch::Event& event) {
    // Log the file change for debugging
    const char* eventStr = "";
    switch (event) {
    case filewatch::Event::added: eventStr = "ADDED"; break;
    case filewatch::Event::removed: eventStr = "REMOVED"; break;
    case filewatch::Event::modified: eventStr = "MODIFIED"; break;
    case filewatch::Event::renamed_old: eventStr = "RENAMED_OLD"; break;
    case filewatch::Event::renamed_new: eventStr = "RENAMED_NEW"; break;
    }

    // std::cout << "[AssetBrowserPanel] File " << eventStr << ": " << relativePath << std::endl;

    // Build full path from rootAssetDirectory + relativePath
    std::filesystem::path fullPathPath = std::filesystem::path(rootAssetDirectory) / relativePath;
    const std::string fullPath = fullPathPath.generic_string();
    std::filesystem::path fullPathObj(fullPath);
    try {
        if (std::filesystem::exists(fullPathObj) && std::filesystem::is_directory(fullPathObj)) {
            // Directory created/modified � refresh UI
            QueueRefresh();
            return;
        }
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Filesystem check error for ", fullPath, ": ", e.what(), "\n");
    }

    // Only process valid asset files
    std::filesystem::path pathObj(relativePath);
    std::string extension = pathObj.extension().string();
    if (!IsValidAssetFile(extension) && event != filewatch::Event::removed) {
        return;
    }

    // Handle different file events
    if (AssetManager::GetInstance().IsAssetExtensionSupported(extension) && !AssetManager::GetInstance().IsExtensionMaterial(extension)) {
        // Sleep this thread for a while to allow the OS to finish the file operation.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (event == filewatch::Event::modified || event == filewatch::Event::added) {
            if (event == filewatch::Event::modified)
                AssetManager::GetInstance().AddToEventQueue(AssetManager::Event::modified, fullPathObj);
            else
                AssetManager::GetInstance().AddToEventQueue(AssetManager::Event::added, fullPathObj);
            
            // Invalidate thumbnail cache for modified textures (Unity behavior)
            //if (AssetManager::GetInstance().IsExtensionTexture(extension)) {
            //    if (MetaFilesManager::MetaFileExists(fullPath)) {
            //        GUID_128 guid = MetaFilesManager::GetGUID128FromAssetFile(fullPath);
            //        RemoveThumbnailFromCache(guid);
            //    }
            //}
        }
        else if (event == filewatch::Event::removed) {
            // Remove from thumbnail cache before unloading
            if (AssetManager::GetInstance().IsExtensionTexture(extension)) {
                if (MetaFilesManager::MetaFileExists(fullPath)) {
                    GUID_128 guid = MetaFilesManager::GetGUID128FromAssetFile(fullPath);
                    RemoveThumbnailFromCache(guid);
                }
            }
            AssetManager::GetInstance().UnloadAsset(fullPath);
        }
        else if (event == filewatch::Event::renamed_old) {
            AssetManager::GetInstance().UnloadAsset(fullPath);
        }
        else if (event == filewatch::Event::renamed_new) {
            AssetManager::GetInstance().AddToEventQueue(AssetManager::Event::modified, fullPathObj);
        }

        QueueRefresh();
    }
    else if (AssetManager::GetInstance().IsExtensionMetaFile(extension)) {
        if (event == filewatch::Event::removed) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[AssetBrowserPanel] Detected removal of .meta file: ", fullPath, "\n");
            AssetManager::GetInstance().HandleMetaFileDeletion(fullPath);

            QueueRefresh();
        }
    }
    else if (ResourceManager::GetInstance().IsResourceExtensionSupported(extension)) {
        if (event == filewatch::Event::removed) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[AssetBrowserPanel] Detected removal of resource file: ", fullPath, "\n");
            AssetManager::GetInstance().HandleResourceFileDeletion(fullPath);

            QueueRefresh();
        }
    }

    //switch (event) {
    //case filewatch::Event::added:
    //    if (!MetaFilesManager::MetaFileExists(fullPath)) {
    //        MetaFilesManager::GenerateMetaFile(fullPath);
    //    }
    //    QueueRefresh();
    //    break;
    //case filewatch::Event::renamed_new:
    //    // For new files, ensure meta file is generated
    //    if (event == filewatch::Event::added) {
    //        std::string fullPath = rootAssetDirectory + "/" + relativePath;
    //        if (!MetaFilesManager::MetaFileExists(fullPath)) {
    //            MetaFilesManager::GenerateMetaFile(fullPath);
    //        }
    //    }
    //    QueueRefresh();
    //    break;

    //case filewatch::Event::removed:
    //    QueueRefresh();
    //    break;
    //case filewatch::Event::renamed_old:
    //    // For removed files, we should clean up meta files
    //    QueueRefresh();
    //    break;

    //case filewatch::Event::modified:
    //    // For modified files, update meta file if needed
    //{
    //    std::string fullPath = rootAssetDirectory + "/" + relativePath;
    //    if (MetaFilesManager::MetaFileExists(fullPath)) {
    //        MetaFilesManager::UpdateMetaFile(fullPath);
    //    }
    //    QueueRefresh();
    //    break;
    //}
}

void AssetBrowserPanel::QueueRefresh() {
    // Set atomic flag to indicate refresh is needed
    refreshPending.store(true);
}

void AssetBrowserPanel::OnImGuiRender() {
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_ASSET_BROWSER);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_ASSET_BROWSER);

    // Check if refresh is needed (from file watcher)
    if (refreshPending.exchange(false)) {
        // std::cout << "[AssetBrowserPanel] Refreshing assets due to file changes." << std::endl;
        RefreshAssets();
    }

    // Sync directory tree if needed (Unity behavior)
    if (needsTreeSync) {
        SyncTreeWithCurrentDirectory();
    }

    // Handle F2 key for renaming
    if (!isRenaming && ImGui::IsKeyPressed(ImGuiKey_F2) && !selectedAssets.empty()) {
        // Start renaming the last selected asset
        StartRenameAsset(lastSelectedAsset);
    }

    // Handle Delete key for deleting
    if (!isRenaming && ImGui::IsKeyPressed(ImGuiKey_Delete) && !selectedAssets.empty()) {
        // Find the selected asset and trigger delete confirmation
        for (const auto& asset : currentAssets) {
            if (IsAssetSelected(asset.guid)) {
                DeleteAsset(asset);
                break; // Delete the first selected asset
            }
        }
    }

    // Handle rename confirmation/cancellation
    if (isRenaming) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            ConfirmRename();
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            CancelRename();
        }
    }

    if (ImGui::Begin(name.c_str(), &isOpen)) {
        // Render toolbar
        RenderToolbar();
        ImGui::Separator();

        // Create splitter for folder tree and asset grid
        ImGui::BeginChild("##AssetBrowserContent", ImVec2(0, 0), false);

        // Use splitter to divide left panel (folder tree) and right panel (asset grid)
        static float splitterWidth = 250.0f;
        const float MIN_WIDTH = 150.0f;
        const float maxWidth = ImGui::GetContentRegionAvail().x - 200.0f;

        
        ImGui::BeginChild("##FolderTree", ImVec2(splitterWidth, 0), false);  // No border
        RenderFolderTree();
        ImGui::EndChild();

        ImGui::SameLine();

        // Splitter bar - darker to blend better
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::Button("##Splitter", ImVec2(8.0f, -1));
        ImGui::PopStyleColor();
        if (ImGui::IsItemActive()) {
            float delta = ImGui::GetIO().MouseDelta.x;
            splitterWidth += delta;
            splitterWidth = std::clamp(splitterWidth, MIN_WIDTH, maxWidth);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        ImGui::SameLine();

        ImGui::BeginChild("##AssetGrid", ImVec2(0, 0), false);  // No border
        RenderAssetGrid();
        ImGui::EndChild();

        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleColor(2);  // Pop WindowBg and ChildBg colors

    // Delete confirmation popup
    if (showDeleteConfirmation) {
        ImGui::OpenPopup("Delete Asset");
        showDeleteConfirmation = false; // Only open once
    }

    if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this file?");
        ImGui::Separator();
        ImGui::Text("File: %s", assetToDelete.fileName.c_str());
        ImGui::Text("Path: %s", assetToDelete.filePath.c_str());
        ImGui::Separator();

        // Center the buttons
        float buttonWidth = 60.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = buttonWidth * 2 + spacing;
        float offset = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
        if (offset > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        if (ImGui::Button("Yes", ImVec2(buttonWidth, 0))) {
            ConfirmDeleteAsset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(buttonWidth, 0))) {
            ImGui::CloseCurrentPopup();
        }

        // Close with Escape key
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::RenderToolbar() {
    // Breadcrumb navigation
    ImGui::Text("Path:");
    ImGui::SameLine();

    if (ImGui::SmallButton(ICON_FA_HOUSE " Resources")) {
        NavigateToDirectory(rootAssetDirectory);
    }

    for (size_t i = 0; i < pathBreadcrumbs.size(); ++i) {
        ImGui::SameLine();
        ImGui::Text("/");
        ImGui::SameLine();

        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton(pathBreadcrumbs[i].c_str())) {
            std::string targetPath = rootAssetDirectory;
            for (size_t j = 0; j <= i; ++j) {
                targetPath += "/" + pathBreadcrumbs[j];
            }
            NavigateToDirectory(targetPath);
        }
        ImGui::PopID();
    }

    ImVec2 button1Size = ImGui::CalcTextSize(ICON_FA_FOLDER_PLUS " New Folder");
    ImVec2 button2Size = ImGui::CalcTextSize(ICON_FA_FILE_IMPORT " Import");
    float totalButtonWidth = button1Size.x + button2Size.x + ImGui::GetStyle().ItemSpacing.x;  // Include spacing

    // Position buttons at the right end of the available space (after breadcrumbs)
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availWidth - totalButtonWidth);

    if (ImGui::Button(ICON_FA_FOLDER_PLUS " New Folder")) {
        std::string newFolderPath = currentDirectory + "/New Folder";
        EnsureDirectoryExists(newFolderPath);
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_IMPORT " Import")) {
        // TODO: Implement import dialog
    }

    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));

    ImGui::SetNextItemWidth(250.0f);
    char searchBuffer[256];
#ifdef _WIN32
    strncpy_s(searchBuffer, searchQuery.c_str(), sizeof(searchBuffer) - 1);
#else
    strncpy(searchBuffer, searchQuery.c_str(), sizeof(searchBuffer) - 1);
#endif
    searchBuffer[sizeof(searchBuffer) - 1] = '\0';

    if (ImGui::InputTextWithHint("##Search", ICON_FA_MAGNIFYING_GLASS " Search assets...", searchBuffer, sizeof(searchBuffer))) {
        searchQuery = searchBuffer;
    }

    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);

    const char* assetTypeNames[] = { "All", "Textures", "Models", "Shaders", "Audio", "Fonts", "Materials" };
    int currentTypeIndex = static_cast<int>(selectedAssetType);

    if (ImGui::Combo("##Filter", &currentTypeIndex, assetTypeNames, IM_ARRAYSIZE(assetTypeNames))) {
        selectedAssetType = static_cast<AssetType>(currentTypeIndex);
    }
}

void AssetBrowserPanel::RenderFolderTree() {
    ImGui::Text("Folders");
    ImGui::Separator();

    // Render root directory
    if (std::filesystem::exists(rootAssetDirectory)) {
        RenderDirectoryNode(std::filesystem::path(rootAssetDirectory), "Resources");
    }
}

void AssetBrowserPanel::RenderDirectoryNode(const std::filesystem::path& directory, const std::string& displayName) {
    bool hasSubdirectories = false;

    // Check if this directory has subdirectories
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_directory()) {
                hasSubdirectories = true;
                break;
            }
        }
    }
    catch (const std::exception&) {
        // Ignore errors for inaccessible directories
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasSubdirectories) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    // Highlight if this is the current directory
    std::string dirPathStr = std::filesystem::path(directory).generic_string();
    if (dirPathStr == currentDirectory) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Use unique ID per node for consistent state storage
    std::string nodeId = dirPathStr;

    // Check if this directory should be expanded
    bool shouldExpand = expandedDirectories.find(dirPathStr) != expandedDirectories.end();
    
    // Set the icon based on the current tree state (check ImGui's internal state)
    ImGuiID id = ImGui::GetID(nodeId.c_str());
    bool isCurrentlyOpen = ImGui::GetStateStorage()->GetBool(id, shouldExpand);
    std::string icon = isCurrentlyOpen ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER_CLOSED;
    std::string label = icon + " " + displayName;

    // Apply default open state if directory should be expanded (only once, not always)
    if (shouldExpand) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    // Use TreeNodeEx with fixed str_id for consistent state, and dynamic label
    bool nodeOpen = ImGui::TreeNodeEx(nodeId.c_str(), flags, "%s", label.c_str());

    // Update expanded state based on actual tree node state
    // Only update if the state changed (user clicked arrow)
    bool wasExpanded = expandedDirectories.find(dirPathStr) != expandedDirectories.end();
    if (nodeOpen && !wasExpanded) {
        expandedDirectories.insert(dirPathStr);
    } else if (!nodeOpen && wasExpanded) {
        expandedDirectories.erase(dirPathStr);
    }

    // Handle selection
    if (ImGui::IsItemClicked()) {
        NavigateToDirectory(directory.string());
    }

    // Render subdirectories if opened
    if (nodeOpen) {
        if (hasSubdirectories) {
            try {
                std::vector<std::filesystem::path> subdirectories;
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_directory() && entry.path().generic_string().find("Shaders") == std::string::npos) { // Don't show Shaders folder
                        subdirectories.push_back(entry.path());
                    }
                }

                // Sort subdirectories
                std::sort(subdirectories.begin(), subdirectories.end());

                for (const auto& subdir : subdirectories) {
                    RenderDirectoryNode(subdir, subdir.filename().string());
                }
            }
            catch (const std::exception&) {
                // Ignore errors for inaccessible directories
            }
        }
        ImGui::TreePop();
    }
}

void AssetBrowserPanel::RenderAssetGrid()
{
    ImGui::PushID(this); // unique ID space for this panel

    // Header
    ImGui::Text("Assets in: %s", GetRelativePath(currentDirectory).c_str());
    ImGui::Separator();

    // ----------------------- GRID -----------------------
    const float availX = ImGui::GetContentRegionAvail().x;
    const float pad = THUMBNAILPADDING;

    int   cols = std::max(1, static_cast<int>(std::floor((availX + pad) / (THUMBNAILBASESIZE + pad))));
    float thumb = (availX - pad * (cols - 1)) / static_cast<float>(cols);
    if (thumb < THUMBNAILMINSIZE) {
        thumb = THUMBNAILMINSIZE;
        cols = std::max(1, static_cast<int>(std::floor((availX + pad) / (thumb + pad))));
        thumb = (availX - pad * (cols - 1)) / static_cast<float>(cols);
    }

    bool anyItemClickedInGrid = false; // on click (on the same frame only)
    ImGuiIO& io = ImGui::GetIO();

    int index = 0;
    for (const auto& asset : currentAssets)
    {
        if (!PassesFilter(asset)) continue;

        ImGui::BeginGroup();
        ImGui::PushID(asset.filePath.c_str());

        // unified hitbox = thumbnail + label
        ImGui::InvisibleButton("cell", ImVec2(thumb, thumb + LABELHEIGHT));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked() || ImGui::IsItemClicked(ImGuiMouseButton_Right);
        const bool released = ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        //bool doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();

        // drag source: prefab -> scene, material/texture -> inspector
        if (!asset.isDirectory) {
            std::string lowerExt = asset.extension;
            std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
            
            bool isMaterial = (lowerExt == ".mat");
            bool isTexture = (lowerExt == ".png" || lowerExt == ".jpg" ||
                             lowerExt == ".jpeg" || lowerExt == ".bmp" ||
                             lowerExt == ".tga" || lowerExt == ".dds");
            bool isModel = (lowerExt == ".obj" || lowerExt == ".fbx" ||
                           lowerExt == ".dae" || lowerExt == ".3ds");
            bool isAudio = (lowerExt == ".wav" || lowerExt == ".ogg" ||
                           lowerExt == ".mp3" || lowerExt == ".flac");
            bool isFont = (lowerExt == ".ttf" || lowerExt == ".otf");
            bool isPrefab = (lowerExt == ".prefab");

            // Handle drag-drop for various asset types
            if ((isMaterial || isTexture || isModel || isAudio || isFont) && ImGui::BeginDragDropSource()) {
                if (isMaterial) {
                    // Store drag data globally for cross-window transfer
                    DraggedMaterialGuid = asset.guid;
                    DraggedMaterialPath = asset.filePath;

                    // Use a simple payload - just a flag that dragging is active
                    ImGui::SetDragDropPayload("MATERIAL_DRAG", nullptr, 0);
                    ImGui::Text("Dragging Material: %s", asset.fileName.c_str());
                } else if (isTexture) {
                    // Send texture path directly
                    ImGui::SetDragDropPayload("TEXTURE_PAYLOAD", asset.filePath.c_str(), asset.filePath.size() + 1);
                    ImGui::Text("Dragging Texture: %s", asset.fileName.c_str());
                } else if (isModel) {
                    // Store drag data globally for cross-window transfer
                    DraggedModelGuid = asset.guid;
                    DraggedModelPath = asset.filePath;

                    // Use a simple payload - just a flag that dragging is active
                    ImGui::SetDragDropPayload("MODEL_DRAG", nullptr, 0);
                    ImGui::Text("Dragging Model: %s", asset.fileName.c_str());
                } else if (isAudio) {
                    // Store drag data globally for cross-window transfer
                    DraggedAudioGuid = asset.guid;
                    DraggedAudioPath = asset.filePath;

                    // Use a simple payload - just a flag that dragging is active
                    ImGui::SetDragDropPayload("AUDIO_DRAG", nullptr, 0);
                    ImGui::Text("Dragging Audio: %s", asset.fileName.c_str());
                } else if (isFont) {
                    // Send font path directly
                    ImGui::SetDragDropPayload("FONT_PAYLOAD", asset.filePath.c_str(), asset.filePath.size() + 1);
                    ImGui::Text("Dragging Font: %s", asset.fileName.c_str());
                }

                ImGui::EndDragDropSource();
            }
            else if (isPrefab && hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    const std::string absPath = std::filesystem::absolute(asset.filePath).generic_string();
                    ImGui::SetDragDropPayload("PREFAB_PATH", absPath.c_str(),
                        static_cast<int>(absPath.size()) + 1);
                    ImGui::Text("Prefab: %s", asset.fileName.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }

        // double click
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (asset.isDirectory) {
                NavigateToDirectory(asset.filePath);
            }
            else {
                std::string lowerExt = asset.extension;
                std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
                if (lowerExt == ".prefab") {
                    GUIManager::SetSelectedAsset(GUID_128{ 0, 0 });

                    // Open the prefab editor
                    PrefabEditor::Open(asset.filePath);
                    // Early return so the rest of this frame doesn't re-use selection state
                    ImGui::PopID();
                    ImGui::EndGroup();
                    break;
                }
                else {
                    ENGINE_PRINT("[AssetBrowserPanel] Opening asset: GUID(high=", asset.guid.high, ", low=", asset.guid.low, ")\n");
                }
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 rectMax = ImGui::GetItemRectMax();
        ImVec2 imgMin = rectMin;
        ImVec2 imgMax = ImVec2(rectMin.x + thumb, rectMin.y + thumb);

        // Check if this is a texture asset and render thumbnail
        std::string lowerExt = asset.extension;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
        bool isTextureAsset = (lowerExt == ".png" || lowerExt == ".jpg" || 
                              lowerExt == ".jpeg" || lowerExt == ".bmp" || 
                              lowerExt == ".tga" || lowerExt == ".dds");
        
        if (isTextureAsset && !asset.isDirectory) {
            // Unity-like: Show actual texture thumbnail instead of icon
            uint32_t textureId = GetOrCreateThumbnail(asset.guid, asset.filePath);
            
            if (textureId != 0) {
                // Add subtle border for texture thumbnails
                dl->AddRect(imgMin, imgMax, IM_COL32(80, 80, 80, 120), 4.0f, ImDrawFlags_RoundCornersAll, 1.0f);
                
                // Calculate UV coordinates to maintain aspect ratio (centered crop)
                // For simplicity, we use full texture here (Unity does smart cropping)
                ImVec2 uv0(0.0f, 0.0f);
                ImVec2 uv1(1.0f, 1.0f);
                
                // Add padding to the thumbnail to create a border effect
                float padding = 4.0f;
                ImVec2 texMin = ImVec2(imgMin.x + padding, imgMin.y + padding);
                ImVec2 texMax = ImVec2(imgMax.x - padding, imgMax.y - padding);
                
                // Draw the texture
                dl->AddImage(
                    (ImTextureID)(intptr_t)textureId,
                    texMin, texMax,
                    uv0, uv1,
                    IM_COL32(255, 255, 255, 255)
                );
            } else {
                // Fallback to icon if texture failed to load
                std::string icon = GetAssetIcon(asset);
                ImFont* font = ImGui::GetFont();
                ImVec2 defaultIconSize = ImGui::CalcTextSize(icon.c_str());
                float scale = (defaultIconSize.y > 0.0f) ? (thumb / defaultIconSize.y) : 1.0f;
                scale *= 0.8f;
                float fontSize = ImGui::GetFontSize() * scale;
                ImVec2 iconSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, icon.c_str());
                ImVec2 iconPos = ImVec2(
                    imgMin.x + (thumb - iconSize.x) * 0.5f,
                    imgMin.y + (thumb - iconSize.y) * 0.5f
                );
                dl->AddText(font, fontSize, iconPos, IM_COL32(220, 220, 220, 255), icon.c_str());
            }
        } else {
            // For non-texture assets, draw icon (existing behavior)
            std::string icon = GetAssetIcon(asset);
            ImFont* font = ImGui::GetFont();
            ImVec2 defaultIconSize = ImGui::CalcTextSize(icon.c_str());
            float scale = (defaultIconSize.y > 0.0f) ? (thumb / defaultIconSize.y) : 1.0f;
            scale *= 0.8f;
            float fontSize = ImGui::GetFontSize() * scale;
            ImVec2 iconSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, icon.c_str());
            ImVec2 iconPos = ImVec2(
                imgMin.x + (thumb - iconSize.x) * 0.5f,
                imgMin.y + (thumb - iconSize.y) * 0.5f
            );

            std::string lowerExtCheck = asset.extension;
            std::transform(lowerExtCheck.begin(), lowerExtCheck.end(), lowerExtCheck.begin(), ::tolower);
            bool isDraggableAsset = (lowerExtCheck == ".obj" || lowerExtCheck == ".fbx" ||
                                    lowerExtCheck == ".dae" || lowerExtCheck == ".3ds" ||
                                    lowerExtCheck == ".mat" || lowerExtCheck == ".prefab");

            ImU32 iconColor = isDraggableAsset ? IM_COL32(100, 180, 255, 255) : IM_COL32(220, 220, 220, 255);
            dl->AddText(font, fontSize, iconPos, iconColor, icon.c_str());
        }


        // label below
        ImGui::SetCursorScreenPos(ImVec2(imgMin.x, imgMax.y));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumb);

        // Check if this asset is being renamed
        if (isRenaming && asset.guid.high == renamingAsset.high && asset.guid.low == renamingAsset.low) {
            // Show text input for renaming
            ImGui::SetNextItemWidth(thumb);
            if (ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer),
                               ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                ConfirmRename();
            }
            ImGui::SetKeyboardFocusHere(-1); // Focus the input
        } else {
            ImGui::TextWrapped("%s", asset.fileName.c_str());
        }

        if (!asset.isDirectory) {
            // Begin drag drop code for PREFABS
            std::string lowerExt = asset.extension;
            std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
            if (lowerExt == ".prefab" && ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    const std::string absPath = std::filesystem::absolute(asset.filePath).generic_string();
                    ImGui::SetDragDropPayload("PREFAB_PATH", absPath.c_str(),
                        static_cast<int>(absPath.size()) + 1);
                    ImGui::Text("Prefab: %s", asset.fileName.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }

        // Selection / activation - mouse release selects, but not during drag operations
        bool shouldSelect = false;
        if (released) {
            // Check if mouse moved significantly during the click-drag-release cycle
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
            float dragThreshold = 5.0f; // pixels
            if (dragDistance < dragThreshold) {
                shouldSelect = true;
            }
        }

        // Mark that an item was clicked (for preventing empty space deselection)
        if (clicked) {
            anyItemClickedInGrid = true;
        }

        // Select asset if shouldSelect is true (already accounts for drag distance)
        if (shouldSelect) {
            bool ctrl = io.KeyCtrl;
            SelectAsset(asset.guid, ctrl);
            // If user selects a prefab asset, don't leave it selected for Inspector
            if (!asset.isDirectory) {
                std::string le = asset.extension;
                std::transform(le.begin(), le.end(), le.begin(), ::tolower);
            }
        }

        bool selected = IsAssetSelected(asset.guid);
        if (selected) {
            dl->AddRectFilled(rectMin, rectMax, IM_COL32(100, 150, 255, 50));
            dl->AddRect(rectMin, rectMax, IM_COL32(100, 150, 255, 120), 4.0f, ImDrawFlags_RoundCornersAll, 2.0f);
        }
        else if (hovered) {
            dl->AddRect(rectMin, rectMax, IM_COL32(255, 255, 255, 30), 4.0f, ImDrawFlags_RoundCornersAll, 2.0f);
        }

        ImGui::PopID();
        // context menu
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            SelectAsset(asset.guid, false);
            ImGui::OpenPopup("AssetContextMenu");
        }

        ImGui::EndGroup();

        // wrap
        ++index;
        if ((index % cols) != 0) ImGui::SameLine(0.0f, pad);

        // double click
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (asset.isDirectory) {
                pendingNavigation = asset.filePath;
                //break;
            }
            else {
                ENGINE_PRINT("[AssetBrowserPanel] Opening asset: GUID(high=", asset.guid.high, ", low=", asset.guid.low, ")\n");
                std::filesystem::path p(asset.fileName);

                // Open scene confirmation dialogue.
                if (p.extension() == ".scene") {
                    OpenScene(asset);
                }
            }
        }

    }
    
    ShowOpenSceneConfirmation();
    if (!pendingNavigation.empty()) {
        NavigateToDirectory(pendingNavigation);
    }

    // Only clear selection if not dragging (to avoid interfering with drag operations)
    if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) && 
    !anyItemClickedInGrid && ImGui::IsWindowHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) 
    {
        selectedAssets.clear();
        lastSelectedAsset = GUID_128{ 0, 0 };
        // Clear the globally selected asset for the Inspector
        GUIManager::SetSelectedAsset(GUID_128{0, 0});
        CancelRename();
    }

    // Right-click context menu for empty space (create new assets)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !anyItemClickedInGrid && ImGui::IsWindowHovered()) {
        ImGui::OpenPopup("CreateAssetMenu");
    }

    // Context menu for selected assets
    if (ImGui::BeginPopup("AssetContextMenu")) {
        AssetInfo* contextAsset = nullptr;
        for (auto& a : currentAssets) {
            if (IsAssetSelected(a.guid)) { contextAsset = &a; break; }
        }
        if (contextAsset) ShowAssetContextMenu(*contextAsset);
        ImGui::EndPopup();
    }

    // Context menu for creating new assets
    if (ImGui::BeginPopup("CreateAssetMenu")) {
        ShowCreateAssetMenu();
        ImGui::EndPopup();
    }

    // ---------------- BACKGROUND DROP (scroll-safe, non-blocking) ----------------
    {
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        const ImRect visible = win->InnerRect; // absolute screen coords of the visible region

        const ImGuiPayload* active = ImGui::GetDragDropPayload();
        const bool entityDragActive = (active && active->IsDataType("HIERARCHY_ENTITY"));

        // Foreground visual (never occluded by items)
        if (entityDragActive)
        {
            ImDrawList* fdl = ImGui::GetForegroundDrawList(win->Viewport);
            fdl->AddRectFilled(visible.Min, visible.Max, IM_COL32(100, 150, 255, 25), 6.0f);
            fdl->AddRect(visible.Min, visible.Max, IM_COL32(100, 150, 255, 200), 6.0f, 0, 3.0f);
        }

        // The target itself; no widget created -> scrolling unaffected
        if (ImGui::BeginDragDropTargetCustom(visible, ImGui::GetID("##AssetGridBgDrop")))
        {
            if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY",
                    ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                if (payload->IsDelivery() && payload->DataSize == sizeof(Entity))
                {
                    const Entity dropped = *reinterpret_cast<const Entity*>(payload->Data);
                    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

                    std::string niceName;
                    if (ecs.HasComponent<NameComponent>(dropped))
                        niceName = ecs.GetComponent<NameComponent>(dropped).name;
                    if (niceName.empty())
                        niceName = "Entity_" + std::to_string(static_cast<uint64_t>(dropped));

                    std::filesystem::path dst =
                        std::filesystem::path(currentDirectory) / (niceName + ".prefab");
                    dst = MakeUniquePath(dst);

                    const std::string absDst = std::filesystem::absolute(dst).generic_string();
                    const bool ok = SaveEntityToPrefabFile(
                        ecs, AssetManager::GetInstance(), dropped, absDst);

                    if (ok) ENGINE_PRINT("[AssetBrowserPanel] Saved prefab: ", absDst, "\n");
                    else ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Failed to save: " , absDst, "\n");

                    RefreshAssets();
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    // -----------------------------------------------------------------------------

    ImGui::PopID();
}

void AssetBrowserPanel::RefreshAssets() {
    currentAssets.clear();

    try {
        if (!std::filesystem::exists(currentDirectory)) {
            currentDirectory = rootAssetDirectory;
        }

        for (const auto& entry : std::filesystem::directory_iterator(currentDirectory)) {
            // Use normalized generic_string() for map lookups and storage
            std::string filePath = entry.path().generic_string();
            bool isDirectory = entry.is_directory();

            GUID_128 guid{ 0, 0 };

            if (!isDirectory) {
                // Skip meta files
                if (entry.path().extension() == ".meta") {
                    continue;
                }

                // Check if it's a valid asset file
                std::string extension = entry.path().extension().string();
                if (!IsValidAssetFile(extension)) {
                    continue;
                }

                // Get or generate GUID using normalized filePath
                if (MetaFilesManager::MetaFileExists(filePath) && MetaFilesManager::MetaFileUpdated(filePath)) {
                    guid = MetaFilesManager::GetGUID128FromAssetFile(filePath);
                } else {
                    // Fallback: Generate a hash-based GUID if no meta file or meta file is outdated
                    std::hash<std::string> hasher;
                    size_t hash = hasher(filePath);
                    guid.high = static_cast<uint64_t>(hash);
                    guid.low = static_cast<uint64_t>(hash >> 32);
                    
                    // Store the mapping from GUID to file path for the Inspector to use
                    FallbackGuidToPath[guid.high] = filePath;
                }
            }
            else {
                // Don't show Shaders folder.
                if (entry.path().generic_string().find("Shaders") != std::string::npos) {
                    continue;
                }
                // For directories, generate a simple hash-based GUID using normalized path
                std::hash<std::string> hasher;
                size_t hash = hasher(filePath);
                guid.high = static_cast<uint64_t>(hash);
                guid.low = static_cast<uint64_t>(hash >> 32);
            }

            currentAssets.emplace_back(filePath, guid, isDirectory);
        }

        // Sort assets: directories first, then files
        std::sort(currentAssets.begin(), currentAssets.end(), [](const AssetInfo& a, const AssetInfo& b) {
            if (a.isDirectory != b.isDirectory) {
                return a.isDirectory > b.isDirectory;
            }
            return a.fileName < b.fileName;
            });

    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Error refreshing assets: ", e.what(), "\n");
    }

    UpdateBreadcrumbs();
}

void AssetBrowserPanel::NavigateToDirectory(const std::string& directory) {
    std::string normalizedPath = std::filesystem::path(directory).generic_string();

    if (std::filesystem::exists(normalizedPath) && std::filesystem::is_directory(normalizedPath)) {
        // Only refresh and update if directory actually changed
        if (currentDirectory != normalizedPath) {
            currentDirectory = normalizedPath;
            selectedAssets.clear();
            lastSelectedAsset = GUID_128{ 0, 0 };
            RefreshAssets(); // Only refresh when directory changes
            
            // Sync directory tree (Unity behavior)
            EnsureDirectoryExpanded(normalizedPath);
            needsTreeSync = true;
        }
    }

    pendingNavigation.clear();
}

void AssetBrowserPanel::UpdateBreadcrumbs() {
    pathBreadcrumbs.clear();

    std::filesystem::path relativePath = std::filesystem::relative(currentDirectory, rootAssetDirectory);

    if (relativePath != ".") {
        for (const auto& part : relativePath) {
            pathBreadcrumbs.push_back(part.string());
        }
    }
}

bool AssetBrowserPanel::PassesFilter(const AssetInfo& asset) const {
    // Search filter
    if (!searchQuery.empty()) {
        std::string lowerFileName = asset.fileName;
        std::string lowerSearch = searchQuery;
        std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

        if (lowerFileName.find(lowerSearch) == std::string::npos) {
            return false;
        }
    }

    // Type filter
    if (selectedAssetType != AssetType::All && !asset.isDirectory) {
        AssetType assetType = GetAssetTypeFromExtension(asset.extension);
        if (assetType != selectedAssetType) {
            return false;
        }
    }

    return true;
}

AssetBrowserPanel::AssetType AssetBrowserPanel::GetAssetTypeFromExtension(const std::string& extension) const {
    std::string lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);

    if (lowerExt == ".png" || lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".bmp" || lowerExt == ".tga") {
        return AssetType::Textures;
    }
    else if (lowerExt == ".obj" || lowerExt == ".fbx" || lowerExt == ".dae" || lowerExt == ".3ds") {
        return AssetType::Models;
    }
    else if (lowerExt == ".vert" || lowerExt == ".frag" || lowerExt == ".glsl" || lowerExt == ".hlsl") {
        return AssetType::Shaders;
    }
    else if (lowerExt == ".wav" || lowerExt == ".mp3" || lowerExt == ".ogg") {
        return AssetType::Audio;
    }
    else if (lowerExt == ".ttf" || lowerExt == ".otf") {
        return AssetType::Fonts;
    }
    else if (lowerExt == ".mat") {
        return AssetType::Materials;
    }

    return AssetType::All;
}

void AssetBrowserPanel::SelectAsset(const GUID_128& guid, bool multiSelect) {
    if (!multiSelect) selectedAssets.clear();

    if (selectedAssets.count(guid)) {
        selectedAssets.erase(guid);
        if (lastSelectedAsset.high == guid.high && lastSelectedAsset.low == guid.low) {
            GUIManager::SetSelectedAsset(GUID_128{ 0, 0 });
        }
        return;
    }

    selectedAssets.insert(guid);
    lastSelectedAsset = guid;

    // Determine if this guid corresponds to a prefab in the current grid
    bool isPrefab = false;
    for (const auto& a : currentAssets) {
        if (a.guid.high == guid.high && a.guid.low == guid.low) {
            std::string le = a.extension;
            std::transform(le.begin(), le.end(), le.begin(), ::tolower);
            isPrefab = (le == ".prefab");
            break;
        }
    }

    // Keep highlight either way; only non-prefabs wake the Inspector
    if (!isPrefab) {
        GUIManager::SetSelectedAsset(guid);
    }
    else {
        GUIManager::SetSelectedAsset(GUID_128{ 0, 0 });
    }
}

bool AssetBrowserPanel::IsAssetSelected(const GUID_128& guid) const {
    return selectedAssets.count(guid) > 0;
}

void AssetBrowserPanel::ShowAssetContextMenu(const AssetInfo& asset) {
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open")) {
        ENGINE_PRINT("[AssetBrowserPanel] Opening: ", asset.fileName, "\n");
    }
    if (ImGui::MenuItem(ICON_FA_FILE_PEN " Rename")) {
        StartRenameAsset(lastSelectedAsset);
        ENGINE_PRINT("[AssetBrowserPanel] Renaming: ", asset.fileName, "\n");
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_EYE " Reveal in Explorer")) {
        RevealInExplorer(asset);
    }

    if (ImGui::MenuItem(ICON_FA_CLIPBOARD " Copy Path")) {
        CopyAssetPath(asset);
    }

    ImGui::Separator();


    // Create submenu with delete option
    if (ImGui::BeginMenu(ICON_FA_PLUS " Create")) {
        if (ImGui::MenuItem(ICON_FA_PAINTBRUSH " Material")) {
            CreateNewMaterial();
        }

        if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " Folder")) {
            CreateNewFolder();
        }

        if (ImGui::MenuItem(ICON_FA_XMARK " Delete")) {
            DeleteAsset(asset);
        }

        ImGui::EndMenu();
    }
}

void AssetBrowserPanel::ShowCreateAssetMenu() {
    if (ImGui::BeginMenu(ICON_FA_PLUS " Create")) {
        if (ImGui::MenuItem(ICON_FA_PAINTBRUSH " Material")) {
            CreateNewMaterial();
        }

        if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " Folder")) {
            CreateNewFolder();
        }

        if (ImGui::MenuItem(ICON_FA_GLOBE " Scene")) {
            CreateNewScene(currentDirectory);
        }

        ImGui::EndMenu();
    }
}

void AssetBrowserPanel::HandleDragAndDrop(const AssetInfo& asset) {
    // Set drag and drop payload with GUID
    ImGui::SetDragDropPayload("ASSET_GUID", &asset.guid, sizeof(GUID_128));

    // Show preview
    ImGui::Text("Dragging: %s", asset.fileName.c_str());
}

void AssetBrowserPanel::DeleteAsset(const AssetInfo& asset) {
    // Store the asset to delete and show confirmation popup
    assetToDelete = asset;
    showDeleteConfirmation = true;
}

void AssetBrowserPanel::ConfirmDeleteAsset() {
    try {
        if (assetToDelete.isDirectory) {
            std::filesystem::remove_all(assetToDelete.filePath);
            ENGINE_PRINT("[AssetBrowserPanel] Deleted directory: ", assetToDelete.filePath, "\n");
        }
        else {
            std::filesystem::remove(assetToDelete.filePath);
            ENGINE_PRINT("[AssetBrowserPanel] Deleted file: ", assetToDelete.filePath, "\n");

            // Also remove meta file
            std::string metaFile = assetToDelete.filePath + ".meta";
            if (std::filesystem::exists(metaFile)) {
                std::filesystem::remove(metaFile);
            }
            
            // Remove from thumbnail cache if it was a texture
            RemoveThumbnailFromCache(assetToDelete.guid);
        }

        // Remove from selection if it was selected
        selectedAssets.erase(assetToDelete.guid);
        if (lastSelectedAsset.high == assetToDelete.guid.high && lastSelectedAsset.low == assetToDelete.guid.low) {
            GUIManager::SetSelectedAsset(GUID_128{0, 0});
            lastSelectedAsset = GUID_128{0, 0};
        }

        // Refresh the asset list
        RefreshAssets();
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Failed to delete asset: ", e.what(), "\n");
    }
}

void AssetBrowserPanel::RevealInExplorer(const AssetInfo& asset) {
#ifdef _WIN32
    std::filesystem::path fullPath = std::filesystem::absolute(asset.filePath);
    std::wstring path = fullPath.wstring(); // <-- not generic_wstring
    std::wstring param = L"/select,\"" + path + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);

#else
    ENGINE_PRINT("[AssetBrowserPanel] Reveal in explorer not implemented for this platform", "\n");
#endif
}

void AssetBrowserPanel::CopyAssetPath(const AssetInfo& asset) {
    std::string relativePath = GetRelativePath(asset.filePath);

#ifdef _WIN32
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, relativePath.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), relativePath.c_str(), relativePath.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
            ENGINE_PRINT("[AssetBrowserPanel] Copy to clipboard: ", relativePath, "\n");
        }
        CloseClipboard();
    }
#else
    ENGINE_PRINT("[AssetBrowserPanel] Copy to clipboard: ", relativePath, "\n");
#endif
}

void AssetBrowserPanel::RenameAsset(const AssetInfo& asset, const std::string& newName) {
    std::filesystem::path oldPath = asset.filePath;
    std::filesystem::path newPath = oldPath.parent_path() / newName;
    std::error_code ec;
    std::filesystem::rename(oldPath, newPath, ec);
    if (!ec) {
        RefreshAssets();
    }
    else {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Rename failed: ", ec.message(), "\n");
    }
}


void AssetBrowserPanel::CreateNewScene(const std::string& directory) {
    std::string newSceneName = "New Scene.scene";
    std::filesystem::path directoryPath(directory);
    std::filesystem::path newSceneNamePath(newSceneName);
    std::filesystem::path newScenePathFull = (directoryPath / newSceneName);
    std::string stem = newSceneNamePath.stem().generic_string();
    std::string extension = newSceneNamePath.extension().generic_string();

    int counter = 1;
    while (std::filesystem::exists(newScenePathFull)) {
        newScenePathFull = (directoryPath / (stem + std::to_string(counter++) + extension));
    }

    std::ofstream file(newScenePathFull.generic_string());
    file.close();

    RefreshAssets();
}

void AssetBrowserPanel::OpenScene(const AssetInfo& _selectedScene) {
    isOpeningScene = true;
    selectedScene = _selectedScene;
    ImGui::OpenPopup("Open Scene?");
}

void AssetBrowserPanel::ShowOpenSceneConfirmation() {
    if (ImGui::BeginPopupModal("Open Scene?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Center it on the main viewport
        ImGui::SetWindowPos(
            ImVec2(ImGui::GetMainViewport()->GetCenter().x,
                ImGui::GetMainViewport()->GetCenter().y),
            ImGuiCond_Appearing); // only when it first appears

        std::string text = "Do you want to open " + selectedScene.fileName + "?\nUnsaved changes will be lost.";
        ImGui::Text(text.c_str());
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            SceneManager::GetInstance().LoadScene(selectedScene.filePath);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            // Cancel
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

std::string AssetBrowserPanel::GetRelativePath(const std::string& fullPath) const {
    try {
        std::filesystem::path relative = std::filesystem::relative(fullPath, rootAssetDirectory);
        return relative.generic_string();
    }
    catch (const std::exception&) {
        return fullPath;
    }
}

bool AssetBrowserPanel::IsValidAssetFile(const std::string& extension) const {
    std::string lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);

    static const std::unordered_set<std::string> VALID_EXTENSIONS = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga",           // Textures
        ".obj", ".fbx", ".dae", ".3ds",                    // Models
        ".vert", ".frag", ".glsl", ".hlsl",                // Shaders
        ".wav", ".mp3", ".ogg",                            // Audio
        ".ttf", ".otf",                                    // Fonts
        ".mat",                                            // Materials
        ".prefab",                                         // Prefabs
        ".scene"                                           // Scenes
    };

    return VALID_EXTENSIONS.count(lowerExt) > 0;
}

void AssetBrowserPanel::EnsureDirectoryExists(const std::string& directory) {
    try {
        if (!std::filesystem::exists(directory)) {
            std::filesystem::create_directories(directory);
        }
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Failed to create directory ", directory, ": ", e.what(), "\n");
    }
}

std::string AssetBrowserPanel::GetFallbackGuidFilePath(const GUID_128& guid) {
    auto it = FallbackGuidToPath.find(guid.high);
    if (it != FallbackGuidToPath.end()) {
        return it->second;
    }
    return ""; // Return empty string if not found
}

void AssetBrowserPanel::CreateNewMaterial() {
    // Generate a unique name for the new material
    std::string baseName = "NewMaterial";
    std::string materialName = baseName;
    int counter = 1;

    // Find a unique name
    std::string materialPath;
    do {
        materialPath = currentDirectory + "/" + materialName + ".mat";
        if (!std::filesystem::exists(materialPath)) {
            break;
        }
        materialName = baseName + std::to_string(counter++);
    } while (counter < 1000); // Safety limit

    try {
        // Create a new default material
        auto material = Material::CreateDefault();
        material->SetName(materialName);

        // Write the material file directly (not using the asset system for creation)
        std::string compiledPath = material->CompileToResource(materialPath);

        if (!compiledPath.empty()) {
            ENGINE_PRINT("[AssetBrowserPanel] Created new material: ", materialPath, "\n");\

            // Compile the asset through the AssetManager to create proper meta files
            AssetManager::GetInstance().CompileAsset<Material>(materialPath, true);

            // Refresh the asset browser to show the new material
            QueueRefresh();
        } else {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Failed to create material file: ", materialPath, "\n");
        }
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Error creating material: ", e.what(), "\n");
    }
}

void AssetBrowserPanel::CreateNewFolder() {
    // Generate a unique name for the new folder
    std::string baseName = "NewFolder";
    std::string folderName = baseName;
    int counter = 1;

    // Find a unique name
    std::string folderPath;
    do {
        folderPath = currentDirectory + "/" + folderName;
        if (!std::filesystem::exists(folderPath)) {
            break;
        }
        folderName = baseName + std::to_string(counter++);
    } while (counter < 1000); // Safety limit

    try {
        std::filesystem::create_directory(folderPath);
        ENGINE_PRINT("[AssetBrowserPanel] Created new folder: ", folderPath, "\n");

        // Refresh the asset browser to show the new folder
        QueueRefresh();
    }
    catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Error creating folder: ", e.what(), "\n");
    }
}

void AssetBrowserPanel::StartRenameAsset(const GUID_128& guid) {
    // Find the asset being renamed
    for (const auto& asset : currentAssets) {
        if (asset.guid.high == guid.high && asset.guid.low == guid.low) {
            isRenaming = true;
            renamingAsset = guid;

            // Set up the rename buffer with the current filename (without extension for files, full name for folders)
            if (asset.isDirectory) {
                strncpy_s(renameBuffer, asset.fileName.c_str(), sizeof(renameBuffer) - 1);
            } else {
                // For files, keep the extension but allow renaming the base name
                std::filesystem::path path(asset.fileName);
                std::string baseName = path.stem().string();
                strncpy_s(renameBuffer, baseName.c_str(), sizeof(renameBuffer) - 1);
            }
            break;
        }
    }
}

void AssetBrowserPanel::CancelRename() {
    isRenaming = false;
    renamingAsset = GUID_128{ 0, 0 };
    memset(renameBuffer, 0, sizeof(renameBuffer));
}

void AssetBrowserPanel::ConfirmRename() {
    if (!isRenaming || strlen(renameBuffer) == 0) {
        CancelRename();
        return;
    }

    // Find the asset being renamed
    for (const auto& asset : currentAssets) {
        if (asset.guid.high == renamingAsset.high && asset.guid.low == renamingAsset.low) {
            try {
                std::filesystem::path oldPath(asset.filePath);
                std::filesystem::path newPath;

                if (asset.isDirectory) {
                    // For directories, rename the entire folder
                    newPath = oldPath.parent_path() / renameBuffer;
                } else {
                    // For files, keep the extension but change the base name
                    std::filesystem::path extension = oldPath.extension();
                    newPath = oldPath.parent_path() / (std::string(renameBuffer) + extension.string());
                }

                // Perform the rename
                if (oldPath != newPath && std::filesystem::exists(oldPath)) {
                    std::filesystem::rename(oldPath, newPath);

                    // Also rename the .meta file if it exists
                    std::filesystem::path oldMetaPath = oldPath;
                    oldMetaPath += ".meta";
                    if (std::filesystem::exists(oldMetaPath)) {
                        std::filesystem::path newMetaPath = newPath;
                        newMetaPath += ".meta";
                        std::filesystem::rename(oldMetaPath, newMetaPath);
                    }
                    ENGINE_PRINT("[AssetBrowserPanel] Renamed: ", oldPath, " -> ", newPath, "\n");
                }
            }
            catch (const std::exception& e) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetBrowserPanel] Error renaming asset: ", e.what(), "\n");
            }

            break;
        }
    }

    CancelRename();
    QueueRefresh();
}

std::string AssetBrowserPanel::GetAssetIcon(const AssetInfo& asset) const {
    if (asset.isDirectory) {
        return ICON_FA_FOLDER;
    }

    std::string lowerExt = asset.extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);

    if (lowerExt == ".png" || lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".bmp" || lowerExt == ".tga" || lowerExt == ".dds") {
        return ICON_FA_IMAGE;
    }
    else if (lowerExt == ".obj" || lowerExt == ".fbx" || lowerExt == ".dae" || lowerExt == ".3ds") {
        return ICON_FA_CUBE;
    }
    else if (lowerExt == ".vert" || lowerExt == ".frag" || lowerExt == ".glsl" || lowerExt == ".hlsl") {
        return ICON_FA_CODE;
    }
    else if (lowerExt == ".wav" || lowerExt == ".mp3" || lowerExt == ".ogg") {
        return ICON_FA_VOLUME_HIGH;
    }
    else if (lowerExt == ".ttf" || lowerExt == ".otf") {
        return ICON_FA_FONT;
    }
    else if (lowerExt == ".mat") {
        return ICON_FA_CIRCLE_HALF_STROKE;
    }
    else if (lowerExt == ".prefab") {
        return ICON_FA_CUBES;
    }
    else if (lowerExt == ".scene") {
        return ICON_FA_EARTH_AMERICAS;
    }

    return ICON_FA_FILE; // Default file icon
}

// ============================================================================
// Thumbnail Management (Unity-like)
// ============================================================================

uint32_t AssetBrowserPanel::GetOrCreateThumbnail(const GUID_128& guid, const std::string& assetPath) {
    // Create cache key from GUID (use high 64 bits as key for simplicity)
    uint64_t cacheKey = guid.high ^ guid.low;
    
    // Check if thumbnail already exists in cache
    //auto it = thumbnailCache.find(cacheKey);
    //if (it != thumbnailCache.end()) {
    //    return it->second;
    //}

    // Load texture using AssetManager and ResourceManager (GUID-based)
    std::shared_ptr<Texture> texture = nullptr;
    
    // Get the texture from ResourceManager.
    texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(guid, assetPath);

    if (texture && texture->ID != 0) {
        // Cache the texture ID for future use
        thumbnailCache[cacheKey] = static_cast<uint32_t>(texture->ID);
        return static_cast<uint32_t>(texture->ID);
    }

    return 0; // Return 0 if failed to load
}

void AssetBrowserPanel::ClearThumbnailCache() {
    // Note: We don't delete the OpenGL textures here because they're managed
    // by the ResourceManager. We just clear our GUID->ID mapping.
    thumbnailCache.clear();
}

void AssetBrowserPanel::RemoveThumbnailFromCache(const GUID_128& guid) {
    // Create cache key from GUID
    uint64_t cacheKey = guid.high ^ guid.low;
    
    auto it = thumbnailCache.find(cacheKey);
    if (it != thumbnailCache.end()) {
        thumbnailCache.erase(it);
    }
}

// ============================================================================
// Directory Tree Synchronization
// ============================================================================

void AssetBrowserPanel::EnsureDirectoryExpanded(const std::string& directoryPath) {
    // Expand all parent directories up to and including the target directory
    std::filesystem::path targetPath(directoryPath);
    std::filesystem::path rootPath(rootAssetDirectory);
    
    // Build list of all parent directories from root to target
    std::vector<std::string> pathsToExpand;
    std::filesystem::path currentPath = targetPath;
    
    while (currentPath != rootPath && currentPath.has_parent_path()) {
        pathsToExpand.push_back(currentPath.generic_string());
        currentPath = currentPath.parent_path();
    }
    
    // Add root path
    pathsToExpand.push_back(rootPath.generic_string());
    
    // Reverse to go from root to target
    std::reverse(pathsToExpand.begin(), pathsToExpand.end());
    
    // Add all paths to expanded directories set
    // This will be applied on next frame with ImGuiCond_Once
    for (const auto& path : pathsToExpand) {
        expandedDirectories.insert(path);
    }
}

void AssetBrowserPanel::SyncTreeWithCurrentDirectory() {
    // This ensures the current directory's parents are expanded
    EnsureDirectoryExpanded(currentDirectory);
    needsTreeSync = false;
}