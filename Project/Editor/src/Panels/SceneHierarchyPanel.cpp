#include "Panels/SceneHierarchyPanel.hpp"
#include "imgui.h"
#include "pch.h"
#include "GUIManager.hpp"
#include <Hierarchy/ChildrenComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>

SceneHierarchyPanel::SceneHierarchyPanel() 
    : EditorPanel("Scene Hierarchy", true) {
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

        ImGui::Text("Scene Objects:");
        ImGui::Separator();

        try {
            // Get the active ECS manager
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

            // Draw entity nodes starting from root entities, in a depth-first manner.
            for (const auto& entity : ecsManager.GetActiveEntities()) {
                if (!ecsManager.HasComponent<ParentComponent>(entity)) {
                    std::string entityName = ecsManager.GetComponent<NameComponent>(entity).name;
                    DrawEntityNode(entityName, entity, ecsManager.HasComponent<ChildrenComponent>(entity));
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

            if (ecsManager.GetActiveEntities().empty()) {
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
                // TODO: Create new empty entity
            }
            if (ImGui::MenuItem("Create Cube")) {
                // TODO: Create cube primitive
            }
            if (ImGui::MenuItem("Create Sphere")) {
                // TODO: Create sphere primitive
            }
            ImGui::EndPopup();
        }

        // Handle unparenting of an entity
        // Take up all remaining space in the window
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.y > 0) {
            ImGui::InvisibleButton("HierarchyBackground", avail);
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                    Entity dragged = *(Entity*)payload->Data;
                    UnparentEntity(dragged);
                }
                ImGui::EndDragDropTarget();
            }
        }
    }
    ImGui::End();
}

void SceneHierarchyPanel::DrawEntityNode(const std::string& entityName, Entity entityId, bool hasChildren) {
    assert(!entityName.empty() && "Entity name cannot be empty");

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (GUIManager::GetSelectedEntity() == entityId) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool opened = false;

    // Check if this entity is being renamed
    if (renamingEntity == entityId) {
        // Show input field for renaming
        ImGui::SetNextItemWidth(-1.0f);

        // Initialize rename buffer if starting to rename
        if (startRenaming) {
            renameBuffer.clear();
            renameBuffer.resize(256, '\0');
            if (!entityName.empty() && entityName.length() < 255) {
                std::copy(entityName.begin(), entityName.end(), renameBuffer.begin());
            }
            startRenaming = false;
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText(("##rename" + std::to_string(entityId)).c_str(),
                           renameBuffer.data(), renameBuffer.size(),
                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            // Apply rename
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                if (ecsManager.HasComponent<NameComponent>(entityId)) {
                    NameComponent& nameComp = ecsManager.GetComponent<NameComponent>(entityId);
                    nameComp.name = std::string(renameBuffer.data());
                }
            } catch (const std::exception& e) {
                // Handle error silently or log
            }
            renamingEntity = static_cast<Entity>(-1);
        }

        // Cancel rename on escape or lost focus
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (!ImGui::IsItemActive() && !ImGui::IsItemFocused())) {
            renamingEntity = static_cast<Entity>(-1);
        }
    } else {
        // Normal tree node display
        opened = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", entityName.c_str());

        if (ImGui::IsItemClicked()) {
            GUIManager::SetSelectedEntity(entityId);
        }
    }

    // Context menu for individual entities
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            // TODO: Delete entity
        }
        if (ImGui::MenuItem("Duplicate")) {
            // TODO: Duplicate entity
        }
        if (ImGui::MenuItem("Rename", "F2")) {
            renamingEntity = entityId;
            startRenaming = true;
        }
        ImGui::EndPopup();
    }

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    // Drag source
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entityId, sizeof(Entity));
        ImGui::Text("Move %s", ecsManager.GetComponent<NameComponent>(entityId).name.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity dragged = *(Entity*)payload->Data;
            ReparentEntity(dragged, entityId); // your logic
        }
        ImGui::EndDragDropTarget();
    }

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
        auto it = oldPChildCompOpt->get().children.find(draggedEntity);
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

        ecsManager.GetComponent<ChildrenComponent>(targetParent).children.insert(draggedEntity);
    }
    else {
        ecsManager.AddComponent<ChildrenComponent>(targetParent, ChildrenComponent{});
        ecsManager.GetComponent<ChildrenComponent>(targetParent).children.insert(draggedEntity);
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
        auto it = pChildrenComponent.children.find(draggedEntity);
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
