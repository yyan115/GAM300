#include "Panels/InspectorPanel.hpp"
#include "Panels/AssetBrowserPanel.hpp"
#include "imgui.h"
#include "GUIManager.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Graphics/Texture.h>
#include <Graphics/ShaderClass.h>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include <Utilities/GUID.hpp>
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>
#include <glm/glm.hpp>

// Global drag-drop state for cross-window material dragging (declared in AssetBrowserPanel.cpp)
extern GUID_128 DraggedMaterialGuid;
extern std::string DraggedMaterialPath;

// Global drag-drop state for cross-window model dragging (declared in AssetBrowserPanel.cpp)
extern GUID_128 DraggedModelGuid;
extern std::string DraggedModelPath;

// Global drag-drop state for cross-window audio dragging (declared in AssetBrowserPanel.cpp)
extern GUID_128 DraggedAudioGuid;
extern std::string DraggedAudioPath;
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <Sound/AudioComponent.hpp>

InspectorPanel::InspectorPanel()
    : EditorPanel("Inspector", true) {
}

void InspectorPanel::OnImGuiRender() {
    if (ImGui::Begin(name.c_str(), &isOpen)) {
        // Check for selected asset first (higher priority)
        GUID_128 selectedAsset = GUIManager::GetSelectedAsset();

		// Determine what to display based on lock state
		Entity displayEntity = static_cast<Entity>(-1);
		GUID_128 displayAsset = { 0, 0 };

        if (inspectorLocked) {
            // Show locked content
            if (lockedEntity != static_cast<Entity>(-1)) {
                displayEntity = lockedEntity;
            } else if (lockedAsset.high != 0 || lockedAsset.low != 0) {
                displayAsset = lockedAsset;
            }
        } else {
            // Show current selection
            if (selectedAsset.high != 0 || selectedAsset.low != 0) {
                displayAsset = selectedAsset;
            } else {
                displayEntity = GUIManager::GetSelectedEntity();
            }
        }

        // Validate locked content
        if (inspectorLocked) {
            if (lockedEntity != static_cast<Entity>(-1)) {
                try {
                    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                    auto activeEntities = ecsManager.GetActiveEntities();
                    bool entityExists = std::find(activeEntities.begin(), activeEntities.end(), lockedEntity) != activeEntities.end();
                    if (!entityExists) {
                        // Locked entity no longer exists, unlock
                        inspectorLocked = false;
                        lockedEntity = static_cast<Entity>(-1);
                        lockedAsset = {0, 0};
                        displayEntity = GUIManager::GetSelectedEntity();
                        displayAsset = GUIManager::GetSelectedAsset();
                    }
                } catch (...) {
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
        if (displayAsset.high != 0 || displayAsset.low != 0) {
            DrawSelectedAsset(displayAsset);
        } else {
            // Clear cached material when no asset is selected
            if (cachedMaterial) {
                std::cout << "[Inspector] Clearing cached material" << std::endl;
                cachedMaterial.reset();
                cachedMaterialGuid = {0, 0};
                cachedMaterialPath.clear();
            }

            if (displayEntity == static_cast<Entity>(-1)) {
                ImGui::Text("No object selected");

                // Lock button on the same line
                ImGui::SameLine(ImGui::GetWindowWidth() - 35);
                if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
                    inspectorLocked = !inspectorLocked;
                    if (inspectorLocked) {
                        // Lock to current content (entity or asset)
                        if (selectedAsset.high != 0 || selectedAsset.low != 0) {
                            lockedAsset = selectedAsset;
                            lockedEntity = static_cast<Entity>(-1);
                        } else {
                            lockedEntity = GUIManager::GetSelectedEntity();
                            lockedAsset = {0, 0};
                        }
                    } else {
                        // Unlock
                        lockedEntity = static_cast<Entity>(-1);
                        lockedAsset = {0, 0};
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
                }

                ImGui::Text("Select an object in the Scene Hierarchy or an asset in the Asset Browser to view its properties");
                if (inspectorLocked) {
                    ImGui::Text("Inspector is locked but no valid content is selected.");
                }
            } else {
                try {
                    // Get the active ECS manager
                    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

                    ImGui::Text("Entity ID: %u", displayEntity);

                    // Lock button on the same line
                    ImGui::SameLine(ImGui::GetWindowWidth() - 35);
                    if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
                        inspectorLocked = !inspectorLocked;
                        if (inspectorLocked) {
                            // Lock to current content (entity or asset)
                            if (selectedAsset.high != 0 || selectedAsset.low != 0) {
                                lockedAsset = selectedAsset;
                                lockedEntity = static_cast<Entity>(-1);
                            } else {
                                lockedEntity = GUIManager::GetSelectedEntity();
                                lockedAsset = {0, 0};
                            }
                        } else {
                            // Unlock
                            lockedEntity = static_cast<Entity>(-1);
                            lockedAsset = {0, 0};
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
                    }
                    ImGui::Separator();

                    // Draw NameComponent if it exists
                    if (ecsManager.HasComponent<NameComponent>(displayEntity)) {
                        DrawNameComponent(displayEntity);
                        ImGui::Separator();
                    }

                    // Draw Transform component if it exists
                    if (ecsManager.HasComponent<Transform>(displayEntity)) {
                        if (DrawComponentHeaderWithRemoval("Transform", displayEntity, "TransformComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
                            DrawTransformComponent(displayEntity);
                        }
                    }

                    // Draw ModelRenderComponent if it exists
                    if (ecsManager.HasComponent<ModelRenderComponent>(displayEntity)) {
                        if (DrawComponentHeaderWithRemoval("Model Renderer", displayEntity, "ModelRenderComponent")) {
                            DrawModelRenderComponent(displayEntity);
                        }
                    }

                    // Draw AudioComponent if present
                    if (ecsManager.HasComponent<AudioComponent>(displayEntity)) {
                        if (DrawComponentHeaderWithRemoval("Audio", displayEntity, "AudioComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
                            DrawAudioComponent(displayEntity);
                        }
                    }

                    // Draw Light Components if present
                    DrawLightComponents(displayEntity);

                    // Add Component button
                    ImGui::Separator();
                    DrawAddComponentButton(displayEntity);

                } catch (const std::exception& e) {
                    ImGui::Text("Error accessing entity: %s", e.what());
                }
            }
        }
    }

    // Process any pending component removals after ImGui rendering is complete
    ProcessPendingComponentRemovals();

    ImGui::End();
}

void InspectorPanel::DrawNameComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        NameComponent& nameComponent = ecsManager.GetComponent<NameComponent>(entity);

		ImGui::PushID("NameComponent");

		// Use a static map to maintain state per entity
		static std::unordered_map<Entity, std::vector<char>> nameBuffers;

        // Get or create buffer for this entity
        auto& nameBuffer = nameBuffers[entity];

        // Initialize buffer if empty or different from component
        std::string currentName = nameComponent.name;
        if (nameBuffer.empty() || std::string(nameBuffer.data()) != currentName) {
            nameBuffer.clear();
            nameBuffer.resize(256, '\0'); // Create 256-char buffer filled with null terminators
            if (!currentName.empty() && currentName.length() < 255) {
                std::copy(currentName.begin(), currentName.end(), nameBuffer.begin());
            }
        }

        // Use InputText with char buffer
        ImGui::Text("Name");
        ImGui::SameLine();
        if (ImGui::InputText("##Name", nameBuffer.data(), nameBuffer.size())) {
            // Update the actual component
            nameComponent.name = std::string(nameBuffer.data());
        }

        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing NameComponent: %s", e.what());
    }
}

void InspectorPanel::DrawTransformComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        Transform& transform = ecsManager.GetComponent<Transform>(entity);

		ImGui::PushID("Transform");

        // Position
        float position[3] = { transform.localPosition.x, transform.localPosition.y, transform.localPosition.z };
        ImGui::Text("Position");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Position", position, 0.1f, -FLT_MAX, FLT_MAX, "%.3f")) {
            ecsManager.transformSystem->SetLocalPosition(entity, { position[0], position[1], position[2] });
        }

        // Rotation
        Vector3D rotationEuler = transform.localRotation.ToEulerDegrees();
        float rotation[3] = { rotationEuler.x, rotationEuler.y, rotationEuler.z };
        ImGui::Text("Rotation");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Rotation", rotation, 1.0f, -180.0f, 180.0f, "%.1f")) {
            ecsManager.transformSystem->SetLocalRotation(entity, { rotation[0], rotation[1], rotation[2] });
        }

        // Scale
        float scale[3] = { transform.localScale.x, transform.localScale.y, transform.localScale.z };
        ImGui::Text("Scale");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Scale", scale, 0.1f, 0.001f, FLT_MAX, "%.3f")) {
            ecsManager.transformSystem->SetLocalScale(entity, { scale[0], scale[1], scale[2] });
        }

        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing Transform: %s", e.what());
    }
}

