#pragma once

#include "EditorPanel.hpp"
#include <ECS/ECSRegistry.hpp>
#include <ECS/NameComponent.hpp>
#include <ECS/Entity.hpp>
#include <ECS/SiblingIndexComponent.hpp>
#include "../GUIManager.hpp"
#include <set>

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

    /**
     * @brief Mark the hierarchy panel as needing a refresh.
     */
    void MarkForRefresh();

    // Public methods for global keyboard shortcuts
    void CopySelectedEntities();
    void PasteEntities();
    std::vector<Entity> DuplicateEntities(const std::vector<Entity>& sourceEntities);
    void DeleteSelectedEntities();

private:
    void DrawEntityNode(const std::string& entityName, Entity entityId, bool hasChildren = false);
    void ReparentEntity(Entity draggedEntity, Entity targetParent);
    void ReorderEntity(Entity draggedEntity, Entity targetSibling, bool insertAfter);
    void UnparentEntity(Entity draggedEntity);
    void TraverseHierarchy(Entity entity, std::set<Entity>& nestedChildren, std::function<void(Entity, std::set<Entity>&)> addNestedChildren);
    void AddNestedChildren(Entity entity, std::set<Entity>& nestedChildren);
    
    // Sibling index management for root entities
    void EnsureSiblingIndex(Entity entity);
    void UpdateSiblingIndices();
    int GetNextRootSiblingIndex();
    std::vector<Entity> GetSortedRootEntities();
    void ReorderRootEntity(Entity draggedEntity, Entity targetSibling, bool insertAfter);

    // Helper to expand all parents of an entity
    void CollectParentChain(Entity entity, std::vector<Entity>& chain);
    void ExpandToEntity(Entity entity);

    // Entity creation functions
    Entity CreateEmptyEntity(const std::string& name = "Empty Entity");
    Entity CreateCubeEntity();
    Entity CreateCameraEntity();

    // Entity duplication (takeSnapshot = true for single entity, false when called from DuplicateEntities)
    Entity DuplicateEntity(Entity sourceEntity, bool takeSnapshot = true);

    // Entity clipboard (for copy/paste) - private helpers

    // Range selection helper
    void SelectRange(Entity fromEntity, Entity toEntity);
    int GetEntityDisplayIndex(Entity entity, const std::vector<Entity>& flatList);
    std::vector<Entity> GetFlatEntityList();
    void CollectEntitiesRecursive(Entity entity, std::vector<Entity>& flatList);

    // Last clicked entity for shift-click range selection
    Entity lastClickedEntity = static_cast<Entity>(-1);

    // Rename functionality
    Entity renamingEntity = static_cast<Entity>(-1);
    std::vector<char> renameBuffer;
    bool startRenaming = false;

    // Force refresh mechanism
    bool needsRefresh = false;

    // Track expanded entities for Unity-like auto-expand behavior
    std::set<Entity> expandedEntities;
    std::set<Entity> forceExpandedEntities;
    Entity lastSelectedEntity = static_cast<Entity>(-1);

    // Helper to expand all parents of an entity
    void ExpandParentsOfEntity(Entity entity);

    // Helper to recursively delete an entity and all its children
    void DeleteEntityWithChildren(Entity entity, bool cleanupParentRef = true);

};