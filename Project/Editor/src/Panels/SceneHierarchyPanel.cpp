#include "Panels/SceneHierarchyPanel.hpp"
#include <imgui.h>
#include "pch.h"
#include "GUIManager.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include "EditorComponents.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
#include "ECS/SiblingIndexComponent.hpp"
#include <Hierarchy/ChildrenComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <PrefabIO.hpp>
#include <imgui_internal.h>
#include "Scene/SceneManager.hpp"
#include <Transform/TransformComponent.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Sprite/SpriteRenderComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Sound/AudioComponent.hpp>
#include <Graphics/Camera/CameraComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include "EditorState.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <Utilities/GUID.hpp>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include "Panels/ScenePanel.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "SnapshotManager.hpp"
#include "UndoableWidgets.hpp"
#include <algorithm>

// Entity clipboard for copy/paste functionality
// Uses GUIDs instead of Entity IDs so clipboard persists across undo/redo
namespace {
    struct EntityClipboard {
        std::vector<GUID_128> copiedEntityGUIDs;  // GUIDs of copied entities (persist across undo/redo)
        bool hasData = false;

        void Clear() {
            copiedEntityGUIDs.clear();
            hasData = false;
        }
    };

    static EntityClipboard g_EntityClipboard;
}

SceneHierarchyPanel::SceneHierarchyPanel()
    : EditorPanel("Scene Hierarchy", true) {
}

void SceneHierarchyPanel::MarkForRefresh() {
    needsRefresh = true;
}