void InspectorPanel::DrawModelRenderComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

		ImGui::PushID("ModelRenderComponent");

        // Display model info (read-only for now)
        ImGui::Text("Model Renderer Component");

        // Model drag-drop slot like Unity
        ImGui::Text("Model:");
        ImGui::SameLine();

        // Create a model slot button that shows current model
        std::string modelButtonText;
        if (modelRenderer.model) {
            // Extract filename from model path or use a default name
            modelButtonText = "Loaded Model (" + std::to_string(modelRenderer.model->meshes.size()) + " meshes)";
        } else {
            modelButtonText = "None (Drop model here)";
        }

        // Create the model slot button (like Unity's model slot)
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        ImGui::Button(modelButtonText.c_str(), ImVec2(buttonWidth, 30.0f));

        // The button is now the drag-drop target for models
        if (ImGui::BeginDragDropTarget()) {
            // Visual feedback - highlight when dragging over
            ImGui::SetTooltip("Drop .obj, .fbx, .dae, or .3ds model here");
            // Accept the cross-window drag payload
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_DRAG")) {
                // Apply the model to the ModelRenderComponent
                ApplyModelToRenderer(entity, DraggedModelGuid, DraggedModelPath);

                // Clear the drag state
                DraggedModelGuid = {0, 0};
                DraggedModelPath.clear();
            }
            ImGui::EndDragDropTarget();
        }

        if (modelRenderer.shader) {
            ImGui::Text("Shader: Loaded");
        } else {
            ImGui::Text("Shader: None");
        }

        ImGui::Separator();

        // Material drag-drop slot like Unity
        ImGui::Text("Material:");
        ImGui::SameLine();

        // Create a material slot button that shows current material
        std::shared_ptr<Material> currentMaterial = modelRenderer.material;
        std::string buttonText;
        if (currentMaterial) {
            buttonText = currentMaterial->GetName();
        } else if (modelRenderer.model && !modelRenderer.model->meshes.empty()) {
            // Show default material from first mesh
            auto& defaultMaterial = modelRenderer.model->meshes[0].material;
            if (defaultMaterial) {
                buttonText = defaultMaterial->GetName() + " (default)";
            } else {
                buttonText = "None (Drop material here)";
            }
        } else {
            buttonText = "None (Drop material here)";
        }

        // Create the material slot button (like Unity's material slot)
        float materialButtonWidth = ImGui::GetContentRegionAvail().x;
        ImGui::Button(buttonText.c_str(), ImVec2(materialButtonWidth, 30.0f));

        // The button is now the drag-drop target
        if (ImGui::BeginDragDropTarget()) {
            // Visual feedback - highlight when dragging over
            ImGui::SetTooltip("Drop material here to apply to model");
            // Accept the cross-window drag payload
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_DRAG")) {
                // Try GUID first, then fallback to path
                if (DraggedMaterialGuid.high != 0 || DraggedMaterialGuid.low != 0) {
                    MaterialInspector::ApplyMaterialToModel(entity, DraggedMaterialGuid);
                } else {
                    MaterialInspector::ApplyMaterialToModelByPath(entity, DraggedMaterialPath);
                }

                // Clear the drag state
                DraggedMaterialGuid = {0, 0};
                DraggedMaterialPath.clear();
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing ModelRenderComponent: %s", e.what());
    }
}

