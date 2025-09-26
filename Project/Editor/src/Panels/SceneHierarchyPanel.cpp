#include "Panels/SceneHierarchyPanel.hpp"
#include "imgui.h"
#include "pch.h"
#include "GUIManager.hpp"

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

            // Get all active entities
            std::vector<Entity> entities = ecsManager.GetActiveEntities();

            // Display each entity
            for (Entity entity : entities) {
                std::string entityName;

                // Try to get the name from NameComponent
                if (ecsManager.HasComponent<NameComponent>(entity)) {
                    const NameComponent& nameComp = ecsManager.GetComponent<NameComponent>(entity);
                    entityName = nameComp.name;
                } else {
                    // Fallback to "Entity [ID]" format
                    entityName = "Entity " + std::to_string(entity);
                }

                DrawEntityNode(entityName, entity);
            }

            if (entities.empty()) {
                ImGui::Text("No entities in scene");
            }
        }
        catch (const std::exception& e) {
            ImGui::Text("Error accessing ECS: %s", e.what());
        }

        ImGui::Separator();

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

    // --- drag source: allow creating a prefab by dragging this row ---
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            Entity payload = entityId; // copy; ImGui copies this buffer into the payload
            ImGui::SetDragDropPayload("ENTITY_AS_PREFAB", &payload, sizeof(Entity));
            ImGui::TextUnformatted("Create Prefab");
            ImGui::Separator();
            ImGui::Text("%s", entityName.c_str());
            ImGui::EndDragDropSource();
        }
    }
    // -----------------------------------------------------------------

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

    if (opened && hasChildren)
        ImGui::TreePop();
}