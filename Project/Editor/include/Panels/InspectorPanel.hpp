#pragma once

#include "imgui.h"
#include "EditorPanel.hpp"
#include <ECS/ECSRegistry.hpp>
#include <ECS/NameComponent.hpp>
#include <ECS/Entity.hpp>
#include <Transform/TransformComponent.hpp>
#include <Transform/TransformSystem.hpp>
#include "Utilities/GUID.hpp"
#include "../GUIManager.hpp"
#include <Graphics/Material.hpp>
#include "MaterialInspector.hpp"

/**
 * @brief Inspector panel for viewing and editing properties of selected objects.
 * 
 * This panel displays detailed information and editable properties for the currently
 * selected entity or object, similar to Unity's Inspector window.
 */
class InspectorPanel : public EditorPanel {
public:
    InspectorPanel();
    virtual ~InspectorPanel() = default;

    struct ComponentRemovalRequest {
        Entity entity;
        std::string componentType;
    };

    /**
     * @brief Render the inspector panel's ImGui content.
     */
    void OnImGuiRender() override;

private:
    void DrawNameComponent(Entity entity);
    void DrawTransformComponent(Entity entity);
    void DrawModelRenderComponent(Entity entity);
    void DrawSpriteRenderComponent(Entity entity);
    void DrawAudioComponent(Entity entity);
    void DrawLightComponents(Entity entity);
    void DrawSelectedAsset(const GUID_128& assetGuid);
    void ApplyMaterialToModel(Entity entity, const GUID_128& materialGuid);
    void ApplyMaterialToModelByPath(Entity entity, const std::string& materialPath);
    void ApplyModelToRenderer(Entity entity, const GUID_128& modelGuid, const std::string& modelPath);
    bool DrawComponentHeaderWithRemoval(const char* label, Entity entity, const std::string& componentType, ImGuiTreeNodeFlags flags = 0);
    void ProcessPendingComponentRemovals();

    // Component addition functionality
    void DrawAddComponentButton(Entity entity);
    void AddComponent(Entity entity, const std::string& componentType);

    // Lock functionality
    bool inspectorLocked = false;
    Entity lockedEntity = static_cast<Entity>(-1);

    // Component removal queue (processed after ImGui rendering)
    std::vector<ComponentRemovalRequest> pendingComponentRemovals;
    GUID_128 lockedAsset = {0, 0};

    // Cache for currently edited material to persist changes across frames
    std::shared_ptr<Material> cachedMaterial;
    std::string cachedMaterialPath;
    GUID_128 cachedMaterialGuid = {0, 0};
};