void InspectorPanel::DrawAudioComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        AudioComponent& audio = ecsManager.GetComponent<AudioComponent>(entity);

        ImGui::PushID("AudioComponent");

        // Audio Clip drag-drop slot (Unity-style)
        ImGui::Text("Clip:");
        ImGui::SameLine();

        // Create audio slot button showing current clip
        std::string audioButtonText;
        if (!audio.Clip.empty()) {
            std::filesystem::path clipPath(audio.Clip);
            audioButtonText = clipPath.filename().string();
        } else {
            audioButtonText = "None (Audio Clip)";
        }

        // Create the audio slot button
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        ImGui::Button(audioButtonText.c_str(), ImVec2(buttonWidth, 30.0f));

        // Audio clip drag-drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_DRAG")) {
                // Use global drag data set by AssetBrowserPanel
                audio.SetClip(DraggedAudioPath);
            }
            ImGui::EndDragDropTarget();
        }

        // Right-click to clear
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !audio.Clip.empty()) {
            ImGui::OpenPopup("ClearAudioClip");
        }

        if (ImGui::BeginPopup("ClearAudioClip")) {
            if (ImGui::MenuItem("Clear Clip")) {
                audio.SetClip("");
            }
            ImGui::EndPopup();
        }

        // Volume slider
        float vol = audio.Volume;
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
            audio.Volume = vol;
            //if (audio.Channel) {
            //    FMOD_Channel_SetVolume(reinterpret_cast<FMOD_CHANNEL*>(0), audio.Volume); // placeholder if needed
            //}
        }

        // Loop checkbox
        if (ImGui::Checkbox("Loop", &audio.Loop)) {
            // no immediate action; applied at play time
        }

        // Play on Awake (Unity naming)
        ImGui::Checkbox("Play On Awake", &audio.PlayOnAwake);

        // Spatialize
        if (ImGui::Checkbox("Spatialize", &audio.Spatialize)) {
            // toggled
        }

        // Spatial Blend (Unity naming)
        float blend = audio.SpatialBlend;
        if (ImGui::SliderFloat("Spatial Blend", &blend, 0.0f, 1.0f)) {
            audio.SetSpatialBlend(blend);
        }

        // Position (if spatialized)
        if (audio.Spatialize) {
            float pos[3] = { audio.Position.x, audio.Position.y, audio.Position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                /*audio.UpdatePosition(Vector3D(pos[0], pos[1], pos[2]));*/
                // Also update Transform if present
                if (ecsManager.HasComponent<Transform>(entity)) {
                    ecsManager.transformSystem->SetLocalPosition(entity, { pos[0], pos[1], pos[2] });
                }
            }
        }

        // Play/Stop buttons
        if (ImGui::Button("Play")) {
            audio.Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            audio.Stop();
        }

        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing AudioComponent: %s", e.what());
    }
}

