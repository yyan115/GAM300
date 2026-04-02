#ifndef PREFABEDITOR_ENABLE_PROPAGATION
#define PREFABEDITOR_ENABLE_PROPAGATION 1
#endif

#include "pch.h"
#include "Panels/PrefabEditorPanel.hpp"
#include "imgui.h"
#include <imgui_internal.h>

#include <filesystem>
#include <algorithm>
#include <new> // placement new

#include <ECS/ECSRegistry.hpp>
#include <ECS/ECSManager.hpp>
#include "PrefabComponent.hpp"
#include "Prefab/PrefabLinkComponent.hpp"
#include <ECS/NameComponent.hpp>
#include <Transform/TransformComponent.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include "GUIManager.hpp"
#include "Prefab/PrefabIO.hpp"
#include "Logging.hpp"
#include "Asset Manager/AssetManager.hpp"
#include <Panels/InspectorPanel.hpp>
#include <Panels/ScenePanel.hpp>
#include <ECS/ActiveComponent.hpp>
#include <Scene/SceneManager.hpp>
#include "Scripting.h"
#include <Graphics/Lights/LightComponent.hpp>

bool PrefabEditor::isInPrefabEditorMode = false;
//bool PrefabEditor::hasUnsavedChanges = false;
Entity PrefabEditor::sandboxEntity = static_cast<Entity>(-1);
Entity PrefabEditor::prefabPreviewLight = static_cast<Entity>(-1);
bool PrefabEditor::previewLightEnabled = true;
std::string PrefabEditor::prefabPath{};
//std::vector<Entity> PrefabEditor::previouslyActiveEntities{};

// trait: does T have .overrideFromPrefab ?
template <typename, typename = void> struct has_override_flag : std::false_type {};
template <typename T>
struct has_override_flag<T, std::void_t<decltype(std::declval<T&>().overrideFromPrefab)>> : std::true_type {};

namespace {
    inline std::string NormalizePath(const std::string& p)
    {
        try {
            std::filesystem::path canon = std::filesystem::weakly_canonical(p);
            std::string s = canon.generic_string();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        }
        catch (...) {
            std::string s = std::filesystem::path(p).generic_string();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        }
    }

    static std::string CanonicalPrefabPath(const std::string& p) {
        std::error_code ec;
        std::filesystem::path canon = std::filesystem::weakly_canonical(p, ec);
        return (ec ? std::filesystem::path(p) : canon).generic_string();
    }

    static bool IsEntityPresentInECS(ECSManager& ecs, Entity entity)
    {
        const auto allEntities = ecs.GetAllEntities();
        return std::find(allEntities.begin(), allEntities.end(), entity) != allEntities.end();
    }

}

//PrefabEditorPanel::PrefabEditorPanel()
//    : EditorPanel("Prefab Editor", /*open=*/true)
//{
//}

//void PrefabEditorPanel::SetPrefabPath(const std::string& path)
//{
//    prefabPath = path;
//    isOpen = true;
//
//    GUIManager::SetSelectedAsset(GUID_128{});
//
//    LoadPrefabSandbox();
//}

