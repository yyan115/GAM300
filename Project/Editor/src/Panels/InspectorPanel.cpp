#include "Panels/InspectorPanel.hpp"
#include "imgui.h"
#include "GUIManager.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Texture.h>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>

// Global drag-drop state for cross-window material dragging (declared in AssetBrowserPanel.cpp)
extern GUID_128 g_draggedMaterialGuid;
extern std::string g_draggedMaterialPath;
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <Sound/AudioComponent.hpp>
#include <Sound/AudioSystem.hpp>

InspectorPanel::InspectorPanel()
    : EditorPanel("Inspector", true)
{
}

void InspectorPanel::OnImGuiRender()
{
    if (ImGui::Begin(name.c_str(), &isOpen))
    {
        // Check for selected asset first (higher priority)
        GUID_128 selectedAsset = GUIManager::GetSelectedAsset();

        // Lock button in the title bar (always visible)
        ImGui::SameLine(ImGui::GetWindowWidth() - 70);
        if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK " Lock" : ICON_FA_UNLOCK " Unlock", ImVec2(65, 0)))
        {
            inspectorLocked = !inspectorLocked;
            if (inspectorLocked)
            {
                // Lock to current content (entity or asset)
                if (selectedAsset.high != 0 || selectedAsset.low != 0)
                {
                    lockedAsset = selectedAsset;
                    lockedEntity = static_cast<Entity>(-1);
                }
                else
                {
                    lockedEntity = GUIManager::GetSelectedEntity();
                    lockedAsset = {0, 0};
                }
            }
            else
            {
                // Unlock
                lockedEntity = static_cast<Entity>(-1);
                lockedAsset = {0, 0};
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
        }

        // Determine what to display based on lock state
        Entity displayEntity = static_cast<Entity>(-1);
        GUID_128 displayAsset = {0, 0};

        if (inspectorLocked)
        {
            // Show locked content
            if (lockedEntity != static_cast<Entity>(-1))
            {
                displayEntity = lockedEntity;
            }
            else if (lockedAsset.high != 0 || lockedAsset.low != 0)
            {
                displayAsset = lockedAsset;
            }
        }
        else
        {
            // Show current selection
            if (selectedAsset.high != 0 || selectedAsset.low != 0)
            {
                displayAsset = selectedAsset;
            }
            else
            {
                displayEntity = GUIManager::GetSelectedEntity();
            }
        }

        // Validate locked content
        if (inspectorLocked)
        {
            if (lockedEntity != static_cast<Entity>(-1))
            {
                try
                {
                    ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                    auto activeEntities = ecsManager.GetActiveEntities();
                    bool entityExists = std::find(activeEntities.begin(), activeEntities.end(), lockedEntity) != activeEntities.end();
                    if (!entityExists)
                    {
                        // Locked entity no longer exists, unlock
                        inspectorLocked = false;
                        lockedEntity = static_cast<Entity>(-1);
                        lockedAsset = {0, 0};
                        displayEntity = GUIManager::GetSelectedEntity();
                        displayAsset = GUIManager::GetSelectedAsset();
                    }
                }
                catch (...)
                {
                    // If there's any error, unlock
                    inspectorLocked = false;
                    lockedEntity = static_cast<Entity>(-1);
                    lockedAsset = {0, 0};
                    displayEntity = GUIManager::GetSelectedEntity();
                    displayAsset = GUIManager::GetSelectedAsset();
                }
            }
            // Note: Asset validation could be added here if needed
        }

        // Display content
        if (displayAsset.high != 0 || displayAsset.low != 0)
        {
            DrawSelectedAsset(displayAsset);
        }
        else
        {
            // Clear cached material when no asset is selected
            if (cachedMaterial)
            {
                std::cout << "[Inspector] Clearing cached material" << std::endl;
                cachedMaterial.reset();
                cachedMaterialGuid = {0, 0};
                cachedMaterialPath.clear();
            }

            if (displayEntity == static_cast<Entity>(-1))
            {
                ImGui::Text("No object selected");
                ImGui::Text("Select an object in the Scene Hierarchy or an asset in the Asset Browser to view its properties");
                if (inspectorLocked)
                {
                    ImGui::Text("Inspector is locked but no valid content is selected.");
                }
            }
            else
            {
                try
                {
                    // Get the active ECS manager
                    ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

                    ImGui::Text("Entity ID: %u%s", displayEntity, inspectorLocked ? " 🔒" : "");
                    ImGui::Separator();

                    // Draw NameComponent if it exists
                    if (ecsManager.HasComponent<NameComponent>(displayEntity))
                    {
                        DrawNameComponent(displayEntity);
                        ImGui::Separator();
                    }

                    // Draw Transform component if it exists
                    if (ecsManager.HasComponent<Transform>(displayEntity))
                    {
                        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            DrawTransformComponent(displayEntity);
                        }
                    }

                    // Draw ModelRenderComponent if it exists
                    if (ecsManager.HasComponent<ModelRenderComponent>(displayEntity))
                    {
                        if (ImGui::CollapsingHeader("Model Renderer"))
                        {
                            DrawModelRenderComponent(displayEntity);
                        }
                    }

                    // Draw AudioComponent if present
                    if (ecsManager.HasComponent<AudioComponent>(selectedEntity))
                    {
                        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            DrawAudioComponent(selectedEntity);
                        }
                    }
                }
                catch (const std::exception &e)
                {
                    ImGui::Text("Error accessing entity: %s", e.what());
                }
            }
        }
    }
    ImGui::End();
}