void InspectorPanel::ApplyMaterialToModel(Entity entity, const GUID_128& materialGuid) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            std::cerr << "[InspectorPanel] Entity does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelRenderer.model) {
            std::cerr << "[InspectorPanel] Model is not loaded" << std::endl;
            return;
        }

        // Get the material asset metadata
        std::shared_ptr<AssetMeta> materialMeta = AssetManager::GetInstance().GetAssetMeta(materialGuid);
        if (!materialMeta) {
            std::cerr << "[InspectorPanel] Material asset not found" << std::endl;
            return;
        }

        // Load the material
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialMeta->sourceFilePath);
        if (!material) {
            std::cerr << "[InspectorPanel] Failed to load material: " << materialMeta->sourceFilePath << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
            std::filesystem::path path(materialMeta->sourceFilePath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[InspectorPanel] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        std::cout << "[InspectorPanel] Applied material '" << material->GetName() << "' to entity" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[InspectorPanel] Error applying material to model: " << e.what() << std::endl;
    }
}

void InspectorPanel::ApplyMaterialToModelByPath(Entity entity, const std::string& materialPath) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            std::cerr << "[InspectorPanel] Entity does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelRenderer.model) {
            std::cerr << "[InspectorPanel] Model is not loaded" << std::endl;
            return;
        }

        // Load the material directly by path
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialPath);
        if (!material) {
            std::cerr << "[InspectorPanel] Failed to load material: " << materialPath << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
            std::filesystem::path path(materialPath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[InspectorPanel] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        std::cout << "[InspectorPanel] Applied material '" << material->GetName() << "' to entity (by path)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[InspectorPanel] Error applying material to model by path: " << e.what() << std::endl;
    }
}

void InspectorPanel::DrawSelectedAsset(const GUID_128& assetGuid) {
    try {
        // Get asset metadata from AssetManager
        std::shared_ptr<AssetMeta> assetMeta = AssetManager::GetInstance().GetAssetMeta(assetGuid);
        std::string sourceFilePath;
        
        if (!assetMeta) {
            // Check if this is a fallback GUID - try to find the file path from the asset browser
            std::cout << "[Inspector] AssetMeta not found for GUID, trying fallback path lookup" << std::endl;
            
            sourceFilePath = AssetBrowserPanel::GetFallbackGuidFilePath(assetGuid);
            if (sourceFilePath.empty()) {
                ImGui::Text("Asset not found - no metadata or fallback path available");

                // Lock button on the same line
                GUID_128 selectedAsset = GUIManager::GetSelectedAsset();
                ImGui::SameLine(ImGui::GetWindowWidth() - 35);
                if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
                    inspectorLocked = !inspectorLocked;
                    if (inspectorLocked) {
                        // Lock to current content (entity or asset)
                        if (selectedAsset.high != 0 || selectedAsset.low != 0) {
                            lockedAsset = selectedAsset;
                            lockedEntity = static_cast<Entity>(-1);
                        } else {
                            lockedEntity = GUIManager::GetSelectedEntity();
                            lockedAsset = {0, 0};
                        }
                    } else {
                        // Unlock
                        lockedEntity = static_cast<Entity>(-1);
                        lockedAsset = {0, 0};
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
                }
                return;
            }
            std::cout << "[Inspector] Found fallback path: " << sourceFilePath << std::endl;
        } else {
            sourceFilePath = assetMeta->sourceFilePath;
        }

        // Determine asset type from extension
        std::filesystem::path assetPath(sourceFilePath);
        std::string extension = assetPath.extension().string();

        // Get selected asset for lock callback
        GUID_128 selectedAsset = GUIManager::GetSelectedAsset();

        // Handle different asset types
        if (extension == ".mat") {
            // Check if we have a cached material for this asset
            if (!cachedMaterial || cachedMaterialGuid.high != assetGuid.high || cachedMaterialGuid.low != assetGuid.low) {
                // Convert to absolute path to avoid path resolution issues
                std::filesystem::path absolutePath = std::filesystem::absolute(sourceFilePath);
                std::string absolutePathStr = absolutePath.string();

                // Load material and cache it
                std::cout << "[Inspector] Loading material from: " << sourceFilePath << std::endl;
                std::cout << "[Inspector] Absolute path: " << absolutePathStr << std::endl;
                cachedMaterial = std::make_shared<Material>();
                if (cachedMaterial->LoadResource(absolutePathStr, "")) {
                    cachedMaterialGuid = assetGuid;
                    cachedMaterialPath = sourceFilePath;
                    std::cout << "[Inspector] Successfully loaded and cached material: " << cachedMaterial->GetName() << " with " << cachedMaterial->GetAllTextureInfo().size() << " textures" << std::endl;
                } else {
                    cachedMaterial.reset();
                    cachedMaterialGuid = {0, 0};
                    cachedMaterialPath.clear();
                    ImGui::Text("Failed to load material");
                    return;
                }
            }

            // Use cached material with lock button
            auto lockCallback = [this, selectedAsset]() {
                inspectorLocked = !inspectorLocked;
                if (inspectorLocked) {
                    lockedAsset = selectedAsset;
                    lockedEntity = static_cast<Entity>(-1);
                } else {
                    lockedEntity = static_cast<Entity>(-1);
                    lockedAsset = {0, 0};
                }
            };

            MaterialInspector::DrawMaterialAsset(cachedMaterial, sourceFilePath, true, &inspectorLocked, lockCallback);
        } else {
            ImGui::Text("Asset type not supported for editing in Inspector");
        }

    } catch (const std::exception& e) {
        ImGui::Text("Error accessing asset: %s", e.what());
    }
}