void PrefabEditor::StartEditingPrefab(const std::string& _prefabPath)
{
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // Save the current scene state to a temp file. It will be restored back when we stop editing the prefab.
    if (!IsInPrefabEditorMode()) {
        SceneManager::GetInstance().SaveTempScene();
    }
    else {
        // If we were previously editing another prefab, save the previous prefab changes first.
        PrefabEditor::SaveEditedPrefab();
    }

    // The preview light (if any) will be destroyed by ClearAllEntities below.
    prefabPreviewLight = static_cast<Entity>(-1);

    // Clear all entities in the current scene.
    ecs.ClearAllEntities();

    // Instantiate the prefab to edit.
    Entity prefab = InstantiatePrefabFromFile(_prefabPath, false, true);
    GUIManager::SetSelectedEntity(prefab);

    //if (sandboxEntity != static_cast<Entity>(-1)) {
    //    ecs.DestroyEntity(sandboxEntity);
    //}

    sandboxEntity = prefab;
    prefabPath = _prefabPath;
    isInPrefabEditorMode = true;

    // Frame the prefab in the scene camera
    if (ecs.HasComponent<Transform>(sandboxEntity)) {
        Transform& transform = ecs.GetComponent<Transform>(sandboxEntity);
        ecs.transformSystem->UpdateTransform(sandboxEntity);
        glm::vec3 entityPos(transform.worldMatrix.m.m03,
            transform.worldMatrix.m.m13,
            transform.worldMatrix.m.m23);

        // Determine if entity is 2D or 3D
        bool entityIs3D = true;
        bool hasSprite = ecs.HasComponent<SpriteRenderComponent>(sandboxEntity);
        bool hasText = ecs.HasComponent<TextRenderComponent>(sandboxEntity);
        bool hasModel = ecs.HasComponent<ModelRenderComponent>(sandboxEntity);

        if (hasModel) {
            entityIs3D = true;
        }
        else if (hasSprite) {
            auto& sprite = ecs.GetComponent<SpriteRenderComponent>(sandboxEntity);
            entityIs3D = sprite.is3D;
            // Always use transform.worldMatrix for position - sprite.position is not kept updated
        }
        else if (hasText) {
            auto& text = ecs.GetComponent<TextRenderComponent>(sandboxEntity);
            entityIs3D = text.is3D;
        }

        // Switch view mode to match entity if needed
        EditorState& editorState = EditorState::GetInstance();
        bool currentIs2D = editorState.Is2DMode();
        bool targetIs2D = !entityIs3D;

        if (currentIs2D != targetIs2D) {
            EditorState::ViewMode newViewMode = entityIs3D ? EditorState::ViewMode::VIEW_3D : EditorState::ViewMode::VIEW_2D;
            editorState.SetViewMode(newViewMode);
            GraphicsManager::ViewMode gfxMode = entityIs3D ? GraphicsManager::ViewMode::VIEW_3D : GraphicsManager::ViewMode::VIEW_2D;
            GraphicsManager::GetInstance().SetViewMode(gfxMode);
        }

        auto scenePanelPtr = GUIManager::GetPanelManager().GetPanel("Scene");
        if (scenePanelPtr) {
            auto scenePanel = std::dynamic_pointer_cast<ScenePanel>(scenePanelPtr);
            if (scenePanel) {
                scenePanel->SetCameraTarget(entityPos);
            }
        }
    }

 //   std::set<Entity> prefabEntities({ prefab });
	//const auto& prefabChildEntities = ecs.transformSystem->GetAllChildEntitiesSet(prefab);
 //   prefabEntities.insert(prefabChildEntities.begin(), prefabChildEntities.end());

	//previouslyActiveEntities = ecs.GetActiveEntities();

 //   // Set all other entities as inactive.
 //   for (const auto& e : ecs.GetActiveEntities()) {
 //       if (prefabEntities.find(e) == prefabEntities.end()) {
 //           ecs.GetComponent<ActiveComponent>(e).isActive = false;
 //       }
	//}

    // Preview light is created lazily by SyncPreviewLightToSceneCamera() and follows
    // the editor Scene camera, so there is no static prefab-root light.

    // Reload scripts so the inspector can create preview instances for script components.
    // ClearAllEntities() above invalidates the module cache; without this, CreateInstanceFromFile
    // fails and no script fields are shown until the user presses the hot reload button manually.
    Scripting::RequestReloadNow();
    if (Scripting::GetLuaState()) Scripting::Tick(0.0f);
}

void PrefabEditor::StopEditingPrefab() {
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // Remove the preview light before saving so it is never written into the prefab file.
    if (prefabPreviewLight != static_cast<Entity>(-1)) {
        ecs.DestroyEntity(prefabPreviewLight);
        prefabPreviewLight = static_cast<Entity>(-1);
    }

    SaveEntityToPrefabFile(ecs, AssetManager::GetInstance(), sandboxEntity, prefabPath);

	ecs.DestroyEntity(sandboxEntity);

	sandboxEntity = static_cast<Entity>(-1);
    prefabPath = "";
	isInPrefabEditorMode = false;

	// Restore the previous scene state from the temp file.
    // The temp scene was saved when prefab editing started, so it contains
    // any instance overrides the user had made before editing the prefab.
    // When this scene is next loaded or saved, it will pick up the updated
    // prefab file and apply the stored overrides on top.
	SceneManager::GetInstance().ReloadTempScene();

    GUIManager::ClearSelectedEntities();
}