void SceneHierarchyPanel::OnImGuiRender() {
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_HIERARCHY);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_HIERARCHY);

    if (ImGui::Begin(name.c_str(), &isOpen)) {
        // Handle F2 key for renaming selected entity
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2)) {
            Entity selectedEntity = GUIManager::GetSelectedEntity();
            if (selectedEntity != static_cast<Entity>(-1)) {
                renamingEntity = selectedEntity;
                startRenaming = true;
            }
        }

        // Note: Ctrl+C, Ctrl+V, Ctrl+D, and Delete are now handled globally in GUIManager::HandleKeyboardShortcuts()
        // This allows them to work regardless of which panel has focus

        // Handle 'F' key to focus on selected entity (Frame Selected - like Unity)
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F)) {
            Entity selectedEntity = GUIManager::GetSelectedEntity();
            if (selectedEntity != static_cast<Entity>(-1)) {
                try {
                    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                    if (ecsManager.HasComponent<Transform>(selectedEntity)) {
                        Transform& transform = ecsManager.GetComponent<Transform>(selectedEntity);
                        glm::vec3 entityPos(transform.worldMatrix.m.m03,
                                           transform.worldMatrix.m.m13,
                                           transform.worldMatrix.m.m23);

                        // Determine if entity is 2D or 3D
                        bool entityIs3D = true;
                        bool hasSprite = ecsManager.HasComponent<SpriteRenderComponent>(selectedEntity);
                        bool hasText = ecsManager.HasComponent<TextRenderComponent>(selectedEntity);
                        bool hasModel = ecsManager.HasComponent<ModelRenderComponent>(selectedEntity);

                        if (hasModel) {
                            entityIs3D = true;
                        } else if (hasSprite) {
                            auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(selectedEntity);
                            entityIs3D = sprite.is3D;
                            // Always use transform.worldMatrix for position - sprite.position is not kept updated
                        } else if (hasText) {
                            auto& text = ecsManager.GetComponent<TextRenderComponent>(selectedEntity);
                            entityIs3D = text.is3D;
                        }

                        // Switch view mode to match entity if needed
                        EditorState& editorState = EditorState::GetInstance();
                        bool currentIs2D = editorState.Is2DMode();
                        bool targetIs2D = !entityIs3D;

                        if (currentIs2D != targetIs2D) {
                            EditorState::ViewMode newViewMode = entityIs3D ? EditorState::ViewMode::VIEW_3D : EditorState::ViewMode::VIEW_2D;
                            editorState.SetViewMode(newViewMode);
                            GraphicsManager::ViewMode gfxMode = entityIs3D ? GraphicsManager::ViewMode::VIEW_3D : GraphicsManager::ViewMode::VIEW_2D;
                            GraphicsManager::GetInstance().SetViewMode(gfxMode);
                        }

                        // Frame the entity in the scene camera
                        auto scenePanelPtr = GUIManager::GetPanelManager().GetPanel("Scene");
                        if (scenePanelPtr) {
                            auto scenePanel = std::dynamic_pointer_cast<ScenePanel>(scenePanelPtr);
                            if (scenePanel) {
                                scenePanel->SetCameraTarget(entityPos);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[SceneHierarchy] Failed to focus entity: " << e.what() << std::endl;
                }
            }
        }


        std::string sceneName = SceneManager::GetInstance().GetSceneName();
        std::string sceneDisplayName = std::string(ICON_FA_EARTH_AMERICAS) + " " + sceneName;

        // Add visual separation: MUCH darker background for scene header (like Unity)
        ImGui::PushStyleColor(ImGuiCol_Header, EditorComponents::PANEL_BG_SCENE_HEADER);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorComponents::PANEL_BG_SCENE_HEADER);

        ImGuiTreeNodeFlags sceneFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
        bool sceneExpanded = ImGui::TreeNodeEx("##SceneRoot", sceneFlags, "%s", sceneDisplayName.c_str());

        ImGui::PopStyleColor(3);

        // Add small spacing after scene header for visual clarity
        ImGui::Spacing();

        // Handle auto-expand when entity is selected from ScenePanel
        Entity currentSelected = GUIManager::GetSelectedEntity();
        if (currentSelected != static_cast<Entity>(-1) && currentSelected != lastSelectedEntity) {
            ExpandToEntity(currentSelected);
            lastSelectedEntity = currentSelected;
        } else if (currentSelected == static_cast<Entity>(-1)) {
            lastSelectedEntity = static_cast<Entity>(-1);
        }

        if (sceneExpanded) {
            try {
                // Get the active ECS manager
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

                // Get sorted root entities (by sibling index)
                std::vector<Entity> rootEntities = GetSortedRootEntities();

                // Draw entity nodes starting from root entities, in a depth-first manner.
                for (const auto& entity : rootEntities) {
                    // Check if entity has NameComponent before accessing it
                    if (!ecsManager.TryGetComponent<NameComponent>(entity).has_value()) {
                        continue;
                    }
                    std::string entityName = ecsManager.GetComponent<NameComponent>(entity).name;

                    // Skip PREVIEW entities (used for drag-and-drop preview)
                    if (entityName == "PREVIEW") {
                        continue;
                    }

                    DrawEntityNode(entityName, entity, ecsManager.TryGetComponent<ChildrenComponent>(entity).has_value());
                }

                // Draw drop zone AFTER the last entity (to allow dropping at the end of the list)
                const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
                bool isDraggingEntity = activePayload && activePayload->IsDataType("HIERARCHY_ENTITY");
                
                if (isDraggingEntity && !rootEntities.empty()) {
                    Entity lastEntity = rootEntities.back();
                    float dropZoneHeight = 4.0f;
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    float contentWidth = ImGui::GetContentRegionAvail().x;
                    
                    ImGui::InvisibleButton("##dropAtEnd", ImVec2(contentWidth, dropZoneHeight + 8.0f));
                    
                    if (ImGui::BeginDragDropTarget()) {
                        // Visual indicator - draw a line
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        ImVec2 lineStart = ImVec2(cursorPos.x, cursorPos.y);
                        ImVec2 lineEnd = ImVec2(cursorPos.x + contentWidth, cursorPos.y);
                        drawList->AddLine(lineStart, lineEnd, IM_COL32(100, 150, 255, 255), 2.0f);
                        
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                            Entity dragged = *(Entity*)payload->Data;
                            ReorderEntity(dragged, lastEntity, true); // Insert after last entity
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            } catch (const std::exception& e) {
                ImGui::Text("Error accessing ECS: %s", e.what());
            }

            ImGui::TreePop();
        }

        //ImGui::Separator();

        // Context menu for creating new objects
        if (ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Create Empty")) {
                Entity newEntity = CreateEmptyEntity();
                GUIManager::SetSelectedEntity(newEntity);
            }
            if (ImGui::MenuItem("Create Cube")) {
                Entity newEntity = CreateCubeEntity();
                GUIManager::SetSelectedEntity(newEntity);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Create Camera")) {
                Entity newEntity = CreateCameraEntity();
                GUIManager::SetSelectedEntity(newEntity);
            }
            ImGui::EndPopup();
        }

        // Handle unparenting of an entity
        // Take up all remaining space in the window
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.y > 0) {
            ImGui::InvisibleButton("HierarchyBackground", avail);

            ImGuiWindow* win = ImGui::GetCurrentWindow();
            const ImRect visible = win->InnerRect; // absolute screen coords of the visible region

            const ImGuiPayload* active = ImGui::GetDragDropPayload();
            const bool entityDragActive = (active && (active->IsDataType("HIERARCHY_ENTITY") || (active->IsDataType("PREFAB_PATH"))));

            // Foreground visual (never occluded by items)
            if (entityDragActive)
            {
                ImDrawList* fdl = ImGui::GetForegroundDrawList(win->Viewport);
                fdl->AddRectFilled(visible.Min, visible.Max, IM_COL32(100, 150, 255, 25), 6.0f);
                fdl->AddRect(visible.Min, visible.Max, IM_COL32(100, 150, 255, 200), 6.0f, 0, 3.0f);
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                    Entity dragged = *(Entity*)payload->Data;
                    UnparentEntity(dragged);
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_PATH")) {
                    const char* prefabPath = static_cast<const char*>(payload->Data);
                    const bool ok = InstantiatePrefabFromFile(prefabPath);
                    if (!ok) {
                        std::cerr << "[ScenePanel] Failed to instantiate prefab: " << prefabPath << "\n";
                    }
                    else {
                        std::cout << "[ScenePanel] Instantiated prefab: " << prefabPath << std::endl;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

    }
    ImGui::End();

    ImGui::PopStyleColor(2);  // Pop WindowBg and ChildBg colors
}


void SceneHierarchyPanel::DrawEntityNode(const std::string& entityName, Entity entityId, bool hasChildren)
{
    if (!renamingEntity)
        assert(!entityName.empty() && "Entity name cannot be empty");

    // Check if this entity should be auto-expanded (has a selected descendant)
    bool forceOpen = expandedEntities.count(entityId) > 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (GUIManager::IsEntitySelected(entityId)) flags |= ImGuiTreeNodeFlags_Selected;
    if (forceOpen && hasChildren) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    // Get item position for drop zone detection
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    float itemHeight = ImGui::GetTextLineHeightWithSpacing();
    float dropZoneHeight = 4.0f; // Height of the reorder drop zone

    // Check for active drag payload
    const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
    bool isDraggingEntity = activePayload && activePayload->IsDataType("HIERARCHY_ENTITY");

    // Draw reorder drop zone BEFORE this item (insert above)
    if (isDraggingEntity) {
        ImVec2 dropMin = ImVec2(cursorPos.x, cursorPos.y - dropZoneHeight * 0.5f);
        ImVec2 dropMax = ImVec2(cursorPos.x + ImGui::GetContentRegionAvail().x, cursorPos.y + dropZoneHeight * 0.5f);
        
        ImGui::SetCursorScreenPos(dropMin);
        ImGui::InvisibleButton(("##dropAbove" + std::to_string(entityId)).c_str(), ImVec2(dropMax.x - dropMin.x, dropZoneHeight));
        
        if (ImGui::BeginDragDropTarget()) {
            // Visual indicator - draw a line
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 lineStart = ImVec2(dropMin.x, cursorPos.y);
            ImVec2 lineEnd = ImVec2(dropMax.x, cursorPos.y);
            drawList->AddLine(lineStart, lineEnd, IM_COL32(100, 150, 255, 255), 2.0f);
            
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                Entity dragged = *(Entity*)payload->Data;
                ReorderEntity(dragged, entityId, false); // Insert before
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::SetCursorScreenPos(cursorPos);
    }

    bool opened = false;

    if (renamingEntity == entityId)
    {
        ImGui::SetNextItemWidth(-1.0f);

        if (startRenaming) {
            renameBuffer.assign(256, '\0');
            if (!entityName.empty() && entityName.length() < 255)
                std::copy(entityName.begin(), entityName.end(), renameBuffer.begin());
            startRenaming = false;
            ImGui::SetKeyboardFocusHere();
        }

        if (UndoableWidgets::InputText(("##rename" + std::to_string(entityId)).c_str(),
                             renameBuffer.data(), renameBuffer.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                if (ecsManager.HasComponent<NameComponent>(entityId)) {
                    auto& nameComp = ecsManager.GetComponent<NameComponent>(entityId);
                    nameComp.name = std::string(renameBuffer.data());
                }
            } catch (...) {}
            renamingEntity = static_cast<Entity>(-1);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            renamingEntity = static_cast<Entity>(-1);
        }
    }
    else
    {
        // Check if entity is inactive (grayed out like Unity)
        bool isEntityActive = true;
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
            if (ecsManager.HasComponent<ActiveComponent>(entityId)) {
                auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entityId);
                isEntityActive = activeComp.isActive;
            }
        } catch (...) {}

        // Apply gray color if inactive
        if (!isEntityActive) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Gray text
        }

        std::string displayName = std::string(ICON_FA_CUBE) + " " + entityName;
        
        // Set open state if force expanding
        if (forceOpen && hasChildren) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        
        opened = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", displayName.c_str());

        // Pop color if we pushed it
        if (!isEntityActive) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemClicked()) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift && lastClickedEntity != static_cast<Entity>(-1)) {
                // Shift+Click: range selection
                SelectRange(lastClickedEntity, entityId);
            } else if (io.KeyCtrl) {
                // Ctrl+Click: toggle selection
                if (GUIManager::IsEntitySelected(entityId)) {
                    GUIManager::RemoveSelectedEntity(entityId);
                } else {
                    GUIManager::AddSelectedEntity(entityId);
                }
                lastClickedEntity = entityId;
            } else {
                // Single click: select only this entity
                GUIManager::SetSelectedEntity(entityId);
                lastClickedEntity = entityId;
            }

            // Double-click to focus the entity in the scene view
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                try {
                    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                    if (ecsManager.HasComponent<Transform>(entityId)) {
                        Transform& transform = ecsManager.GetComponent<Transform>(entityId);
                        glm::vec3 entityPos(transform.worldMatrix.m.m03,
                                          transform.worldMatrix.m.m13,
                                          transform.worldMatrix.m.m23);

                        std::cout << "[SceneHierarchy] Double-clicked entity '" << entityName
                                 << "' at world position (" << entityPos.x << ", " << entityPos.y << ", " << entityPos.z << ")" << std::endl;

                        // Determine if entity is 2D or 3D
                        bool entityIs3D = true; // Default to 3D
                        bool hasSprite = ecsManager.HasComponent<SpriteRenderComponent>(entityId);
                        bool hasText = ecsManager.HasComponent<TextRenderComponent>(entityId);
                        bool hasModel = ecsManager.HasComponent<ModelRenderComponent>(entityId);

                        if (hasModel) {
                            entityIs3D = true;
                        } else if (hasSprite) {
                            auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entityId);
                            entityIs3D = sprite.is3D;
                            // Always use transform.worldMatrix for position - sprite.position is not kept updated
                            std::cout << "[SceneHierarchy] Entity has sprite, is3D=" << sprite.is3D << std::endl;
                        } else if (hasText) {
                            auto& text = ecsManager.GetComponent<TextRenderComponent>(entityId);
                            entityIs3D = text.is3D;
                            std::cout << "[SceneHierarchy] Entity has text component is3D=" << text.is3D << std::endl;
                        }

                        // Switch view mode to match entity
                        EditorState& editorState = EditorState::GetInstance();
                        bool currentIs2D = editorState.Is2DMode();
                        bool targetIs2D = !entityIs3D;

                        if (currentIs2D != targetIs2D) {
                            // Need to switch modes
                            EditorState::ViewMode newViewMode = entityIs3D ? EditorState::ViewMode::VIEW_3D : EditorState::ViewMode::VIEW_2D;
                            editorState.SetViewMode(newViewMode);

                            // Sync with GraphicsManager
                            GraphicsManager::ViewMode gfxMode = entityIs3D ? GraphicsManager::ViewMode::VIEW_3D : GraphicsManager::ViewMode::VIEW_2D;
                            GraphicsManager::GetInstance().SetViewMode(gfxMode);

                            std::cout << "[SceneHierarchy] Switched view mode to " << (entityIs3D ? "3D" : "2D") << std::endl;
                        }

                        // Frame the entity in the scene camera
                        auto scenePanelPtr = GUIManager::GetPanelManager().GetPanel("Scene");
                        if (scenePanelPtr) {
                            auto scenePanel = std::dynamic_pointer_cast<ScenePanel>(scenePanelPtr);
                            if (scenePanel) {
                                scenePanel->SetCameraTarget(entityPos);
                                std::cout << "[SceneHierarchy] Set camera target to ("
                                         << entityPos.x << ", " << entityPos.y << ", " << entityPos.z << ")" << std::endl;
                            } else {
                                std::cout << "[SceneHierarchy] Failed to cast to ScenePanel" << std::endl;
                            }
                        } else {
                            std::cout << "[SceneHierarchy] Scene panel not found" << std::endl;
                        }
                    } else {
                        std::cout << "[SceneHierarchy] Entity '" << entityName << "' has no Transform component" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[SceneHierarchy] Error focusing entity: " << e.what() << std::endl;
                }
            }
        }
    }

    // --- DRAG SOURCE from a hierarchy row (exactly one payload) ---
    {
        // Start a drag from this row?
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                Entity payload = entityId; // ImGui copies this buffer
                ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &payload, sizeof(Entity));
                ImGui::Text("Move %s", entityName.c_str());
                ImGui::Separator();
                ImGui::Text("%s", entityName.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }
    // -----------------------------------------------------------------

    // Drop target on the item itself (for reparenting - making it a child)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity dragged = *(Entity*)payload->Data;
            ReparentEntity(dragged, entityId);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem())
    {
        const std::vector<Entity>& selectedEntities = GUIManager::GetSelectedEntities();
        bool isMultiSelect = selectedEntities.size() > 1;

        // Copy menu item
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            if (isMultiSelect) {
                CopySelectedEntities();
            } else {
                // Store GUID instead of Entity ID for persistence
                EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();
                GUID_128 guid = guidRegistry.GetGUIDByEntity(entityId);
                g_EntityClipboard.copiedEntityGUIDs.clear();
                if (guid.high != 0 || guid.low != 0) {
                    g_EntityClipboard.copiedEntityGUIDs.push_back(guid);
                    g_EntityClipboard.hasData = true;
                }
            }
        }

        // Paste menu item
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, g_EntityClipboard.hasData)) {
            PasteEntities();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            if (isMultiSelect) {
                std::vector<Entity> duplicated = DuplicateEntities(selectedEntities);
                if (!duplicated.empty()) {
                    GUIManager::SetSelectedEntities(duplicated);
                }
            } else {
                Entity duplicatedEntity = DuplicateEntity(entityId);
                if (duplicatedEntity != static_cast<Entity>(-1)) {
                    GUIManager::SetSelectedEntity(duplicatedEntity);
                }
            }
        }
        if (ImGui::MenuItem("Rename", "F2", false, !isMultiSelect)) {
            renamingEntity = entityId;
            startRenaming  = true;
        }
        if (ImGui::MenuItem("Delete", "Del")) {
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

                if (isMultiSelect) {
                    std::string snapshotDesc = "Delete " + std::to_string(selectedEntities.size()) + " Entities";
                    SnapshotManager::GetInstance().TakeSnapshot(snapshotDesc);

                    std::vector<Entity> entitiesToDelete = selectedEntities;
                    GUIManager::ClearSelectedEntities();

                    for (Entity entity : entitiesToDelete) {
                        if (ecsManager.TryGetComponent<NameComponent>(entity).has_value()) {
                            ecsManager.DestroyEntity(entity);
                        }
                    }
                } else {
                    std::string entityName = ecsManager.GetComponent<NameComponent>(entityId).name;
                    SnapshotManager::GetInstance().TakeSnapshot("Delete Entity: " + entityName);
                    GUIManager::SetSelectedEntity(static_cast<Entity>(-1));
                    ecsManager.DestroyEntity(entityId);
                }
            } catch (...) {}
        }
        ImGui::EndPopup();
    }

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (opened && hasChildren) {
        // Child nodes would be drawn here in a real implementation
        for (const auto& childGUID : ecsManager.GetComponent<ChildrenComponent>(entityId).children) {
            Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);
            DrawEntityNode(ecsManager.GetComponent<NameComponent>(child).name, child, ecsManager.HasComponent<ChildrenComponent>(child));
        }

        ImGui::TreePop();
    }
}