void InspectorPanel::DrawLightComponents(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Draw DirectionalLightComponent if present
        if (ecsManager.HasComponent<DirectionalLightComponent>(entity)) {
            if (DrawComponentHeaderWithRemoval("Directional Light", entity, "DirectionalLightComponent")) {
                ImGui::PushID("DirectionalLight");
                DirectionalLightComponent& light = ecsManager.GetComponent<DirectionalLightComponent>(entity);

                ImGui::Checkbox("Enabled", &light.enabled);
                ImGui::ColorEdit3("Color", &light.color.x);
                ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

                // Direction controls with visual helper
                ImGui::Text("Direction");
                ImGui::DragFloat3("##Direction", &light.direction.x, 0.01f, -1.0f, 1.0f);

                // Direction visualization
                ImGui::SameLine();
                if (ImGui::Button("Normalize")) {
                    light.direction = glm::normalize(light.direction);
                }

                // Show direction as normalized vector and common presets
                glm::vec3 normalizedDir = glm::normalize(light.direction);
                ImGui::Text("Normalized: (%.2f, %.2f, %.2f)", normalizedDir.x, normalizedDir.y, normalizedDir.z);

                // Common direction presets
                ImGui::Text("Presets:");
                if (ImGui::Button("Down")) light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
                ImGui::SameLine();
                if (ImGui::Button("Forward-Down")) light.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
                ImGui::SameLine();
                if (ImGui::Button("Side-Down")) light.direction = glm::vec3(-1.0f, -1.0f, 0.0f);

                // Visual direction indicator
                ImGui::Text("Direction Visualization:");
                ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
                ImVec2 canvas_size = ImVec2(100, 100);
                ImDrawList* draw_list = ImGui::GetWindowDrawList();

                // Draw a circle representing the "world"
                ImVec2 center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
                draw_list->AddCircle(center, 40.0f, IM_COL32(100, 100, 100, 255), 0, 2.0f);

                // Draw direction arrow (project 3D direction to 2D)
                glm::vec3 dir = glm::normalize(light.direction);
                ImVec2 arrow_end = ImVec2(center.x + dir.x * 35.0f, center.y + dir.y * 35.0f);
                draw_list->AddLine(center, arrow_end, IM_COL32(255, 255, 0, 255), 3.0f);

                // Arrow head
                ImVec2 arrowDir = ImVec2(arrow_end.x - center.x, arrow_end.y - center.y);
                float arrowLength = sqrt(arrowDir.x * arrowDir.x + arrowDir.y * arrowDir.y);
                if (arrowLength > 0) {
                    arrowDir.x /= arrowLength;
                    arrowDir.y /= arrowLength;
                    ImVec2 perpendicular = ImVec2(-arrowDir.y, arrowDir.x);
                    ImVec2 arrowHead1 = ImVec2(arrow_end.x - arrowDir.x * 8 + perpendicular.x * 4, arrow_end.y - arrowDir.y * 8 + perpendicular.y * 4);
                    ImVec2 arrowHead2 = ImVec2(arrow_end.x - arrowDir.x * 8 - perpendicular.x * 4, arrow_end.y - arrowDir.y * 8 - perpendicular.y * 4);
                    draw_list->AddLine(arrow_end, arrowHead1, IM_COL32(255, 255, 0, 255), 2.0f);
                    draw_list->AddLine(arrow_end, arrowHead2, IM_COL32(255, 255, 0, 255), 2.0f);
                }

                ImGui::Dummy(canvas_size);

                ImGui::Separator();
                ImGui::Text("Lighting Properties");
                ImGui::ColorEdit3("Ambient", &light.ambient.x);
                ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
                ImGui::ColorEdit3("Specular", &light.specular.x);

                ImGui::PopID();
            }
        }

        // Draw PointLightComponent if present
        if (ecsManager.HasComponent<PointLightComponent>(entity)) {
            if (DrawComponentHeaderWithRemoval("Point Light", entity, "PointLightComponent")) {
                ImGui::PushID("PointLight");
                PointLightComponent& light = ecsManager.GetComponent<PointLightComponent>(entity);

                ImGui::Checkbox("Enabled", &light.enabled);
                ImGui::ColorEdit3("Color", &light.color.x);
                ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

                ImGui::Separator();
                ImGui::Text("Attenuation");
                ImGui::DragFloat("Constant", &light.constant, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Linear", &light.linear, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

                ImGui::Separator();
                ImGui::Text("Lighting Properties");
                ImGui::ColorEdit3("Ambient", &light.ambient.x);
                ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
                ImGui::ColorEdit3("Specular", &light.specular.x);

                ImGui::PopID();
            }
        }

        // Draw SpotLightComponent if present
        if (ecsManager.HasComponent<SpotLightComponent>(entity)) {
            if (DrawComponentHeaderWithRemoval("Spot Light", entity, "SpotLightComponent")) {
                ImGui::PushID("SpotLight");
                SpotLightComponent& light = ecsManager.GetComponent<SpotLightComponent>(entity);

                ImGui::Checkbox("Enabled", &light.enabled);
                ImGui::ColorEdit3("Color", &light.color.x);
                ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);
                ImGui::DragFloat3("Direction", &light.direction.x, 0.1f, -1.0f, 1.0f);

                ImGui::Separator();
                ImGui::Text("Cone Settings");
                float cutOffDegrees = glm::degrees(glm::acos(light.cutOff));
                float outerCutOffDegrees = glm::degrees(glm::acos(light.outerCutOff));
                if (ImGui::DragFloat("Inner Cutoff", &cutOffDegrees, 1.0f, 0.0f, 90.0f)) {
                    light.cutOff = glm::cos(glm::radians(cutOffDegrees));
                }
                if (ImGui::DragFloat("Outer Cutoff", &outerCutOffDegrees, 1.0f, 0.0f, 90.0f)) {
                    light.outerCutOff = glm::cos(glm::radians(outerCutOffDegrees));
                }

                ImGui::Separator();
                ImGui::Text("Attenuation");
                ImGui::DragFloat("Constant", &light.constant, 0.01f, 0.0f, 2.0f);
                ImGui::DragFloat("Linear", &light.linear, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

                ImGui::Separator();
                ImGui::Text("Lighting Properties");
                ImGui::ColorEdit3("Ambient", &light.ambient.x);
                ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
                ImGui::ColorEdit3("Specular", &light.specular.x);

                ImGui::PopID();
            }
        }
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing light components: %s", e.what());
    }
}