void PrefabEditor::SaveEditedPrefab() {
    std::cout << "[PrefabEditor] Saving prefab..." << std::endl;
    SaveEntityToPrefabFile(ECSRegistry::GetInstance().GetActiveECSManager(), AssetManager::GetInstance(), sandboxEntity, prefabPath);
}

void PrefabEditor::SetPreviewLightEnabled(bool enabled)
{
    previewLightEnabled = enabled;

    if (!isInPrefabEditorMode) {
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!previewLightEnabled) {
        if (prefabPreviewLight != static_cast<Entity>(-1) && IsEntityPresentInECS(ecs, prefabPreviewLight)) {
            ecs.DestroyEntity(prefabPreviewLight);
        }
        prefabPreviewLight = static_cast<Entity>(-1);
        return;
    }

    if (prefabPreviewLight == static_cast<Entity>(-1) || !IsEntityPresentInECS(ecs, prefabPreviewLight)) {
        return;
    }

    if (ecs.HasComponent<PointLightComponent>(prefabPreviewLight)) {
        ecs.GetComponent<PointLightComponent>(prefabPreviewLight).enabled = previewLightEnabled;
    }
    if (ecs.HasComponent<ActiveComponent>(prefabPreviewLight)) {
        ecs.GetComponent<ActiveComponent>(prefabPreviewLight).isActive = previewLightEnabled;
    }
}

void PrefabEditor::SyncPreviewLightToSceneCamera(const glm::vec3& cameraPosition,
                                                 const glm::vec3& cameraForward,
                                                 const glm::vec3& cameraUp)
{
    if (!isInPrefabEditorMode) {
        return;
    }

    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!previewLightEnabled) {
        if (prefabPreviewLight != static_cast<Entity>(-1) && IsEntityPresentInECS(ecs, prefabPreviewLight)) {
            ecs.DestroyEntity(prefabPreviewLight);
        }
        prefabPreviewLight = static_cast<Entity>(-1);
        return;
    }

    if (prefabPreviewLight == static_cast<Entity>(-1) || !IsEntityPresentInECS(ecs, prefabPreviewLight)) {
        Entity lightEntity = ecs.CreateEntity();
        if (lightEntity == static_cast<Entity>(-1)) {
            return;
        }

        if (ecs.HasComponent<NameComponent>(lightEntity)) {
            ecs.GetComponent<NameComponent>(lightEntity).name = "__PrefabPreviewLight__";
        }

        if (ecs.HasComponent<ActiveComponent>(lightEntity)) {
            ecs.GetComponent<ActiveComponent>(lightEntity).isActive = previewLightEnabled;
        }

        PointLightComponent previewLight;
        previewLight.enabled = previewLightEnabled;
        previewLight.intensity = 3.0f;
        previewLight.range = 30.0f;
        previewLight.linear = 0.022f;
        previewLight.quadratic = 0.0019f;
        previewLight.diffuse = Vector3D(1.0f, 0.98f, 0.92f);
        previewLight.ambient = Vector3D(0.55f, 0.55f, 0.55f);
        ecs.AddComponent<PointLightComponent>(lightEntity, previewLight);

        prefabPreviewLight = lightEntity;
    }

    if (ecs.HasComponent<ActiveComponent>(prefabPreviewLight)) {
        ecs.GetComponent<ActiveComponent>(prefabPreviewLight).isActive = true;
    }

    if (ecs.HasComponent<PointLightComponent>(prefabPreviewLight)) {
        ecs.GetComponent<PointLightComponent>(prefabPreviewLight).enabled = previewLightEnabled;
    }

    if (!previewLightEnabled || !ecs.HasComponent<Transform>(prefabPreviewLight)) {
        return;
    }

    const glm::vec3 offsetPosition = cameraPosition + cameraForward * 0.4f + cameraUp * 0.15f;
    Transform& lightTransform = ecs.GetComponent<Transform>(prefabPreviewLight);
    lightTransform.localPosition = Vector3D(offsetPosition.x, offsetPosition.y, offsetPosition.z);
    lightTransform.isDirty = true;
    ecs.transformSystem->UpdateTransform(prefabPreviewLight);
}