void SceneHierarchyPanel::ReparentEntity(Entity draggedEntity, Entity targetParent) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();
    GUID_128 draggedEntityGUID = guidRegistry.GetGUIDByEntity(draggedEntity);
    GUID_128 targetParentGUID = guidRegistry.GetGUIDByEntity(targetParent);

    Transform& draggedEntityTransform = ecsManager.GetComponent<Transform>(draggedEntity);

    // If the target parent is one of the dragged entity's child, do nothing (circular dependency).
    std::set<Entity> nestedChildren;
    TraverseHierarchy(draggedEntity, nestedChildren, [&](Entity _entity, std::set<Entity>& nestedChildren) {
        AddNestedChildren(_entity, nestedChildren);
    });

    if (nestedChildren.find(targetParent) != nestedChildren.end()) {
        return;
    }

    // If the dragged entity had a parent
    if (ecsManager.HasComponent<ParentComponent>(draggedEntity)) {
        ParentComponent& parentComponent = ecsManager.GetComponent<ParentComponent>(draggedEntity);

        // Check if the new parent is the same as the old parent. If it is, exit.
        if (parentComponent.parent == targetParentGUID) return;
        
        // Set the child's new parent.
        GUID_128 oldParentGUID = parentComponent.parent;
        Entity oldParent = guidRegistry.GetEntityByGUID(oldParentGUID);
        parentComponent.parent = targetParentGUID;

        // Remove the child from the old parent.
        auto oldPChildCompOpt = ecsManager.TryGetComponent<ChildrenComponent>(oldParent);
        auto it = std::find(oldPChildCompOpt->get().children.begin(), oldPChildCompOpt->get().children.end(), EntityGUIDRegistry::GetInstance().GetGUIDByEntity(draggedEntity));
        ecsManager.GetComponent<ChildrenComponent>(oldParent).children.erase(it);

        // If the old parent has no more children, remove the children component from the old parent.
        if (oldPChildCompOpt->get().children.empty())
            ecsManager.RemoveComponent<ChildrenComponent>(oldParent);
    }
    else {
        ecsManager.AddComponent<ParentComponent>(draggedEntity, ParentComponent{});
        ecsManager.GetComponent<ParentComponent>(draggedEntity).parent = targetParentGUID;
    }

    // If the parent already has children
    if (ecsManager.HasComponent<ChildrenComponent>(targetParent)) {

        ecsManager.GetComponent<ChildrenComponent>(targetParent).children.push_back(draggedEntityGUID);
    }
    else {
        ecsManager.AddComponent<ChildrenComponent>(targetParent, ChildrenComponent{});
        ecsManager.GetComponent<ChildrenComponent>(targetParent).children.push_back(draggedEntityGUID);
    }

    // Calculate the child's world position, rotation and scale.
    Vector3D worldPos = Matrix4x4::ExtractTranslation(draggedEntityTransform.worldMatrix);
    Vector3D worldScale = Matrix4x4::ExtractScale(draggedEntityTransform.worldMatrix);
    Matrix4x4 noScale = Matrix4x4::RemoveScale(draggedEntityTransform.worldMatrix);
    Quaternion worldRot = Quaternion::FromMatrix(noScale);

    ecsManager.transformSystem->SetWorldPosition(draggedEntity, worldPos);
    ecsManager.transformSystem->SetWorldRotation(draggedEntity, worldRot.ToEulerDegrees());
    ecsManager.transformSystem->SetWorldScale(draggedEntity, worldScale);
}