void InspectorPanel::DrawAddComponentButton(Entity entity) {
    ImGui::Text("Add Component");

    if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

            ImGui::Text("Select Component to Add:");
            ImGui::Separator();

            // Rendering Components
            if (ImGui::BeginMenu("Rendering")) {
                if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
                    if (ImGui::MenuItem("Model Renderer")) {
                        AddComponent(entity, "ModelRenderComponent");
                    }
                }
                ImGui::EndMenu();
            }

            // Audio Components
            if (ImGui::BeginMenu("Audio")) {
                if (!ecsManager.HasComponent<AudioComponent>(entity)) {
                    if (ImGui::MenuItem("Audio Source")) {
                        AddComponent(entity, "AudioComponent");
                    }
                }
                ImGui::EndMenu();
            }

            // Lighting Components
            if (ImGui::BeginMenu("Lighting")) {
                if (!ecsManager.HasComponent<DirectionalLightComponent>(entity)) {
                    if (ImGui::MenuItem("Directional Light")) {
                        AddComponent(entity, "DirectionalLightComponent");
                    }
                }
                if (!ecsManager.HasComponent<PointLightComponent>(entity)) {
                    if (ImGui::MenuItem("Point Light")) {
                        AddComponent(entity, "PointLightComponent");
                    }
                }
                if (!ecsManager.HasComponent<SpotLightComponent>(entity)) {
                    if (ImGui::MenuItem("Spot Light")) {
                        AddComponent(entity, "SpotLightComponent");
                    }
                }
                ImGui::EndMenu();
            }

        } catch (const std::exception& e) {
            ImGui::Text("Error: %s", e.what());
        }

        ImGui::EndPopup();
    }
}

