#include "Panels/SceneHierarchyPanel.hpp"
#include "imgui.h"
#include "pch.h"
#include "GUIManager.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/NameComponent.hpp"
#include <Hierarchy/ChildrenComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <PrefabIO.hpp>
#include <imgui_internal.h>
#include "Scene/SceneManager.hpp"
#include <Transform/TransformComponent.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Sound/AudioComponent.hpp>
#include <Utilities/GUID.hpp>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>

SceneHierarchyPanel::SceneHierarchyPanel()
    : EditorPanel("Scene Hierarchy", true) {
}

void SceneHierarchyPanel::MarkForRefresh() {
    needsRefresh = true;
}

void SceneHierarchyPanel::OnImGuiRender() {
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

        ImGui::Text(SceneManager::GetInstance().GetSceneName().c_str());
        ImGui::Separator();

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

            //// Get all active entities
            //std::vector<Entity> entities = ecsManager.GetActiveEntities();

            //// Display each entity
            //for (Entity entity : entities) {
            //    std::string entityName;

            //    // Try to get the name from NameComponent
            //    if (ecsManager.HasComponent<NameComponent>(entity)) {
            //        const NameComponent& nameComp = ecsManager.GetComponent<NameComponent>(entity);
            //        entityName = nameComp.name;
            //    } else {
            //        // Fallback to "Entity [ID]" format
            //        entityName = "Entity " + std::to_string(entity);
            //    }

            //    bool hasChildren = ecsManager.HasComponent<ChildrenComponent>(entity);
            //    DrawEntityNode(entityName, entity, hasChildren);
            //}

            if (allEntities.empty()) {
                ImGui::Text("No entities in scene");
            }
        }
        catch (const std::exception& e) {
            ImGui::Text("Error accessing ECS: %s", e.what());
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
}


void SceneHierarchyPanel::DrawEntityNode(const std::string& entityName, Entity entityId, bool hasChildren)
{
    assert(!entityName.empty() && "Entity name cannot be empty");

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (GUIManager::GetSelectedEntity() == entityId) flags |= ImGuiTreeNodeFlags_Selected;

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

        if (ImGui::InputText(("##rename" + std::to_string(entityId)).c_str(),
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

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (!ImGui::IsItemActive() && !ImGui::IsItemFocused()))
        {
            renamingEntity = static_cast<Entity>(-1);
        }
    }
    else
    {
        opened = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", entityName.c_str());
        if (ImGui::IsItemClicked())
            GUIManager::SetSelectedEntity(entityId);
    }

    // --- DRAG SOURCE from a hierarchy row (exactly one payload) ---
    {
        ImGuiIO& io = ImGui::GetIO();

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
        for (const auto& child : ecsManager.GetComponent<ChildrenComponent>(entityId).children) {
            DrawEntityNode(ecsManager.GetComponent<NameComponent>(child).name, child, ecsManager.HasComponent<ChildrenComponent>(child));
        }

        ImGui::TreePop();
    }
}

void SceneHierarchyPanel::ReparentEntity(Entity draggedEntity, Entity targetParent) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
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
        if (parentComponent.parent == targetParent) return;
        
        // Set the child's new parent.
        Entity oldParent = parentComponent.parent;
        parentComponent.parent = targetParent;

        // Remove the child from the old parent.
        auto oldPChildCompOpt = ecsManager.TryGetComponent<ChildrenComponent>(oldParent);
        auto it = std::find(oldPChildCompOpt->get().children.begin(), oldPChildCompOpt->get().children.end(), draggedEntity);
        ecsManager.GetComponent<ChildrenComponent>(oldParent).children.erase(it);

        // If the old parent has no more children, remove the children component from the old parent.
        if (oldPChildCompOpt->get().children.empty())
            ecsManager.RemoveComponent<ChildrenComponent>(oldParent);
    }
    else {
        ecsManager.AddComponent<ParentComponent>(draggedEntity, ParentComponent{});
        ecsManager.GetComponent<ParentComponent>(draggedEntity).parent = targetParent;
    }

    // If the parent already has children
    if (ecsManager.HasComponent<ChildrenComponent>(targetParent)) {

        ecsManager.GetComponent<ChildrenComponent>(targetParent).children.push_back(draggedEntity);
    }
    else {
        ecsManager.AddComponent<ChildrenComponent>(targetParent, ChildrenComponent{});
        ecsManager.GetComponent<ChildrenComponent>(targetParent).children.push_back(draggedEntity);
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
    // First check if the dragged entity has a ParentComponent
    if (ecsManager.HasComponent<ParentComponent>(draggedEntity)) {
        // If there is, remove it, but store the parent first.
        Entity parent = ecsManager.GetComponent<ParentComponent>(draggedEntity).parent;
        ecsManager.RemoveComponent<ParentComponent>(draggedEntity);

        // Update the parent by removing the dragged entity from its children.
        ChildrenComponent& pChildrenComponent = ecsManager.GetComponent<ChildrenComponent>(parent);
        auto it = std::find(pChildrenComponent.children.begin(), pChildrenComponent.children.end(),draggedEntity);
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
    if (ecsManager.HasComponent<ChildrenComponent>(entity)) {
        auto& childrenComp = ecsManager.GetComponent<ChildrenComponent>(entity);
        for (const auto& child : childrenComp.children) {
            TraverseHierarchy(child, nestedChildren, addNestedChildren);
        }
    }
}

void SceneHierarchyPanel::AddNestedChildren(Entity entity, std::set<Entity>& nestedChildren) {
    nestedChildren.insert(entity);
}

Entity SceneHierarchyPanel::CreateEmptyEntity(const std::string& name) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        Entity newEntity = ecsManager.CreateEntity();

        // Update the name (NameComponent and Transform are already added by CreateEntity)
        if (ecsManager.HasComponent<NameComponent>(newEntity)) {
            ecsManager.GetComponent<NameComponent>(newEntity).name = name;
        }

        std::cout << "[SceneHierarchy] Created empty entity '" << name << "' with ID " << newEntity << std::endl;

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
        std::string modelPath = "Resources/Models/cube.obj";
        cubeRenderer.model = ResourceManager::GetInstance().GetResource<Model>(modelPath);

        if (cubeRenderer.model) {
            std::cout << "[SceneHierarchy] Cube model loaded successfully from: " << modelPath << std::endl;
        } else {
            std::cerr << "[SceneHierarchy] Failed to load cube model from: " << modelPath << std::endl;
        }

        // Load the default shader using direct file path
        std::string shaderPath = "Resources/Shaders/default";
        cubeRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

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

        // TODO: Add CameraComponent when it exists
        // For now just create empty entity with transform
        std::cout << "[SceneHierarchy] Created camera entity with ID " << cameraEntity << " (Camera component not implemented yet)" << std::endl;
        return cameraEntity;
    } catch (const std::exception& e) {
        std::cerr << "[SceneHierarchy] Failed to create camera entity: " << e.what() << std::endl;
        return static_cast<Entity>(-1);
    }
}
