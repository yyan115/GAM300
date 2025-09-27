#pragma once

#include "EditorPanel.hpp"
#include <ECS/ECSRegistry.hpp>
#include <ECS/NameComponent.hpp>
#include <ECS/Entity.hpp>
#include "../GUIManager.hpp"

/**
 * @brief Scene Hierarchy panel showing the structure of objects in the current scene.
 * 
 * This panel displays a tree view of all entities and objects in the scene,
 * similar to Unity's Hierarchy window.
 */
class SceneHierarchyPanel : public EditorPanel {
public:
    SceneHierarchyPanel();
    virtual ~SceneHierarchyPanel() = default;

    /**
     * @brief Render the scene hierarchy panel's ImGui content.
     */
    void OnImGuiRender() override;

private:
    void DrawEntityNode(const std::string& entityName, Entity entityId, bool hasChildren = false);
    void ReparentEntity(Entity draggedEntity, Entity targetParent);
    void UnparentEntity(Entity draggedEntity);
    void TraverseHierarchy(Entity entity, std::set<Entity>& nestedChildren, std::function<void(Entity, std::set<Entity>&)> addNestedChildren);
    void AddNestedChildren(Entity entity, std::set<Entity>& nestedChildren);

    // Rename functionality
    Entity renamingEntity = static_cast<Entity>(-1);
    std::vector<char> renameBuffer;
    bool startRenaming = false;
};