void InspectorPanel::DrawNameComponent(Entity entity)
{
    try
    {
        ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        NameComponent &nameComponent = ecsManager.GetComponent<NameComponent>(entity);

        ImGui::PushID("NameComponent");

        // Use a static map to maintain state per entity
        static std::unordered_map<Entity, std::vector<char>> nameBuffers;

        // Get or create buffer for this entity
        auto &nameBuffer = nameBuffers[entity];

        // Initialize buffer if empty or different from component
        std::string currentName = nameComponent.name;
        if (nameBuffer.empty() || std::string(nameBuffer.data()) != currentName)
        {
            nameBuffer.clear();
            nameBuffer.resize(256, '\0'); // Create 256-char buffer filled with null terminators
            if (!currentName.empty() && currentName.length() < 255)
            {
                std::copy(currentName.begin(), currentName.end(), nameBuffer.begin());
            }
        }

        // Use InputText with char buffer
        ImGui::Text("Name");
        ImGui::SameLine();
        if (ImGui::InputText("##Name", nameBuffer.data(), nameBuffer.size()))
        {
            // Update the actual component
            nameComponent.name = std::string(nameBuffer.data());
        }

        ImGui::PopID();
    }
    catch (const std::exception &e)
    {
        ImGui::Text("Error accessing NameComponent: %s", e.what());
    }
}

void InspectorPanel::DrawTransformComponent(Entity entity)
{
    try
    {
        ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        Transform &transform = ecsManager.GetComponent<Transform>(entity);

        ImGui::PushID("Transform");

        // Position
        float position[3] = {transform.localPosition.x, transform.localPosition.y, transform.localPosition.z};
        ImGui::Text("Position");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Position", position, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
        {
            ecsManager.transformSystem->SetLocalPosition(entity, {position[0], position[1], position[2]});
        }

        // Rotation
        Vector3D rotationEuler = transform.localRotation.ToEulerDegrees();
        float rotation[3] = {rotationEuler.x, rotationEuler.y, rotationEuler.z};
        ImGui::Text("Rotation");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Rotation", rotation, 1.0f, -180.0f, 180.0f, "%.1f"))
        {
            ecsManager.transformSystem->SetLocalRotation(entity, {rotation[0], rotation[1], rotation[2]});
        }

        // Scale
        float scale[3] = {transform.localScale.x, transform.localScale.y, transform.localScale.z};
        ImGui::Text("Scale");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Scale", scale, 0.1f, 0.001f, FLT_MAX, "%.3f"))
        {
            ecsManager.transformSystem->SetLocalScale(entity, {scale[0], scale[1], scale[2]});
        }

        ImGui::PopID();
    }
    catch (const std::exception &e)
    {
        ImGui::Text("Error accessing Transform: %s", e.what());
    }
}