void SceneHierarchyPanel::ReorderEntity(Entity draggedEntity, Entity targetSibling, bool insertAfter) {
    if (draggedEntity == targetSibling) return;
    
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();
    
    // Check if both entities are root entities (no parent)
    bool draggedIsRoot = !ecsManager.HasComponent<ParentComponent>(draggedEntity);
    bool targetIsRoot = !ecsManager.HasComponent<ParentComponent>(targetSibling);
    
    // If both are root entities, use the specialized root reordering
    if (draggedIsRoot && targetIsRoot) {
        ReorderRootEntity(draggedEntity, targetSibling, insertAfter);
        return;
    }
    
    GUID_128 draggedGUID = guidRegistry.GetGUIDByEntity(draggedEntity);
    GUID_128 targetGUID = guidRegistry.GetGUIDByEntity(targetSibling);
    
    // Prevent circular dependency
    std::set<Entity> nestedChildren;
    TraverseHierarchy(draggedEntity, nestedChildren, [&](Entity e, std::set<Entity>& nc) {
        AddNestedChildren(e, nc);
    });
    if (nestedChildren.count(targetSibling) > 0) return;
    
    // Get the parent of the target sibling (this will be the new parent for dragged entity)
    Entity targetParent = static_cast<Entity>(-1);
    GUID_128 targetParentGUID;
    
    if (ecsManager.HasComponent<ParentComponent>(targetSibling)) {
        targetParentGUID = ecsManager.GetComponent<ParentComponent>(targetSibling).parent;
        targetParent = guidRegistry.GetEntityByGUID(targetParentGUID);
    }
    
    // Save world transform before reparenting
    Transform& draggedTransform = ecsManager.GetComponent<Transform>(draggedEntity);
    Vector3D worldPos = Matrix4x4::ExtractTranslation(draggedTransform.worldMatrix);
    Vector3D worldScale = Matrix4x4::ExtractScale(draggedTransform.worldMatrix);
    Matrix4x4 noScale = Matrix4x4::RemoveScale(draggedTransform.worldMatrix);
    Quaternion worldRot = Quaternion::FromMatrix(noScale);
    
    // Remove dragged entity from its current parent's children list
    if (ecsManager.HasComponent<ParentComponent>(draggedEntity)) {
        GUID_128 oldParentGUID = ecsManager.GetComponent<ParentComponent>(draggedEntity).parent;
        Entity oldParent = guidRegistry.GetEntityByGUID(oldParentGUID);
        
        if (ecsManager.HasComponent<ChildrenComponent>(oldParent)) {
            auto& oldChildren = ecsManager.GetComponent<ChildrenComponent>(oldParent).children;
            auto it = std::find(oldChildren.begin(), oldChildren.end(), draggedGUID);
            if (it != oldChildren.end()) {
                oldChildren.erase(it);
            }
            if (oldChildren.empty()) {
                ecsManager.RemoveComponent<ChildrenComponent>(oldParent);
            }
        }
        
        if (targetParent == static_cast<Entity>(-1)) {
            // Moving to root level
            ecsManager.RemoveComponent<ParentComponent>(draggedEntity);
        } else {
            // Update parent to new parent
            ecsManager.GetComponent<ParentComponent>(draggedEntity).parent = targetParentGUID;
        }
    } else if (targetParent != static_cast<Entity>(-1)) {
        // Entity was at root, now needs a parent
        ecsManager.AddComponent<ParentComponent>(draggedEntity, ParentComponent{targetParentGUID});
    }
    
    // Insert dragged entity into the new position
    if (targetParent != static_cast<Entity>(-1)) {
        // Has a parent - insert into parent's children list
        if (!ecsManager.HasComponent<ChildrenComponent>(targetParent)) {
            ecsManager.AddComponent<ChildrenComponent>(targetParent, ChildrenComponent{});
        }
        
        auto& children = ecsManager.GetComponent<ChildrenComponent>(targetParent).children;
        auto targetIt = std::find(children.begin(), children.end(), targetGUID);
        
        if (targetIt != children.end()) {
            if (insertAfter) {
                children.insert(targetIt + 1, draggedGUID);
            } else {
                children.insert(targetIt, draggedGUID);
            }
        } else {
            children.push_back(draggedGUID);
        }
        
        // Restore world transform
        ecsManager.transformSystem->SetWorldPosition(draggedEntity, worldPos);
        ecsManager.transformSystem->SetWorldRotation(draggedEntity, worldRot.ToEulerDegrees());
        ecsManager.transformSystem->SetWorldScale(draggedEntity, worldScale);
    } else {
        // No parent - this is at root level, use sibling index ordering
        // First, find position relative to target sibling
        EnsureSiblingIndex(draggedEntity);
        EnsureSiblingIndex(targetSibling);
        ReorderRootEntity(draggedEntity, targetSibling, insertAfter);
        
        // Restore local = world transform
        ecsManager.transformSystem->SetLocalPosition(draggedEntity, worldPos);
        ecsManager.transformSystem->SetLocalRotation(draggedEntity, worldRot.ToEulerDegrees());
        ecsManager.transformSystem->SetLocalScale(draggedEntity, worldScale);
    }
}

