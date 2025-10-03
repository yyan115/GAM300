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

PrefabEditorPanel::PrefabEditorPanel()
    : EditorPanel("Prefab Editor", /*open=*/true)
{
}

void PrefabEditorPanel::SetPrefabPath(const std::string& path)
{
    prefabPath = path;
    isOpen = true;

    GUIManager::SetSelectedAsset(GUID_128{});

    LoadPrefabSandbox();

    if (sandboxEntity != static_cast<Entity>(-1)) {
        GUIManager::SetSelectedEntity(sandboxEntity);
    }
}

void PrefabEditorPanel::OnImGuiRender()
{
    if (!isOpen) return;

    if (ImGui::Begin(name.c_str(), &isOpen))
    {
        ImGui::TextUnformatted(prefabPath.c_str());
        ImGui::Separator();

        // Only re-point selection when the user actually clicks inside this window
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (sandboxEntity != static_cast<Entity>(-1))
                GUIManager::SetSelectedEntity(sandboxEntity);
            // Leave asset selection alone; do not clear it every frame.
        }

        if (sandboxEntity != static_cast<Entity>(-1))
        {
            if (sandboxECS.HasComponent<NameComponent>(sandboxEntity)) {
                auto& nc = sandboxECS.GetComponent<NameComponent>(sandboxEntity);
                char buf[128] = {};
                std::snprintf(buf, sizeof(buf), "%s", nc.name.c_str());
                if (ImGui::InputText("Name", buf, sizeof(buf))) nc.name = buf;

                if constexpr (has_override_flag<NameComponent>::value) {
                    ImGui::Checkbox("Override From Prefab##Name", &nc.overrideFromPrefab);
                }
            }
            else {
                sandboxECS.AddComponent<NameComponent>(sandboxEntity, NameComponent{});
                sandboxECS.GetComponent<NameComponent>(sandboxEntity).name = "Prefab (editing)";
            }

            if (sandboxECS.HasComponent<Transform>(sandboxEntity)) {
                auto& t = sandboxECS.GetComponent<Transform>(sandboxEntity);
                ImGui::DragFloat3("Position", &t.localPosition.x, 0.01f);
                ImGui::DragFloat3("Rotation", &t.localRotation.x, 0.5f);
                ImGui::DragFloat3("Scale", &t.localScale.x, 0.01f);
                t.isDirty = true;
                if constexpr (has_override_flag<Transform>::value) {
                    ImGui::Checkbox("Override From Prefab##Transform", &t.overrideFromPrefab);
                }
            }
            else {
                sandboxECS.AddComponent<Transform>(sandboxEntity, Transform{});
            }

            if (sandboxECS.HasComponent<ModelRenderComponent>(sandboxEntity)) {
                auto& m = sandboxECS.GetComponent<ModelRenderComponent>(sandboxEntity);
                ImGui::Separator();
                ImGui::TextUnformatted("Model Render (GUIDs only; sandbox does not resolve assets)");
                ImGui::Text("Model GUID:  %s", (m.modelGUID.high || m.modelGUID.low) ? "set" : "empty");
                ImGui::Text("Shader GUID: %s", (m.shaderGUID.high || m.shaderGUID.low) ? "set" : "empty");
                if constexpr (has_override_flag<ModelRenderComponent>::value) {
                    ImGui::Checkbox("Override From Prefab", &m.overrideFromPrefab);
                }
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Save Prefab"))
        {
            if (sandboxEntity == static_cast<Entity>(-1)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PrefabEditor] No sandbox entity to save.\n");
            }
            else if (SaveEntityToPrefabFile(
                sandboxECS,
                AssetManager::GetInstance(),
                sandboxEntity,
                prefabPath))
            {
                ENGINE_PRINT("[PrefabEditor] Saved: ", prefabPath, "\n");
#if PREFABEDITOR_ENABLE_PROPAGATION
                PropagateToInstances();
#endif
            }
            else
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[PrefabEditor] Save failed: ", prefabPath, "\n");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            LoadPrefabSandbox();
        }
    }
    ImGui::End();
}

void PrefabEditorPanel::LoadPrefabSandbox()
{
    // Reconstruct ECSManager in place (copy/move assignment is deleted)
    sandboxECS.~ECSManager();
    new (&sandboxECS) ECSManager();

    sandboxEntity = sandboxECS.CreateEntity();

    if (!sandboxECS.HasComponent<NameComponent>(sandboxEntity))
        sandboxECS.AddComponent<NameComponent>(sandboxEntity, NameComponent{});
    sandboxECS.GetComponent<NameComponent>(sandboxEntity).name = "Prefab (editing)";

    // Load the prefab *into the sandbox* WITHOUT resolving assets to avoid spam/draw
    InstantiatePrefabIntoEntity(
        sandboxECS,
        AssetManager::GetInstance(),
        prefabPath,
        sandboxEntity,
        /*keepExistingPosition=*/false,
        /*resolveAssets=*/false);   // <— key change
}

#if PREFABEDITOR_ENABLE_PROPAGATION
void PrefabEditorPanel::PropagateToInstances()
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

// ------------ helper (no PanelManager modification) ----------------
namespace PrefabEditor
{
    void Open(const std::string& path)
    {
        auto& pm = GUIManager::GetPanelManager();
        if (auto p = pm.GetPanel("Prefab Editor"))
        {
            if (auto pref = std::dynamic_pointer_cast<PrefabEditorPanel>(p))
            {
                pref->SetPrefabPath(path);
                return;
            }
        }

        auto pref = std::make_shared<PrefabEditorPanel>();
        pref->SetPrefabPath(path);
        pm.RegisterPanel(pref);
    }
}