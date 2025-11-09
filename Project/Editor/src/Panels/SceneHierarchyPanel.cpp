#include "Panels/SceneHierarchyPanel.hpp"
#include "imgui.h"
#include "pch.h"
#include "GUIManager.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include "EditorComponents.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
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
#include "EditorState.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <Utilities/GUID.hpp>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include "Panels/ScenePanel.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "SnapshotManager.hpp"
#include "UndoableWidgets.hpp"

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

        // Handle Delete key for deleting selected entity
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            Entity selectedEntity = GUIManager::GetSelectedEntity();
            if (selectedEntity != static_cast<Entity>(-1)) {
                try {
                    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                    std::string entityName = ecsManager.GetComponent<NameComponent>(selectedEntity).name;

                    std::cout << "[SceneHierarchy] Deleting entity: " << entityName << " (ID: " << selectedEntity << ")" << std::endl;

                    // Take snapshot before deleting (for undo)
                    SnapshotManager::GetInstance().TakeSnapshot("Delete Entity: " + entityName);

                    // Clear selection before deleting
                    GUIManager::SetSelectedEntity(static_cast<Entity>(-1));

                    // Delete the entity
                    ecsManager.DestroyEntity(selectedEntity);

                    std::cout << "[SceneHierarchy] Entity deleted successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[SceneHierarchy] Failed to delete entity: " << e.what() << std::endl;
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

        if (sceneExpanded) {
            try {
                // Get the active ECS manager
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

                // Always get fresh entity list to ensure we see newly created entities
                std::vector<Entity> allEntities = ecsManager.GetActiveEntities();

                // Draw entity nodes starting from root entities, in a depth-first manner.
                for (const auto& entity : allEntities) {
                    // Only draw root entities (entities without a parent)
                    if (!ecsManager.TryGetComponent<ParentComponent>(entity).has_value()) {
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

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (GUIManager::IsEntitySelected(entityId)) flags |= ImGuiTreeNodeFlags_Selected;

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
        opened = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", displayName.c_str());

        // Pop color if we pushed it
        if (!isEntityActive) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemClicked()) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl) {
                // Multi-select: toggle
                if (GUIManager::IsEntitySelected(entityId)) {
                    GUIManager::RemoveSelectedEntity(entityId);
                } else {
                    GUIManager::AddSelectedEntity(entityId);
                }
            } else {
                // Single select
                GUIManager::SetSelectedEntity(entityId);
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
                            std::cout << "[SceneHierarchy] Entity has sprite at position ("
                                     << sprite.position.x << ", " << sprite.position.y << ", " << sprite.position.z
                                     << ") is3D=" << sprite.is3D << std::endl;
                            // For 2D sprites, use the sprite position instead of transform
                            if (!sprite.is3D) {
                                entityPos = sprite.position.ConvertToGLM();
                                std::cout << "[SceneHierarchy] Using sprite position for 2D sprite" << std::endl;
                            }
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
        //ImGuiIO& io = ImGui::GetIO();

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

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity dragged = *(Entity*)payload->Data;
            ReparentEntity(dragged, entityId);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))   { /* TODO */ }
        if (ImGui::MenuItem("Duplicate")){ /* TODO */ }
        if (ImGui::MenuItem("Rename", "F2")) {
            renamingEntity = entityId;
            startRenaming  = true;
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

Entity SceneHierarchyPanel::CreateEmptyEntity(const std::string& Pathname) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        Entity newEntity = ecsManager.CreateEntity();

        // Update the name (NameComponent and Transform are already added by CreateEntity)
        if (ecsManager.HasComponent<NameComponent>(newEntity)) {
            ecsManager.GetComponent<NameComponent>(newEntity).name = Pathname;
        }

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
        std::string modelPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/cube.obj";
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
            transform.localScale = Vector3D(0.1f, 0.1f, 0.1f);
            transform.isDirty = true; // Mark for update
            std::cout << "[SceneHierarchy] Set cube scale to 0.1,0.1,0.1" << std::endl;
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