void SceneHierarchyPanel::UnparentEntity(Entity draggedEntity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();

    // First check if the dragged entity has a ParentComponent
    if (ecsManager.HasComponent<ParentComponent>(draggedEntity)) {
        // If there is, remove it, but store the parent first.
        GUID_128 parentGUID = ecsManager.GetComponent<ParentComponent>(draggedEntity).parent;
        Entity parent = guidRegistry.GetEntityByGUID(parentGUID);
        ecsManager.RemoveComponent<ParentComponent>(draggedEntity);

        // Update the parent by removing the dragged entity from its children.
        ChildrenComponent& pChildrenComponent = ecsManager.GetComponent<ChildrenComponent>(parent);
        auto it = std::find(pChildrenComponent.children.begin(), pChildrenComponent.children.end(),EntityGUIDRegistry::GetInstance().GetGUIDByEntity(draggedEntity));
        pChildrenComponent.children.erase(it);
        if (pChildrenComponent.children.empty()) {
            ecsManager.RemoveComponent<ChildrenComponent>(parent);
        }

        // Calculate the child's local position, rotation and scale equal to its world transform (since it now has no parent).
        Transform& draggedEntityTransform = ecsManager.GetComponent<Transform>(draggedEntity);
        Vector3D worldPos = Matrix4x4::ExtractTranslation(draggedEntityTransform.worldMatrix);
        Vector3D worldScale = Matrix4x4::ExtractScale(draggedEntityTransform.worldMatrix);
        Matrix4x4 noScale = Matrix4x4::RemoveScale(draggedEntityTransform.worldMatrix);
        Quaternion worldRot = Quaternion::FromMatrix(noScale);

        ecsManager.transformSystem->SetLocalPosition(draggedEntity, worldPos);
        ecsManager.transformSystem->SetLocalRotation(draggedEntity, worldRot.ToEulerDegrees());
        ecsManager.transformSystem->SetLocalScale(draggedEntity, worldScale);
        
        // Assign a sibling index since this is now a root entity
        EnsureSiblingIndex(draggedEntity);
    }
}

void SceneHierarchyPanel::TraverseHierarchy(Entity entity, std::set<Entity>& nestedChildren, std::function<void(Entity, std::set<Entity>&)> addNestedChildren) {
    // Update its own transform first.
    addNestedChildren(entity, nestedChildren);

    // Then traverse children.
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();

    if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
        auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(entity);
        for (const auto& childGUID : childrenComp.children) {
            Entity child = guidRegistry.GetEntityByGUID(childGUID);
            TraverseHierarchy(child, nestedChildren, addNestedChildren);
        }
    }
}

void SceneHierarchyPanel::AddNestedChildren(Entity entity, std::set<Entity>& nestedChildren) {
    nestedChildren.insert(entity);
}

void SceneHierarchyPanel::CollectParentChain(Entity entity, std::vector<Entity>& chain) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();
    
    Entity current = entity;
    while (ecsManager.HasComponent<ParentComponent>(current)) {
        GUID_128 parentGUID = ecsManager.GetComponent<ParentComponent>(current).parent;
        Entity parent = guidRegistry.GetEntityByGUID(parentGUID);
        if (parent == static_cast<Entity>(-1)) break;
        chain.push_back(parent);
        current = parent;
    }
}

void SceneHierarchyPanel::ExpandToEntity(Entity entity) {
    // Clear old expanded entities
    expandedEntities.clear();
    
    // Collect all ancestors that need to be expanded
    std::vector<Entity> parentChain;
    CollectParentChain(entity, parentChain);
    
    // Add all parents to the expanded set
    for (Entity parent : parentChain) {
        expandedEntities.insert(parent);
    }
}

Entity SceneHierarchyPanel::CreateEmptyEntity(const std::string& Pathname) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        Entity newEntity = ecsManager.CreateEntity();

        // Update the name (NameComponent and Transform are already added by CreateEntity)
        if (ecsManager.HasComponent<NameComponent>(newEntity)) {
            ecsManager.GetComponent<NameComponent>(newEntity).name = Pathname;
        }

        // Assign sibling index for hierarchy ordering
        int nextIndex = GetNextRootSiblingIndex();
        ecsManager.AddComponent<SiblingIndexComponent>(newEntity, SiblingIndexComponent{ nextIndex });

        std::cout << "[SceneHierarchy] Created empty entity '" << Pathname << "' with ID " << newEntity << std::endl;

        // Take snapshot after creating entity (for undo)
        SnapshotManager::GetInstance().TakeSnapshot("Create Entity: " + Pathname);

        return newEntity;
    } catch (const std::exception& e) {
        std::cerr << "[SceneHierarchy] Failed to create empty entity: " << e.what() << std::endl;
        return static_cast<Entity>(-1);
    }
}

