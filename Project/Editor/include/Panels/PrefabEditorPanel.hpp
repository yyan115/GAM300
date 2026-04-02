#pragma once
#include "EditorPanel.hpp"
#include <string>
#include <glm/vec3.hpp>
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
	//static bool HasUnsavedChanges() { return hasUnsavedChanges; }
	//static void SetUnsavedChanges(bool unsaved) { hasUnsavedChanges = unsaved; }

	static Entity GetSandboxEntity() { return sandboxEntity; }
    static void StartEditingPrefab(const std::string& _prefabPath);
    static void StopEditingPrefab();
    static void SaveEditedPrefab();
    static void SetPreviewLightEnabled(bool enabled);
    static bool IsPreviewLightEnabled() { return previewLightEnabled; }
    static void SyncPreviewLightToSceneCamera(const glm::vec3& cameraPosition,
                                              const glm::vec3& cameraForward,
                                              const glm::vec3& cameraUp);

private:
    // Loads prefab into the sandbox ECS (isolated from the live scene)
    void LoadPrefabSandbox();
    // Pushes saved prefab to live instances (uses active ECS)
    static void PropagateToInstances();
    // Pushes saved prefab to live instances for a specific prefab path
    static void PropagateToInstancesForPath(const std::string& pathToPrefab);

    // --- sandbox "Prefab Mode" state ---
    ECSManager sandboxECS{};                   // isolated world just for editing
    static Entity sandboxEntity;
    static Entity prefabPreviewLight;          // temporary point light for visibility in prefab editing
    static bool previewLightEnabled;

    static std::string prefabPath;
    static bool isInPrefabEditorMode;
	//static bool hasUnsavedChanges;

	//static std::vector<Entity> previouslyActiveEntities;
};

//// ---- helper API (no PanelManager changes needed) ----
//namespace PrefabEditor
//{
//    // Opens (or focuses) the Prefab Editor and loads `path`.
//    void Open(const std::string& path);
//}