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
    void DrawTagComponent(Entity entity);
    void DrawLayerComponent(Entity entity);
    void DrawModelRenderComponent(Entity entity);
	void DrawBrainComponent(Entity entity);

    // Generic reflection-based rendering
    void DrawComponentGeneric(void* componentPtr, const char* componentTypeName, Entity entity);
    void DrawComponentsViaReflection(Entity entity);

    void DrawSelectedAsset(const GUID_128& assetGuid);
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