Entity SceneHierarchyPanel::CreateCubeEntity() {
    Entity cubeEntity = CreateEmptyEntity("Cube");
    if (cubeEntity == static_cast<Entity>(-1)) return cubeEntity;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Add ModelRenderComponent for cube with cube.obj model
        ModelRenderComponent cubeRenderer; // Uses default constructor

        // Load the cube model using direct file path
        std::string modelPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/1MeterCube.fbx";
        cubeRenderer.model = ResourceManager::GetInstance().GetResource<Model>(modelPath);
        cubeRenderer.modelGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(modelPath);

        if (cubeRenderer.model) {
            std::cout << "[SceneHierarchy] Cube model loaded successfully from: " << modelPath << std::endl;
        } else {
            std::cerr << "[SceneHierarchy] Failed to load cube model from: " << modelPath << std::endl;
        }

        // Load the default shader using direct file path
        std::string shaderPath = ResourceManager::GetPlatformShaderPath("default");
        cubeRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
        cubeRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(shaderPath);

        if (cubeRenderer.shader) {
            std::cout << "[SceneHierarchy] Default shader loaded successfully from: " << shaderPath << std::endl;
        } else {
            std::cerr << "[SceneHierarchy] Failed to load default shader from: " << shaderPath << std::endl;
        }

        ecsManager.AddComponent<ModelRenderComponent>(cubeEntity, cubeRenderer);

        // Set cube scale to 0.1,0.1,0.1
        if (ecsManager.HasComponent<Transform>(cubeEntity)) {
            Transform& transform = ecsManager.GetComponent<Transform>(cubeEntity);
            transform.localScale = Vector3D(1.0f, 1.0f, 1.0f);
            transform.isDirty = true; // Mark for update
            std::cout << "[SceneHierarchy] Set cube scale to 1.0,1.0,1.0" << std::endl;
        }

        std::cout << "[SceneHierarchy] Created cube entity with ID " << cubeEntity << std::endl;
        return cubeEntity;
    } catch (const std::exception& e) {
        std::cerr << "[SceneHierarchy] Failed to create cube entity: " << e.what() << std::endl;
        return static_cast<Entity>(-1);
    }
}

Entity SceneHierarchyPanel::CreateCameraEntity() {
    Entity cameraEntity = CreateEmptyEntity("Camera");
    if (cameraEntity == static_cast<Entity>(-1)) return cameraEntity;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Find the highest priority among existing cameras and deactivate all cameras (Unity-like)
        int maxPriority = -1;
        std::vector<Entity> allEntities = ecsManager.GetActiveEntities();
        for (const auto& entity : allEntities) {
            if (ecsManager.HasComponent<CameraComponent>(entity)) {
                auto& existingCam = ecsManager.GetComponent<CameraComponent>(entity);
                existingCam.isActive = false; // Deactivate existing cameras
                if (existingCam.priority > maxPriority) {
                    maxPriority = existingCam.priority;
                }
            }
        }

        std::cout << "[SceneHierarchy] Deactivated all existing cameras before creating new one" << std::endl;

        // Add CameraComponent with default settings
        CameraComponent cameraComp;
        cameraComp.isActive = true; // Activate new camera by default (Unity-like behavior)
        cameraComp.priority = maxPriority + 1; // Higher priority than existing cameras
        cameraComp.target = glm::vec3(0.0f, 0.0f, -1.0f);
        cameraComp.up = glm::vec3(0.0f, 1.0f, 0.0f);
        cameraComp.yaw = -90.0f;
        cameraComp.pitch = 0.0f;
        cameraComp.useFreeRotation = true;
        cameraComp.projectionType = ProjectionType::PERSPECTIVE;
        cameraComp.fov = 45.0f;
        cameraComp.nearPlane = 0.1f;
        cameraComp.farPlane = 100.0f;
        cameraComp.orthoSize = 5.0f;

        ecsManager.AddComponent<CameraComponent>(cameraEntity, cameraComp);

        std::cout << "[SceneHierarchy] Created camera entity with ID " << cameraEntity
                  << " with CameraComponent (active=true, priority=" << cameraComp.priority << ")" << std::endl;
        return cameraEntity;
    } catch (const std::exception& e) {
        std::cerr << "[SceneHierarchy] Failed to create camera entity: " << e.what() << std::endl;
        return static_cast<Entity>(-1);
    }
}

Entity SceneHierarchyPanel::DuplicateEntity(Entity sourceEntity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Get source entity name
        std::string sourceName = "Entity";
        if (ecsManager.HasComponent<NameComponent>(sourceEntity)) {
            sourceName = ecsManager.GetComponent<NameComponent>(sourceEntity).name;
        }

        // Generate unique name (Entity (1), Entity (2), etc.)
        std::string newName = sourceName;
        int counter = 1;
        bool nameExists = true;
        while (nameExists) {
            newName = sourceName + " (" + std::to_string(counter) + ")";
            nameExists = false;
            // Check if name already exists
            for (const auto& entity : ecsManager.GetActiveEntities()) {
                if (ecsManager.HasComponent<NameComponent>(entity)) {
                    if (ecsManager.GetComponent<NameComponent>(entity).name == newName) {
                        nameExists = true;
                        counter++;
                        break;
                    }
                }
            }
        }

        // Create new entity
        Entity newEntity = ecsManager.CreateEntity();
        std::cout << "[SceneHierarchy] Duplicating entity '" << sourceName << "' -> '" << newName << "'" << std::endl;

        // Set name
        if (ecsManager.HasComponent<NameComponent>(newEntity)) {
            ecsManager.GetComponent<NameComponent>(newEntity).name = newName;
        }

        // Copy Transform
        if (ecsManager.HasComponent<Transform>(sourceEntity)) {
            Transform sourceTransform = ecsManager.GetComponent<Transform>(sourceEntity);
            if (ecsManager.HasComponent<Transform>(newEntity)) {
                ecsManager.GetComponent<Transform>(newEntity) = sourceTransform;
            } else {
                ecsManager.AddComponent<Transform>(newEntity, sourceTransform);
            }
        }

        // Copy ActiveComponent
        if (ecsManager.HasComponent<ActiveComponent>(sourceEntity)) {
            ActiveComponent sourceActive = ecsManager.GetComponent<ActiveComponent>(sourceEntity);
            ecsManager.AddComponent<ActiveComponent>(newEntity, sourceActive);
        }

        // Copy ModelRenderComponent
        if (ecsManager.HasComponent<ModelRenderComponent>(sourceEntity)) {
            ModelRenderComponent sourceModel = ecsManager.GetComponent<ModelRenderComponent>(sourceEntity);
            ecsManager.AddComponent<ModelRenderComponent>(newEntity, sourceModel);
        }

        // Copy SpriteRenderComponent
        if (ecsManager.HasComponent<SpriteRenderComponent>(sourceEntity)) {
            SpriteRenderComponent sourceSprite = ecsManager.GetComponent<SpriteRenderComponent>(sourceEntity);
            ecsManager.AddComponent<SpriteRenderComponent>(newEntity, sourceSprite);
        }

        // Copy TextRenderComponent
        if (ecsManager.HasComponent<TextRenderComponent>(sourceEntity)) {
            TextRenderComponent sourceText = ecsManager.GetComponent<TextRenderComponent>(sourceEntity);
            ecsManager.AddComponent<TextRenderComponent>(newEntity, sourceText);
        }

        // Copy LightComponent
        if (ecsManager.HasComponent<LightComponent>(sourceEntity)) {
            LightComponent sourceLight = ecsManager.GetComponent<LightComponent>(sourceEntity);
            ecsManager.AddComponent<LightComponent>(newEntity, sourceLight);
        }

        // Copy CameraComponent
        if (ecsManager.HasComponent<CameraComponent>(sourceEntity)) {
            CameraComponent sourceCam = ecsManager.GetComponent<CameraComponent>(sourceEntity);
            // Don't copy active status for cameras (Unity-like)
            sourceCam.isActive = false;
            ecsManager.AddComponent<CameraComponent>(newEntity, sourceCam);
        }

        // Copy AudioComponent
        if (ecsManager.HasComponent<AudioComponent>(sourceEntity)) {
            AudioComponent sourceAudio = ecsManager.GetComponent<AudioComponent>(sourceEntity);
            ecsManager.AddComponent<AudioComponent>(newEntity, sourceAudio);
        }

        // Copy AnimationComponent
        if (ecsManager.HasComponent<AnimationComponent>(sourceEntity)) {
            AnimationComponent sourceAnim = ecsManager.GetComponent<AnimationComponent>(sourceEntity);
            ecsManager.AddComponent<AnimationComponent>(newEntity, sourceAnim);

            // Re-link animator to model if both exist
            if (ecsManager.HasComponent<ModelRenderComponent>(newEntity)) {
                auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(newEntity);
                auto& animComp = ecsManager.GetComponent<AnimationComponent>(newEntity);
                if (modelComp.model && !animComp.clipPaths.empty()) {
                    Animator* animator = animComp.EnsureAnimator();
                    modelComp.SetAnimator(animator);
                    animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount(), newEntity);
                }
            }
        }

        // Take snapshot after duplication (for undo)
        SnapshotManager::GetInstance().TakeSnapshot("Duplicate Entity: " + sourceName);

        std::cout << "[SceneHierarchy] Successfully duplicated entity (ID: " << newEntity << ")" << std::endl;
        return newEntity;

    } catch (const std::exception& e) {
        std::cerr << "[SceneHierarchy] Failed to duplicate entity: " << e.what() << std::endl;
        return static_cast<Entity>(-1);
    }
}

