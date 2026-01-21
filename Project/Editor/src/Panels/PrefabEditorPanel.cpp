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
#include "PrefabLinkComponent.hpp"
#include <ECS/NameComponent.hpp>
#include <Transform/TransformComponent.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include "GUIManager.hpp"
#include "PrefabIO.hpp"
#include "Logging.hpp"
#include "Asset Manager/AssetManager.hpp"
#include <Panels/InspectorPanel.hpp>
#include <Panels/ScenePanel.hpp>
#include <ECS/ActiveComponent.hpp>

bool PrefabEditor::isInPrefabEditorMode = false;
bool PrefabEditor::hasUnsavedChanges = false;
Entity PrefabEditor::sandboxEntity = static_cast<Entity>(-1);
std::string PrefabEditor::prefabPath{};
std::vector<Entity> PrefabEditor::previouslyActiveEntities{};

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

void PrefabEditor::StartEditingPrefab(Entity prefab, const std::string& _prefabPath)
{
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    sandboxEntity = prefab;
    prefabPath = _prefabPath;
    isInPrefabEditorMode = true;

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

        // Frame the entity in the scene camera
        auto scenePanelPtr = GUIManager::GetPanelManager().GetPanel("Scene");
        if (scenePanelPtr) {
            auto scenePanel = std::dynamic_pointer_cast<ScenePanel>(scenePanelPtr);
            if (scenePanel) {
                scenePanel->SetCameraTarget(entityPos);
            }
        }
    }

    std::set<Entity> prefabEntities({ prefab });
	const auto& prefabChildEntities = ecs.transformSystem->GetAllChildEntitiesSet(prefab);
    prefabEntities.insert(prefabChildEntities.begin(), prefabChildEntities.end());

	previouslyActiveEntities = ecs.GetActiveEntities();

    // Set all other entities as inactive.
    for (const auto& e : ecs.GetActiveEntities()) {
        if (prefabEntities.find(e) == prefabEntities.end()) {
            ecs.GetComponent<ActiveComponent>(e).isActive = false;
        }
	}
}

void PrefabEditor::StopEditingPrefab() {
	sandboxEntity = static_cast<Entity>(-1);
    prefabPath = "";
	isInPrefabEditorMode = false;

    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    // Restore previously active entities.
    for (const auto& e : previouslyActiveEntities) {
        ecs.GetComponent<ActiveComponent>(e).isActive = true;
    }
	previouslyActiveEntities.clear();
	SetUnsavedChanges(false);
}

void PrefabEditor::SaveEditedPrefab() {
    if (SaveEntityToPrefabFile(ECSRegistry::GetInstance().GetActiveECSManager(), AssetManager::GetInstance(), sandboxEntity, prefabPath)) {

    }
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

    sandboxEntity = InstantiatePrefabFromFile(prefabPath);

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
    //    /*resolveAssets=*/false);   // <— key change
}

#if PREFABEDITOR_ENABLE_PROPAGATION
void PrefabEditor::PropagateToInstances()
{
    ECSManager& liveECS = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!liveECS.IsComponentTypeRegistered<PrefabLinkComponent>()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[PrefabEditor] PrefabLinkComponent not registered; propagation skipped.\n");
        return;
    }

    const std::string myPath = CanonicalPrefabPath(prefabPath);
    const std::string myNorm = NormalizePath(myPath);

    for (Entity e : liveECS.GetActiveEntities())
    {
        if (!liveECS.HasComponent<PrefabLinkComponent>(e)) continue;
        const auto& link = liveECS.GetComponent<PrefabLinkComponent>(e);
        const std::string refNorm = NormalizePath(CanonicalPrefabPath(link.prefabPath));
        if (refNorm != myNorm) continue;

        (void)InstantiatePrefabIntoEntity(
            liveECS, AssetManager::GetInstance(), myPath, e,
            /*keepExistingPosition=*/true,
            /*resolveAssets=*/true);
    }
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