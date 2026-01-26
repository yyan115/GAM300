#pragma once
#include "EditorPanel.hpp"
#include <string>
#include <ECS/ECSRegistry.hpp>
#include <ECS/ECSManager.hpp>

class PrefabEditorPanel : public EditorPanel
{
public:
    PrefabEditorPanel();

    // Call this each time you want to edit another prefab
    void SetPrefabPath(const std::string& path);
    void OnImGuiRender() override;

private:
    // Loads prefab into the sandbox ECS (isolated from the live scene)
    void LoadPrefabSandbox();
    // Pushes saved prefab to live instances (uses active ECS)
    void PropagateToInstances();

    // --- sandbox "Prefab Mode" state ---
    ECSManager sandboxECS{};                   // isolated world just for editing
    Entity     sandboxEntity = static_cast<Entity>(-1);

    std::string prefabPath;
};

// ---- helper API (no PanelManager changes needed) ----
namespace PrefabEditor
{
    // Opens (or focuses) the Prefab Editor and loads `path`.
    void Open(const std::string& path);
}