void InspectorPanel::AddComponent(Entity entity, const std::string& componentType) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (componentType == "ModelRenderComponent") {
            ModelRenderComponent component; // Use default constructor

            // Set default shader GUID for new components
            component.shaderGUID = {0x007ebbc8de41468e, 0x0002c7078200001b}; // Default shader GUID

            // Load the default shader
            std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(component.shaderGUID);
            component.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(component.shaderGUID, shaderPath);

            if (component.shader) {
                std::cout << "[Inspector] Default shader loaded successfully for new ModelRenderComponent" << std::endl;
            } else {
                std::cerr << "[Inspector] Warning: Failed to load default shader for new ModelRenderComponent" << std::endl;
            }

            ecsManager.AddComponent<ModelRenderComponent>(entity, component);
            std::cout << "[Inspector] Added ModelRenderComponent to entity " << entity << " (ready for model assignment)" << std::endl;
        }
        else if (componentType == "AudioComponent") {
            AudioComponent component;
            ecsManager.AddComponent<AudioComponent>(entity, component);
            std::cout << "[Inspector] Added AudioComponent to entity " << entity << std::endl;
        }
        else if (componentType == "DirectionalLightComponent") {
            DirectionalLightComponent component;
            // Set reasonable default values (matching SceneInstance.cpp)
            component.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
            component.ambient = glm::vec3(0.05f);
            component.diffuse = glm::vec3(0.4f);
            component.specular = glm::vec3(0.5f);
            component.enabled = true;

            ecsManager.AddComponent<DirectionalLightComponent>(entity, component);

            // Ensure entity has a Transform component
            if (!ecsManager.HasComponent<Transform>(entity)) {
                Transform transform;
                ecsManager.AddComponent<Transform>(entity, transform);
            }

            // Register entity with lighting system
            if (ecsManager.lightingSystem) {
                ecsManager.lightingSystem->RegisterEntity(entity);
            }

            std::cout << "[Inspector] Added DirectionalLightComponent to entity " << entity << std::endl;
        }
        else if (componentType == "PointLightComponent") {
            PointLightComponent component;
            // Set reasonable default values (matching SceneInstance.cpp)
            component.ambient = glm::vec3(0.05f);
            component.diffuse = glm::vec3(0.8f);
            component.specular = glm::vec3(1.0f);
            component.constant = 1.0f;
            component.linear = 0.09f;
            component.quadratic = 0.032f;
            component.enabled = true;

            ecsManager.AddComponent<PointLightComponent>(entity, component);

            // Ensure entity has a Transform component for positioning
            if (!ecsManager.HasComponent<Transform>(entity)) {
                Transform transform;
                ecsManager.AddComponent<Transform>(entity, transform);
                std::cout << "[Inspector] Added Transform component for PointLight positioning" << std::endl;
            }

            // Register entity with lighting system
            if (ecsManager.lightingSystem) {
                ecsManager.lightingSystem->RegisterEntity(entity);
            }

            std::cout << "[Inspector] Added PointLightComponent to entity " << entity << std::endl;
        }
        else if (componentType == "SpotLightComponent") {
            SpotLightComponent component;
            // Set reasonable default values (matching SceneInstance.cpp)
            component.direction = glm::vec3(0.0f, 0.0f, -1.0f);
            component.ambient = glm::vec3(0.0f);
            component.diffuse = glm::vec3(1.0f);
            component.specular = glm::vec3(1.0f);
            component.constant = 1.0f;
            component.linear = 0.09f;
            component.quadratic = 0.032f;
            component.cutOff = 0.976f;      // cos(12.5 degrees)
            component.outerCutOff = 0.966f; // cos(15 degrees)
            component.enabled = true;

            ecsManager.AddComponent<SpotLightComponent>(entity, component);

            // Ensure entity has a Transform component
            if (!ecsManager.HasComponent<Transform>(entity)) {
                Transform transform;
                ecsManager.AddComponent<Transform>(entity, transform);
            }

            // Register entity with lighting system
            if (ecsManager.lightingSystem) {
                ecsManager.lightingSystem->RegisterEntity(entity);
            }

            std::cout << "[Inspector] Added SpotLightComponent to entity " << entity << std::endl;
        }
        else {
            std::cerr << "[Inspector] Unknown component type: " << componentType << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[Inspector] Failed to add component " << componentType << " to entity " << entity << ": " << e.what() << std::endl;
    }
}