// ============================================================================
// Sibling Index Management Functions
// ============================================================================

void SceneHierarchyPanel::EnsureSiblingIndex(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecsManager.HasComponent<SiblingIndexComponent>(entity)) {
        int nextIndex = GetNextRootSiblingIndex();
        ecsManager.AddComponent<SiblingIndexComponent>(entity, SiblingIndexComponent{ nextIndex });
    }
}

int SceneHierarchyPanel::GetNextRootSiblingIndex() {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    std::vector<Entity> allEntities = ecsManager.GetActiveEntities();
    int maxIndex = -1;
    
    for (Entity entity : allEntities) {
        // Only consider root entities
        if (ecsManager.TryGetComponent<ParentComponent>(entity).has_value()) continue;
        
        if (ecsManager.HasComponent<SiblingIndexComponent>(entity)) {
            int idx = ecsManager.GetComponent<SiblingIndexComponent>(entity).siblingIndex;
            if (idx > maxIndex) maxIndex = idx;
        }
    }
    return maxIndex + 1;
}

std::vector<Entity> SceneHierarchyPanel::GetSortedRootEntities() {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    std::vector<Entity> allEntities = ecsManager.GetActiveEntities();
    std::vector<Entity> rootEntities;
    
    // Collect root entities (no parent)
    for (Entity entity : allEntities) {
        if (!ecsManager.TryGetComponent<ParentComponent>(entity).has_value()) {
            // Ensure the entity has a sibling index
            EnsureSiblingIndex(entity);
            rootEntities.push_back(entity);
        }
    }
    
    // Sort by sibling index
    std::sort(rootEntities.begin(), rootEntities.end(), 
        [&ecsManager](Entity a, Entity b) {
            int idxA = ecsManager.HasComponent<SiblingIndexComponent>(a) 
                       ? ecsManager.GetComponent<SiblingIndexComponent>(a).siblingIndex : 0;
            int idxB = ecsManager.HasComponent<SiblingIndexComponent>(b) 
                       ? ecsManager.GetComponent<SiblingIndexComponent>(b).siblingIndex : 0;
            return idxA < idxB;
        });
    
    return rootEntities;
}

void SceneHierarchyPanel::UpdateSiblingIndices() {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    std::vector<Entity> sortedRoots = GetSortedRootEntities();
    
    // Reassign consecutive indices to compact the order
    for (int i = 0; i < static_cast<int>(sortedRoots.size()); ++i) {
        if (ecsManager.HasComponent<SiblingIndexComponent>(sortedRoots[i])) {
            ecsManager.GetComponent<SiblingIndexComponent>(sortedRoots[i]).siblingIndex = i;
        }
    }
}

void SceneHierarchyPanel::ReorderRootEntity(Entity draggedEntity, Entity targetSibling, bool insertAfter) {
    if (draggedEntity == targetSibling) return;
    
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    
    // Ensure both entities have sibling indices
    EnsureSiblingIndex(draggedEntity);
    EnsureSiblingIndex(targetSibling);
    
    int targetIndex = ecsManager.GetComponent<SiblingIndexComponent>(targetSibling).siblingIndex;
    
    // Get sorted list of root entities
    std::vector<Entity> sortedRoots = GetSortedRootEntities();
    
    // Remove dragged entity from the list
    sortedRoots.erase(std::remove(sortedRoots.begin(), sortedRoots.end(), draggedEntity), sortedRoots.end());
    
    // Find position to insert
    auto targetIt = std::find(sortedRoots.begin(), sortedRoots.end(), targetSibling);
    if (targetIt != sortedRoots.end()) {
        if (insertAfter) {
            sortedRoots.insert(targetIt + 1, draggedEntity);
        } else {
            sortedRoots.insert(targetIt, draggedEntity);
        }
    } else {
        sortedRoots.push_back(draggedEntity);
    }
    
    // Reassign sibling indices
    for (int i = 0; i < static_cast<int>(sortedRoots.size()); ++i) {
        if (ecsManager.HasComponent<SiblingIndexComponent>(sortedRoots[i])) {
            ecsManager.GetComponent<SiblingIndexComponent>(sortedRoots[i]).siblingIndex = i;
        }
    }
}

// ============================================================================
// Copy/Paste and Multi-Selection Functions
// ============================================================================