void InspectorPanel::DrawModelRenderComponent(Entity entity)
{
    try
    {
        ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        ModelRenderComponent &modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        ImGui::PushID("ModelRenderComponent");

        // Display model info (read-only for now)
        ImGui::Text("Model Renderer Component");
        if (modelRenderer.model)
        {
            ImGui::Text("Model: Loaded (%zu meshes)", modelRenderer.model->meshes.size());
        }
        else
        {
            ImGui::Text("Model: None");
        }

        if (modelRenderer.shader)
        {
            ImGui::Text("Shader: Loaded");
        }
        else
        {
            ImGui::Text("Shader: None");
        }

        // Material section with drag and drop
        ImGui::Separator();
        ImGui::Text("Material");

        // Show current material for this entity
        ImGui::Text("Current Material:");
        std::shared_ptr<Material> currentMaterial = modelRenderer.material;
        if (currentMaterial)
        {
            ImGui::Text("  %s", currentMaterial->GetName().c_str());
        }
        else if (modelRenderer.model && !modelRenderer.model->meshes.empty())
        {
            // Show default material from first mesh
            auto &defaultMaterial = modelRenderer.model->meshes[0].material;
            if (defaultMaterial)
            {
                ImGui::Text("  %s (default)", defaultMaterial->GetName().c_str());
            }
            else
            {
                ImGui::Text("  None");
            }
        }
        else
        {
            ImGui::Text("  None");
        }

        // Make the button a drag drop target
        if (ImGui::BeginDragDropTarget())
        {
            // Accept the cross-window drag payload
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MATERIAL_DRAG"))
            {
                std::cout << "[InspectorPanel] Received MATERIAL_DRAG drop - GUID: {" << g_draggedMaterialGuid.high << ", " << g_draggedMaterialGuid.low << "}, Path: " << g_draggedMaterialPath << std::endl;

                // Try GUID first, then fallback to path
                if (g_draggedMaterialGuid.high != 0 || g_draggedMaterialGuid.low != 0)
                {
                    std::cout << "[InspectorPanel] Using GUID for material loading" << std::endl;
                    ApplyMaterialToModel(entity, g_draggedMaterialGuid);
                }
                else
                {
                    std::cout << "[InspectorPanel] Using path for material loading: " << g_draggedMaterialPath << std::endl;
                    ApplyMaterialToModelByPath(entity, g_draggedMaterialPath);
                }

                // Clear the drag state
                g_draggedMaterialGuid = {0, 0};
                g_draggedMaterialPath.clear();
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }
    catch (const std::exception &e)
    {
        ImGui::Text("Error accessing ModelRenderComponent: %s", e.what());
    }
}

void InspectorPanel::ApplyMaterialToModel(Entity entity, const GUID_128 &materialGuid)
{
    try
    {
        ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity))
        {
            std::cerr << "[InspectorPanel] Entity does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent &modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelRenderer.model)
        {
            std::cerr << "[InspectorPanel] Model is not loaded" << std::endl;
            return;
        }

        // Get the material asset metadata
        std::shared_ptr<AssetMeta> materialMeta = AssetManager::GetInstance().GetAssetMeta(materialGuid);
        if (!materialMeta)
        {
            std::cerr << "[InspectorPanel] Material asset not found" << std::endl;
            return;
        }

        // Load the material
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialMeta->sourceFilePath);
        if (!material)
        {
            std::cerr << "[InspectorPanel] Failed to load material: " << materialMeta->sourceFilePath << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial")
        {
            std::filesystem::path path(materialMeta->sourceFilePath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[InspectorPanel] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        std::cout << "[InspectorPanel] Applied material '" << material->GetName() << "' to entity" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[InspectorPanel] Error applying material to model: " << e.what() << std::endl;
    }
}

void InspectorPanel::ApplyMaterialToModelByPath(Entity entity, const std::string &materialPath)
{
    try
    {
        ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity))
        {
            std::cerr << "[InspectorPanel] Entity does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent &modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelRenderer.model)
        {
            std::cerr << "[InspectorPanel] Model is not loaded" << std::endl;
            return;
        }

        // Load the material directly by path
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialPath);
        if (!material)
        {
            std::cerr << "[InspectorPanel] Failed to load material: " << materialPath << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial")
        {
            std::filesystem::path path(materialPath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[InspectorPanel] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        std::cout << "[InspectorPanel] Applied material '" << material->GetName() << "' to entity (by path)" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[InspectorPanel] Error applying material to model by path: " << e.what() << std::endl;
    }
}

void InspectorPanel::DrawSelectedAsset(const GUID_128 &assetGuid)
{
    try
    {
        // Get asset metadata from AssetManager
        std::shared_ptr<AssetMeta> assetMeta = AssetManager::GetInstance().GetAssetMeta(assetGuid);
        if (!assetMeta)
        {
            ImGui::Text("Asset not found");
            return;
        }

        // Determine asset type from extension
        std::filesystem::path assetPath(assetMeta->sourceFilePath);
        std::string extension = assetPath.extension().string();

        // Handle different asset types
        if (extension == ".mat")
        {
            // Check if we have a cached material for this asset
            if (!cachedMaterial || cachedMaterialGuid.high != assetGuid.high || cachedMaterialGuid.low != assetGuid.low)
            {
                // Load material and cache it
                std::cout << "[Inspector] Loading material from: " << assetMeta->sourceFilePath << std::endl;
                cachedMaterial = std::make_shared<Material>();
                if (cachedMaterial->LoadResource(assetMeta->sourceFilePath))
                {
                    cachedMaterialGuid = assetGuid;
                    cachedMaterialPath = assetMeta->sourceFilePath;
                    std::cout << "[Inspector] Successfully loaded and cached material: " << cachedMaterial->GetName() << " with " << cachedMaterial->GetAllTextureInfo().size() << " textures" << std::endl;
                }
                else
                {
                    cachedMaterial.reset();
                    cachedMaterialGuid = {0, 0};
                    cachedMaterialPath.clear();
                    ImGui::Text("Failed to load material");
                    return;
                }
            }

            // Use cached material
            MaterialInspector::DrawMaterialAsset(cachedMaterial, assetMeta->sourceFilePath);
        }
        else
        {
            ImGui::Text("Asset type not supported for editing in Inspector");
        }
    }
    catch (const std::exception &e)
    {
        ImGui::Text("Error accessing asset: %s", e.what());
    }
}

void InspectorPanel::DrawAudioComponent(Entity entity)
{
    try
    {
        ECSManager &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        AudioComponent &audio = ecsManager.GetComponent<AudioComponent>(entity);

        ImGui::PushID("AudioComponent");

        // Asset path input
        char buffer[512] = {0};
        if (!audio.AudioAssetPath.empty())
        {
            strncpy_s(buffer, audio.AudioAssetPath.c_str(), sizeof(buffer) - 1);
        }
        ImGui::Text("Audio Asset Path");
        ImGui::SameLine();
        if (ImGui::InputText("##AudioPath", buffer, sizeof(buffer)))
        {
            audio.SetAudioAssetPath(std::string(buffer));
        }

        // Volume slider
        float vol = audio.Volume;
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f))
        {
            audio.Volume = vol;
            // if (audio.Channel) {
            //     FMOD_Channel_SetVolume(reinterpret_cast<FMOD_CHANNEL*>(0), audio.Volume); // placeholder if needed
            // }
        }

        // Loop checkbox
        if (ImGui::Checkbox("Loop", &audio.Loop))
        {
            // no immediate action; applied at play time
        }

        // Play on Awake
        ImGui::Checkbox("Play On Awake", &audio.PlayOnAwake);

        // Spatialize
        if (ImGui::Checkbox("Spatialize", &audio.Spatialize))
        {
            // toggled
        }

        // Attenuation
        float att = audio.Attenuation;
        if (ImGui::SliderFloat("Attenuation", &att, 0.0f, 10.0f))
        {
            audio.Attenuation = att;
        }

        // Position (if spatialized)
        if (audio.Spatialize)
        {
            float pos[3] = {audio.Position.x, audio.Position.y, audio.Position.z};
            if (ImGui::DragFloat3("Position", pos, 0.1f))
            {
                audio.UpdatePosition(Vector3D(pos[0], pos[1], pos[2]));
                // Also update Transform if present
                if (ecsManager.HasComponent<Transform>(entity))
                {
                    ecsManager.transformSystem->SetLocalPosition(entity, {pos[0], pos[1], pos[2]});
                }
            }
        }

        // Play/Stop buttons
        if (ImGui::Button("Play"))
        {
            audio.Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            audio.Stop();
        }

        ImGui::PopID();
    }
    catch (const std::exception &e)
    {
        ImGui::Text("Error accessing AudioComponent: %s", e.what());
    }
}
