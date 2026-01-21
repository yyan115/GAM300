#pragma once
#include "EditorPanel.hpp"
#include <string>
#include <ECS/ECSRegistry.hpp>
#include <ECS/ECSManager.hpp>

class PrefabEditor
{
public:
    PrefabEditor() = default;

    // Call this each time you want to edit another prefab
    //void SetPrefabPath(const std::string& path);
    //void OnImGuiRender() override;

	static bool IsInPrefabEditorMode() { return isInPrefabEditorMode; }
	static bool HasUnsavedChanges() { return hasUnsavedChanges; }
	static void SetUnsavedChanges(bool unsaved) { hasUnsavedChanges = unsaved; }

	static Entity GetSandboxEntity() { return sandboxEntity; }
    static void StartEditingPrefab(Entity prefab, const std::string& _prefabPath);
    static void StopEditingPrefab();
    static void SaveEditedPrefab();

private:
    // Loads prefab into the sandbox ECS (isolated from the live scene)
    void LoadPrefabSandbox();
    // Pushes saved prefab to live instances (uses active ECS)
    void PropagateToInstances();

    // --- sandbox "Prefab Mode" state ---
    ECSManager sandboxECS{};                   // isolated world just for editing
    static Entity sandboxEntity;

    static std::string prefabPath;
    static bool isInPrefabEditorMode;
	static bool hasUnsavedChanges;

	static std::vector<Entity> previouslyActiveEntities;
};

//// ---- helper API (no PanelManager changes needed) ----
//namespace PrefabEditor
//{
//    // Opens (or focuses) the Prefab Editor and loads `path`.
//    void Open(const std::string& path);
//}