bool InspectorPanel::DrawComponentHeaderWithRemoval(const char* label, Entity entity, const std::string& componentType, ImGuiTreeNodeFlags flags) {
    bool isOpen = ImGui::CollapsingHeader(label, flags);

    // Handle right-click for component removal
    std::string popupName = "ComponentContextMenu_" + componentType;
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) { // Right-click
        ImGui::OpenPopup(popupName.c_str());
    }

    // Context menu for component removal
    if (ImGui::BeginPopup(popupName.c_str())) {
        if (ImGui::MenuItem("Remove Component")) {
            // Queue the component removal for processing after ImGui rendering is complete
            pendingComponentRemovals.push_back({entity, componentType});
        }
        ImGui::EndPopup();
    }

    // Show tooltip on hover
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Right-click to remove component");
    }

    return isOpen;
}

void InspectorPanel::ProcessPendingComponentRemovals() {
    for (const auto& request : pendingComponentRemovals) {
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

            // Remove the component based on type
            if (request.componentType == "DirectionalLightComponent") {
                ecsManager.RemoveComponent<DirectionalLightComponent>(request.entity);
                std::cout << "[Inspector] Removed DirectionalLightComponent from entity " << request.entity << std::endl;
            }
            else if (request.componentType == "PointLightComponent") {
                ecsManager.RemoveComponent<PointLightComponent>(request.entity);
                std::cout << "[Inspector] Removed PointLightComponent from entity " << request.entity << std::endl;
            }
            else if (request.componentType == "SpotLightComponent") {
                ecsManager.RemoveComponent<SpotLightComponent>(request.entity);
                std::cout << "[Inspector] Removed SpotLightComponent from entity " << request.entity << std::endl;
            }
            else if (request.componentType == "ModelRenderComponent") {
                ecsManager.RemoveComponent<ModelRenderComponent>(request.entity);
                std::cout << "[Inspector] Removed ModelRenderComponent from entity " << request.entity << std::endl;
            }
            else if (request.componentType == "AudioComponent") {
                ecsManager.RemoveComponent<AudioComponent>(request.entity);
                std::cout << "[Inspector] Removed AudioComponent from entity " << request.entity << std::endl;
            }
            else if (request.componentType == "TransformComponent") {
                std::cerr << "[Inspector] Cannot remove TransformComponent - all entities must have one" << std::endl;
            }
            else {
                std::cerr << "[Inspector] Unknown component type for removal: " << request.componentType << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Inspector] Failed to remove component " << request.componentType << " from entity " << request.entity << ": " << e.what() << std::endl;
        }
    }

    // Clear the queue after processing
    pendingComponentRemovals.clear();
}

void InspectorPanel::ApplyModelToRenderer(Entity entity, const GUID_128& modelGuid, const std::string& modelPath) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            std::cerr << "[Inspector] Entity " << entity << " does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        std::cout << "[Inspector] Applying model to entity " << entity << " - GUID: {" << modelGuid.high << ", " << modelGuid.low << "}, Path: " << modelPath << std::endl;

        // Try to load model using GUID first, then fallback to path
        std::shared_ptr<Model> loadedModel = nullptr;

        if (modelGuid.high != 0 || modelGuid.low != 0) {
            std::cout << "[Inspector] Loading model using GUID..." << std::endl;
            loadedModel = ResourceManager::GetInstance().GetResourceFromGUID<Model>(modelGuid, modelPath);
        } else if (!modelPath.empty()) {
            std::cout << "[Inspector] Loading model using path: " << modelPath << std::endl;
            loadedModel = ResourceManager::GetInstance().GetResource<Model>(modelPath);
        }

        if (loadedModel) {
            std::cout << "[Inspector] Model loaded successfully, applying to ModelRenderComponent..." << std::endl;
            modelRenderer.model = loadedModel;
            modelRenderer.modelGUID = modelGuid;

            // Ensure entity has a shader for rendering
            if (modelRenderer.shaderGUID.high == 0 && modelRenderer.shaderGUID.low == 0) {
                std::cout << "[Inspector] Setting default shader for entity " << entity << std::endl;
                modelRenderer.shaderGUID = {0x007ebbc8de41468e, 0x0002c7078200001b}; // Default shader GUID
            }

            // Load the shader if it's not already loaded
            if (!modelRenderer.shader) {
                std::cout << "[Inspector] Loading shader for entity " << entity << std::endl;
                std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelRenderer.shaderGUID);
                modelRenderer.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(modelRenderer.shaderGUID, shaderPath);

                if (modelRenderer.shader) {
                    std::cout << "[Inspector] Shader loaded successfully" << std::endl;
                } else {
                    std::cerr << "[Inspector] Failed to load shader for entity " << entity << std::endl;
                }
            }

            std::cout << "[Inspector] Model successfully applied to entity " << entity << std::endl;
        } else {
            std::cerr << "[Inspector] Failed to load model for entity " << entity << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[Inspector] Error applying model to entity " << entity << ": " << e.what() << std::endl;
    }
}
