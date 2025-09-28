#pragma once

#include "EditorPanel.hpp"
#include <ECS/ECSRegistry.hpp>
#include <ECS/NameComponent.hpp>
#include <ECS/Entity.hpp>
#include <Transform/TransformComponent.hpp>
#include <Transform/TransformSystem.hpp>
#include "../GUIManager.hpp"

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
};