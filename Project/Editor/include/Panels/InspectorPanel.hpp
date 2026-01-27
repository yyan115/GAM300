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
#include <FileWatch.hpp>

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

    struct ComponentResetRequest {
        Entity entity;
        std::string componentType;
    };

    /**
     * @brief Render the inspector panel's ImGui content.
     */
    void OnImGuiRender() override;
    static void DrawComponentsViaReflection(Entity entity);

private:
    void DrawTagComponent(Entity entity);
    void DrawLayerComponent(Entity entity);
    void DrawModelRenderComponent(Entity entity);
	void DrawBrainComponent(Entity entity);

    // Generic reflection-based rendering
    static void DrawComponentGeneric(void* componentPtr, const char* componentTypeName, Entity entity);

    // Multi-entity editing
    void DrawMultiEntityInspector(const std::vector<Entity>& entities);
    void DrawSharedComponentsHeader(const std::vector<Entity>& entities);
    std::vector<std::string> GetSharedComponentTypes(const std::vector<Entity>& entities);
    void DrawSharedComponentGeneric(const std::vector<Entity>& entities, const std::string& componentType);
    bool HasComponent(Entity entity, const std::string& componentType);
    void* GetComponentPtr(Entity entity, const std::string& componentType);

    void DrawSelectedAsset(const GUID_128& assetGuid);
    void ApplyModelToRenderer(Entity entity, const GUID_128& modelGuid, const std::string& modelPath);
    static bool DrawComponentHeaderWithRemoval(const char* label, Entity entity, const std::string& componentType, void* componentPtr = nullptr, ImGuiTreeNodeFlags flags = 0);
    void ProcessPendingComponentRemovals();
    void ProcessPendingComponentResets();

    // Component addition functionality
    void DrawAddComponentButton(Entity entity);
    void AddComponent(Entity entity, const std::string& componentType);

    // File watcher callback
    void OnScriptFileChanged(const std::string& path, const filewatch::Event& event);

    // Search state for add component
    char componentSearchBuffer[256] = "";
    bool componentSearchActive = false;

    // Tree reset state for add component popup
    bool resetComponentTrees = false;

    // Lock functionality
    bool inspectorLocked = false;
    Entity lockedEntity = static_cast<Entity>(-1);

    // Component removal queue (processed after ImGui rendering)
    static std::vector<ComponentRemovalRequest> pendingComponentRemovals;
    static std::vector<ComponentResetRequest> pendingComponentResets;
    GUID_128 lockedAsset = {0, 0};

    // Cache for currently edited material to persist changes across frames
    std::shared_ptr<Material> cachedMaterial;
    std::string cachedMaterialPath;
    GUID_128 cachedMaterialGuid = {0, 0};

    // File watcher for scripts
    std::unique_ptr<filewatch::FileWatch<std::string>> scriptFileWatcher;

    // Script cache
    std::vector<std::string> cachedScripts;
};