void SceneHierarchyPanel::CopySelectedEntities() {
    const std::vector<Entity>& selectedEntities = GUIManager::GetSelectedEntities();
    if (selectedEntities.empty()) return;

    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();

    // Store GUIDs instead of Entity IDs for persistence
    g_EntityClipboard.copiedEntityGUIDs.clear();
    for (Entity entity : selectedEntities) {
        GUID_128 guid = guidRegistry.GetGUIDByEntity(entity);
        if (guid.high != 0 || guid.low != 0) {
            g_EntityClipboard.copiedEntityGUIDs.push_back(guid);
        }
    }
    g_EntityClipboard.hasData = !g_EntityClipboard.copiedEntityGUIDs.empty();

    std::cout << "[SceneHierarchy] Copied " << g_EntityClipboard.copiedEntityGUIDs.size() << " entities to clipboard" << std::endl;
}

void SceneHierarchyPanel::PasteEntities() {
    std::cout << "[SceneHierarchy] PasteEntities() called. hasData=" << g_EntityClipboard.hasData
              << ", numGUIDs=" << g_EntityClipboard.copiedEntityGUIDs.size() << std::endl;

    if (!g_EntityClipboard.hasData || g_EntityClipboard.copiedEntityGUIDs.empty()) {
        std::cout << "[SceneHierarchy] Clipboard is empty, nothing to paste" << std::endl;
        return;
    }

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();

    // Resolve GUIDs to current Entity IDs and verify they still exist
    std::vector<Entity> validEntities;
    for (const GUID_128& guid : g_EntityClipboard.copiedEntityGUIDs) {
        Entity entity = guidRegistry.GetEntityByGUID(guid);
        if (entity != static_cast<Entity>(-1) && ecsManager.TryGetComponent<NameComponent>(entity).has_value()) {
            validEntities.push_back(entity);
        }
    }

    if (validEntities.empty()) {
        std::cout << "[SceneHierarchy] No valid entities to paste (original entities may have been deleted)" << std::endl;
        g_EntityClipboard.Clear();
        return;
    }

    std::vector<Entity> pastedEntities = DuplicateEntities(validEntities);
    if (!pastedEntities.empty()) {
        GUIManager::SetSelectedEntities(pastedEntities);
        std::cout << "[SceneHierarchy] Pasted " << pastedEntities.size() << " entities" << std::endl;
    }
    // Note: Clipboard is NOT cleared after paste, allowing multiple pastes
}

std::vector<Entity> SceneHierarchyPanel::DuplicateEntities(const std::vector<Entity>& sourceEntities) {
    std::vector<Entity> duplicatedEntities;
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (sourceEntities.empty()) return duplicatedEntities;

    // Build snapshot description
    std::string snapshotDesc = sourceEntities.size() == 1
        ? "Duplicate Entity: " + ecsManager.GetComponent<NameComponent>(sourceEntities[0]).name
        : "Duplicate " + std::to_string(sourceEntities.size()) + " Entities";

    // Take snapshot before duplicating (for undo)
    SnapshotManager::GetInstance().TakeSnapshot(snapshotDesc);

    for (Entity sourceEntity : sourceEntities) {
        Entity duplicated = DuplicateEntity(sourceEntity);
        if (duplicated != static_cast<Entity>(-1)) {
            duplicatedEntities.push_back(duplicated);
        }
    }

    std::cout << "[SceneHierarchy] Duplicated " << duplicatedEntities.size() << " entities" << std::endl;
    return duplicatedEntities;
}

void SceneHierarchyPanel::CollectEntitiesRecursive(Entity entity, std::vector<Entity>& flatList) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    EntityGUIDRegistry& guidRegistry = EntityGUIDRegistry::GetInstance();

    flatList.push_back(entity);

    // If this entity has children, add them recursively
    if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
        const auto& children = ecsManager.GetComponent<ChildrenComponent>(entity).children;
        for (const auto& childGUID : children) {
            Entity child = guidRegistry.GetEntityByGUID(childGUID);
            if (child != static_cast<Entity>(-1)) {
                CollectEntitiesRecursive(child, flatList);
            }
        }
    }
}

std::vector<Entity> SceneHierarchyPanel::GetFlatEntityList() {
    std::vector<Entity> flatList;
    std::vector<Entity> rootEntities = GetSortedRootEntities();

    for (Entity root : rootEntities) {
        CollectEntitiesRecursive(root, flatList);
    }

    return flatList;
}

int SceneHierarchyPanel::GetEntityDisplayIndex(Entity entity, const std::vector<Entity>& flatList) {
    auto it = std::find(flatList.begin(), flatList.end(), entity);
    if (it != flatList.end()) {
        return static_cast<int>(std::distance(flatList.begin(), it));
    }
    return -1;
}

void SceneHierarchyPanel::DeleteSelectedEntities() {
    const std::vector<Entity>& selectedEntities = GUIManager::GetSelectedEntities();
    if (selectedEntities.empty()) return;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Build description for undo
        std::string snapshotDesc = selectedEntities.size() == 1
            ? "Delete Entity: " + ecsManager.GetComponent<NameComponent>(selectedEntities[0]).name
            : "Delete " + std::to_string(selectedEntities.size()) + " Entities";

        std::cout << "[SceneHierarchy] Deleting " << selectedEntities.size() << " entities" << std::endl;

        // Take snapshot before deleting (for undo)
        SnapshotManager::GetInstance().TakeSnapshot(snapshotDesc);

        // Make a copy since we're modifying the selection
        std::vector<Entity> entitiesToDelete = selectedEntities;

        // Clear selection before deleting
        GUIManager::ClearSelectedEntities();

        // Delete all selected entities
        for (Entity entity : entitiesToDelete) {
            if (ecsManager.TryGetComponent<NameComponent>(entity).has_value()) {
                ecsManager.DestroyEntity(entity);
            }
        }

        std::cout << "[SceneHierarchy] Entities deleted successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[SceneHierarchy] Failed to delete entities: " << e.what() << std::endl;
    }
}

void SceneHierarchyPanel::SelectRange(Entity fromEntity, Entity toEntity) {
    std::vector<Entity> flatList = GetFlatEntityList();

    int fromIndex = GetEntityDisplayIndex(fromEntity, flatList);
    int toIndex = GetEntityDisplayIndex(toEntity, flatList);

    if (fromIndex == -1 || toIndex == -1) {
        // If either entity not found, just select the target
        GUIManager::SetSelectedEntity(toEntity);
        return;
    }

    // Determine range direction
    int startIndex = std::min(fromIndex, toIndex);
    int endIndex = std::max(fromIndex, toIndex);

    // Select all entities in the range
    std::vector<Entity> rangeSelection;
    for (int i = startIndex; i <= endIndex; ++i) {
        rangeSelection.push_back(flatList[i]);
    }

    GUIManager::SetSelectedEntities(rangeSelection);
    std::cout << "[SceneHierarchy] Range selected " << rangeSelection.size() << " entities" << std::endl;
}