//void PrefabEditorPanel::OnImGuiRender()
//{
//    if (!isOpen) return;
//
//    if (ImGui::Begin(name.c_str(), &isOpen))
//    {
//        ImGui::TextUnformatted(prefabPath.c_str());
//        ImGui::Separator();
//
//        if (sandboxEntity != static_cast<Entity>(-1))
//        {
//            // Use ReflectionRenderer to display all components automatically
//			InspectorPanel::DrawComponentsViaReflection(sandboxEntity);
//        }
//        else
//        {
//            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No entity loaded");
//        }
//
//        ImGui::Separator();
//
//        if (ImGui::Button("Save Prefab"))
//        {
//            if (sandboxEntity == static_cast<Entity>(-1)) {
//                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PrefabEditor] No sandbox entity to save.\n");
//            }
//            else if (SaveEntityToPrefabFile(
//                ECSRegistry::GetInstance().GetActiveECSManager(),
//                AssetManager::GetInstance(),
//                sandboxEntity,
//                prefabPath))
//            {
//                ENGINE_PRINT("[PrefabEditor] Saved: ", prefabPath, "\n");
//#if PREFABEDITOR_ENABLE_PROPAGATION
//                PropagateToInstances();
//#endif
//            }
//            else
//            {
//                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PrefabEditor] Save failed: ", prefabPath, "\n");
//            }
//        }
//
//        ImGui::SameLine();
//        if (ImGui::Button("Revert"))
//        {
//            LoadPrefabSandbox();
//        }
//    }
//    ImGui::End();
//}

void PrefabEditor::LoadPrefabSandbox()
{
    // Reconstruct ECSManager in place (copy/move assignment is deleted)
    sandboxECS.~ECSManager();
    new (&sandboxECS) ECSManager();

    sandboxEntity = InstantiatePrefabFromFile(prefabPath, false, true);

    //if (!sandboxECS.HasComponent<NameComponent>(sandboxEntity))
    //    sandboxECS.AddComponent<NameComponent>(sandboxEntity, NameComponent{});
    //sandboxECS.GetComponent<NameComponent>(sandboxEntity).name = "Prefab (editing)";

    //// Load the prefab *into the sandbox* WITHOUT resolving assets to avoid spam/draw
    //InstantiatePrefabIntoEntity(
    //    sandboxECS,
    //    AssetManager::GetInstance(),
    //    prefabPath,
    //    sandboxEntity,
    //    /*keepExistingPosition=*/false,
    //    /*resolveAssets=*/false);   // < key change
}

#if PREFABEDITOR_ENABLE_PROPAGATION
void PrefabEditor::PropagateToInstances()
{
    PropagateToInstancesForPath(prefabPath);
}

void PrefabEditor::PropagateToInstancesForPath(const std::string& pathToPrefab)
{
    if (pathToPrefab.empty()) {
        return;
    }

    ECSManager& liveECS = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!liveECS.IsComponentTypeRegistered<PrefabLinkComponent>()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[PrefabEditor] PrefabLinkComponent not registered; propagation skipped.\n");
        return;
    }

    const std::string myPath = CanonicalPrefabPath(pathToPrefab);
    const std::string myNorm = NormalizePath(myPath);

    // Collect entities to update first (avoid iterator invalidation)
    std::vector<Entity> entitiesToUpdate;

    for (Entity e : liveECS.GetAllEntities()) {
        if (!liveECS.HasComponent<PrefabLinkComponent>(e)) continue;
        const auto& link = liveECS.GetComponent<PrefabLinkComponent>(e);
        const std::string refNorm = NormalizePath(CanonicalPrefabPath(link.prefabPath));
        if (refNorm != myNorm) continue;

        entitiesToUpdate.push_back(e);
    }

    // Now update each prefab instance
    for (Entity e : entitiesToUpdate) {
        InstantiatePrefabIntoEntity(pathToPrefab, e);
    }

    ENGINE_PRINT("[PrefabEditor] Propagated prefab changes to ", entitiesToUpdate.size(), " instance(s).\n");
}
#endif

//// ------------ helper (no PanelManager modification) ----------------
//namespace PrefabEditor
//{
//    void Open(const std::string& path)
//    {
//        auto& pm = GUIManager::GetPanelManager();
//        if (auto p = pm.GetPanel("Prefab Editor"))
//        {
//            if (auto pref = std::dynamic_pointer_cast<PrefabEditorPanel>(p))
//            {
//                pref->SetPrefabPath(path);
//                return;
//            }
//        }
//
//        auto pref = std::make_shared<PrefabEditorPanel>();
//        pref->SetPrefabPath(path);
//        pm.RegisterPanel(pref);
//    }
//}