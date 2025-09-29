#pragma once

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

    /**
     * @brief Render the inspector panel's ImGui content.
     */
    void OnImGuiRender() override;

private:
    void DrawNameComponent(Entity entity);
    void DrawTransformComponent(Entity entity);
    void DrawModelRenderComponent(Entity entity);
    void DrawAudioComponent(Entity entity);
    void DrawSelectedAsset(const GUID_128& assetGuid);
    void ApplyMaterialToModel(Entity entity, const GUID_128& materialGuid);
    void ApplyMaterialToModelByPath(Entity entity, const std::string& materialPath);

    // Lock functionality
    bool inspectorLocked = false;
    Entity lockedEntity = static_cast<Entity>(-1);
    GUID_128 lockedAsset = {0, 0};

    // Cache for currently edited material to persist changes across frames
    std::shared_ptr<Material> cachedMaterial;
    std::string cachedMaterialPath;
    GUID_128 cachedMaterialGuid = {0, 0};
};