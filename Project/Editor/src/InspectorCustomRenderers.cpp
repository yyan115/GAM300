/* Start Header ************************************************************************/
/*!
\file       InspectorCustomRenderers.cpp
\author     Claude Code Assistant
\date       2025
\brief      Custom field renderers for Inspector components that need special handling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "ReflectionRenderer.hpp"
#include "ECS/ECSManager.hpp"
#include "Transform/TransformSystem.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Physics/CollisionLayers.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include <glm/glm.hpp>
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/Particle/ParticleComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Sound/AudioComponent.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include "Animation/AnimationComponent.hpp"
#include "imgui.h"
#include "EditorComponents.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <glm/glm.hpp>
#include <cfloat>

// External drag-drop state
extern GUID_128 DraggedModelGuid;
extern std::string DraggedModelPath;
extern GUID_128 DraggedMaterialGuid;
extern std::string DraggedMaterialPath;
extern GUID_128 DraggedAudioGuid;
extern std::string DraggedAudioPath;
extern GUID_128 DraggedFontGuid;
extern std::string DraggedFontPath;

void RegisterInspectorCustomRenderers() {
    // ==================== CUSTOM TYPE RENDERERS ====================
    // Register custom renderer for glm::vec3 (used by CameraComponent)

    ReflectionRenderer::RegisterCustomRenderer("glm::vec3",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            glm::vec3* vec = static_cast<glm::vec3*>(ptr);

            // Convert field name from camelCase to "Proper Case"
            std::string displayName = name;
            if (!displayName.empty()) {
                displayName[0] = static_cast<char>(std::toupper(displayName[0]));
                for (size_t i = 1; i < displayName.size(); ++i) {
                    if (std::isupper(displayName[i]) && i > 0 && std::islower(displayName[i-1])) {
                        displayName.insert(i, " ");
                        i++;
                    }
                }
            }

            ImGui::Text("%s", displayName.c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            float values[3] = { vec->x, vec->y, vec->z };
            std::string id = std::string("##") + name + "_" + std::to_string(reinterpret_cast<uintptr_t>(ptr));

            if (ImGui::DragFloat3(id.c_str(), values, 0.1f)) {
                vec->x = values[0];
                vec->y = values[1];
                vec->z = values[2];
                return true;
            }

            return false;
        });

    // ==================== NAME COMPONENT ====================
    // Name component is rendered without collapsing header at the top

    ReflectionRenderer::RegisterComponentRenderer("NameComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            NameComponent& nameComp = *static_cast<NameComponent*>(componentPtr);

            // Unity-style checkbox on the left (from ActiveComponent)
            if (ecs.HasComponent<ActiveComponent>(entity)) {
                auto& activeComp = ecs.GetComponent<ActiveComponent>(entity);

                // Style the checkbox to be smaller with white checkmark
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2)); // Smaller padding
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White checkmark
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Dark gray background
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Lighter on hover
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Even lighter when clicking

                ImGui::Checkbox("##EntityActive", &activeComp.isActive);

                ImGui::PopStyleColor(4); // Pop all 4 colors
                ImGui::PopStyleVar(); // Pop padding

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Enable/Disable Entity");
                }
                ImGui::SameLine();
            }

            // Simple text input for name (no collapsing header)
            char buf[128] = {};
            std::snprintf(buf, sizeof(buf), "%s", nameComp.name.c_str());
            if (ImGui::InputText("Name", buf, sizeof(buf))) {
                nameComp.name = buf;
            }

            return true;  // Skip default rendering (we rendered everything)
        });

    // ==================== TAG COMPONENT ====================
    // Tag component uses TagManager dropdown (rendered inline with Layer)

    ReflectionRenderer::RegisterComponentRenderer("TagComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            TagComponent& tagComp = *static_cast<TagComponent*>(componentPtr);

            // Get available tags
            const auto& availableTags = TagManager::GetInstance().GetAllTags();

            // Create items for combo box, including "Add Tag..." option
            std::vector<std::string> tagItems;
            tagItems.reserve(availableTags.size() + 1);
            for (const auto& tag : availableTags) {
                tagItems.push_back(tag);
            }
            tagItems.push_back("Add Tag...");

            // Convert to const char* array for ImGui
            std::vector<const char*> tagItemPtrs;
            tagItemPtrs.reserve(tagItems.size());
            for (const auto& item : tagItems) {
                tagItemPtrs.push_back(item.c_str());
            }

            // Ensure tagIndex is valid
            if (tagComp.tagIndex < 0 || tagComp.tagIndex >= static_cast<int>(availableTags.size())) {
                tagComp.tagIndex = 0; // Default to first tag
            }

            // Inline rendering (no label, just combo)
            ImGui::Text("Tag");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            int currentTag = tagComp.tagIndex;
            if (ImGui::Combo("##Tag", &currentTag, tagItemPtrs.data(), static_cast<int>(tagItemPtrs.size()))) {
                if (currentTag >= 0 && currentTag < static_cast<int>(availableTags.size())) {
                    tagComp.tagIndex = currentTag;
                } else if (currentTag == static_cast<int>(availableTags.size())) {
                    // "Add Tag..." was selected - could open Tags & Layers window here
                    // Reset selection to current tag
                    currentTag = tagComp.tagIndex;
                }
            }

            ImGui::SameLine();  // Keep Layer on same line

            return true;  // Skip default rendering
        });

    // ==================== LAYER COMPONENT ====================
    // Layer component uses LayerManager dropdown (rendered inline with Tag)

    ReflectionRenderer::RegisterComponentRenderer("LayerComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            LayerComponent& layerComp = *static_cast<LayerComponent*>(componentPtr);

            // Get available layers
            const auto& availableLayers = LayerManager::GetInstance().GetAllLayers();

            // Create items for combo box (only show named layers)
            std::vector<std::string> layerItems;
            std::vector<int> layerIndices;
            for (int i = 0; i < LayerManager::MAX_LAYERS; ++i) {
                const std::string& layerName = availableLayers[i];
                if (!layerName.empty()) {
                    layerItems.push_back(std::to_string(i) + ": " + layerName);
                    layerIndices.push_back(i);
                }
            }

            // Add "Add Layer..." option
            layerItems.push_back("Add Layer...");
            std::vector<int> tempIndices = layerIndices;
            tempIndices.push_back(-1); // Special value for "Add Layer..."

            // Convert to const char* array for ImGui
            std::vector<const char*> layerItemPtrs;
            layerItemPtrs.reserve(layerItems.size());
            for (const auto& item : layerItems) {
                layerItemPtrs.push_back(item.c_str());
            }

            // Ensure layerIndex is valid
            if (layerComp.layerIndex < 0 || layerComp.layerIndex >= LayerManager::MAX_LAYERS) {
                layerComp.layerIndex = 0; // Default to first layer
            }

            // Find current selection index in our filtered list
            int currentSelection = -1;
            for (size_t i = 0; i < layerIndices.size(); ++i) {
                if (layerIndices[i] == layerComp.layerIndex) {
                    currentSelection = static_cast<int>(i);
                    break;
                }
            }

            // If current layer is not in the named list, default to first
            if (currentSelection == -1 && !layerIndices.empty()) {
                currentSelection = 0;
                layerComp.layerIndex = layerIndices[0];
            }

            // Inline rendering (continues from Tag on same line)
            ImGui::Text("Layer");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::Combo("##Layer", &currentSelection, layerItemPtrs.data(), static_cast<int>(layerItemPtrs.size()))) {
                if (currentSelection >= 0 && currentSelection < static_cast<int>(tempIndices.size())) {
                    int selectedIndex = tempIndices[currentSelection];
                    if (selectedIndex != -1) {
                        layerComp.layerIndex = selectedIndex;
                    }
                }
            }

            ImGui::Separator();  // Add separator after Tag/Layer line

            return true;  // Skip default rendering
        });

    // ==================== TRANSFORM COMPONENT ====================
    // Transform needs to use TransformSystem for setting values

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localPosition",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            Vector3D* pos = static_cast<Vector3D*>(ptr);
            float arr[3] = { pos->x, pos->y, pos->z };
            ImGui::Text("Position");
            ImGui::SameLine();
            if (ImGui::DragFloat3("##Position", arr, 0.1f, -FLT_MAX, FLT_MAX, "%.3f")) {
                ecs.transformSystem->SetLocalPosition(entity, { arr[0], arr[1], arr[2] });
                return true;
            }
            return false;
        });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localRotation",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            Quaternion* quat = static_cast<Quaternion*>(ptr);
            Vector3D euler = quat->ToEulerDegrees();
            float arr[3] = { euler.x, euler.y, euler.z };
            ImGui::Text("Rotation");
            ImGui::SameLine();
            if (ImGui::DragFloat3("##Rotation", arr, 1.0f, -180.0f, 180.0f, "%.1f")) {
                ecs.transformSystem->SetLocalRotation(entity, { arr[0], arr[1], arr[2] });
                return true;
            }
            return false;
        });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localScale",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            Vector3D* scale = static_cast<Vector3D*>(ptr);
            float arr[3] = { scale->x, scale->y, scale->z };
            ImGui::Text("Scale");
            ImGui::SameLine();
            if (ImGui::DragFloat3("##Scale", arr, 0.1f, 0.001f, FLT_MAX, "%.3f")) {
                ecs.transformSystem->SetLocalScale(entity, { arr[0], arr[1], arr[2] });
                return true;
            }
            return false;
        });

    // ==================== COLLIDER COMPONENT ====================
    // ColliderComponent needs custom rendering for shape type and parameters

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "shapeTypeID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            auto& collider = ecs.GetComponent<ColliderComponent>(entity);

            ImGui::Text("Shape Type");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            const char* shapeTypes[] = { "Box", "Sphere", "Capsule", "Cylinder" };
            int currentShapeType = static_cast<int>(collider.shapeType);

            EditorComponents::PushComboColors();
            bool changed = ImGui::Combo("##ShapeType", &currentShapeType, shapeTypes, 4);
            EditorComponents::PopComboColors();

            if (changed) {
                collider.shapeType = static_cast<ColliderShapeType>(currentShapeType);
                collider.shapeTypeID = currentShapeType;
                collider.version++;
            }

            // Shape Parameters based on type
            bool shapeParamsChanged = false;
            switch (collider.shapeType) {
                case ColliderShapeType::Box: {
                    ImGui::Text("Half Extents");
                    ImGui::SameLine();
                    float halfExtents[3] = { collider.boxHalfExtents.x, collider.boxHalfExtents.y, collider.boxHalfExtents.z };
                    if (ImGui::DragFloat3("##HalfExtents", halfExtents, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                        collider.boxHalfExtents = Vector3D(halfExtents[0], halfExtents[1], halfExtents[2]);
                        shapeParamsChanged = true;
                    }
                    break;
                }
                case ColliderShapeType::Sphere: {
                    ImGui::Text("Radius");
                    ImGui::SameLine();
                    if (ImGui::DragFloat("##SphereRadius", &collider.sphereRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                        shapeParamsChanged = true;
                    }
                    break;
                }
                case ColliderShapeType::Capsule: {
                    ImGui::Text("Radius");
                    ImGui::SameLine();
                    if (ImGui::DragFloat("##CapsuleRadius", &collider.capsuleRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                        shapeParamsChanged = true;
                    }
                    ImGui::Text("Half Height");
                    ImGui::SameLine();
                    if (ImGui::DragFloat("##CapsuleHalfHeight", &collider.capsuleHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                        shapeParamsChanged = true;
                    }
                    break;
                }
                case ColliderShapeType::Cylinder: {
                    ImGui::Text("Radius");
                    ImGui::SameLine();
                    if (ImGui::DragFloat("##CylinderRadius", &collider.cylinderRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                        shapeParamsChanged = true;
                    }
                    ImGui::Text("Half Height");
                    ImGui::SameLine();
                    if (ImGui::DragFloat("##CylinderHalfHeight", &collider.cylinderHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                        shapeParamsChanged = true;
                    }
                    break;
                }
            }

            if (shapeParamsChanged) {
                collider.version++;
            }

            return changed || shapeParamsChanged;
        });

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "layerID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            auto& collider = ecs.GetComponent<ColliderComponent>(entity);

            ImGui::Text("Layer");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            const char* layers[] = { "Non-Moving", "Moving", "Sensor", "Debris" };
            int currentLayer = static_cast<int>(collider.layer);

            EditorComponents::PushComboColors();
            bool changed = ImGui::Combo("##Layer", &currentLayer, layers, 4);
            EditorComponents::PopComboColors();

            if (changed) {
                collider.layer = static_cast<JPH::ObjectLayer>(currentLayer);
                collider.layerID = currentLayer;
                collider.version++;
            }

            return changed;
        });

    // Skip non-reflected fields (these are handled with shapeTypeID)
    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "boxHalfExtents",
        [](const char*, void*, Entity, ECSManager&) { return false; });

    // ==================== CAMERA COMPONENT ====================
    // Camera needs special handling for enum and glm::vec3 properties

    ReflectionRenderer::RegisterComponentRenderer("CameraComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            CameraComponent& camera = *static_cast<CameraComponent*>(componentPtr);

            // Manually render the non-reflected properties first

            // Projection Type dropdown
            ImGui::Text("Projection");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            const char* projTypes[] = { "Perspective", "Orthographic" };
            int currentProj = static_cast<int>(camera.projectionType);

            EditorComponents::PushComboColors();
            if (ImGui::Combo("##Projection", &currentProj, projTypes, 2)) {
                camera.projectionType = static_cast<ProjectionType>(currentProj);
            }
            EditorComponents::PopComboColors();

            // Target (glm::vec3)
            ImGui::Text("Target");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            float target[3] = { camera.target.x, camera.target.y, camera.target.z };
            if (ImGui::DragFloat3("##Target", target, 0.1f)) {
                camera.target = glm::vec3(target[0], target[1], target[2]);
            }

            // Up (glm::vec3)
            ImGui::Text("Up");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            float up[3] = { camera.up.x, camera.up.y, camera.up.z };
            if (ImGui::DragFloat3("##Up", up, 0.1f)) {
                camera.up = glm::vec3(up[0], up[1], up[2]);
            }

            // Return false to continue with reflected properties (all the floats/bools)
            return false;
        });

    // ==================== GUID FIELDS WITH DRAG-DROP ====================
    // Model GUID drag-drop

    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "modelGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Model:");
            ImGui::SameLine();

            // Display current model path or "None"
            std::string modelPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = modelPath.empty() ? "None (Model)" : modelPath.substr(modelPath.find_last_of("/\\") + 1);

            // Use EditorComponents for better drag-drop visual feedback
            float buttonWidth = ImGui::GetContentRegionAvail().x;
            EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

            // Drag-drop target with proper payload type
            if (EditorComponents::BeginDragDropTarget()) {
                ImGui::SetTooltip("Drop .obj, .fbx, .dae, or .3ds model here");
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_DRAG")) {
                    // Load and apply the model
                    auto& modelRenderer = ecs.GetComponent<ModelRenderComponent>(entity);

                    std::cout << "[Inspector] Applying model - GUID: {" << DraggedModelGuid.high << ", " << DraggedModelGuid.low << "}, Path: " << DraggedModelPath << std::endl;

                    try {
                        // Load model using ResourceManager
                        std::shared_ptr<Model> loadedModel = nullptr;
                        if (DraggedModelGuid.high != 0 || DraggedModelGuid.low != 0) {
                            loadedModel = ResourceManager::GetInstance().GetResourceFromGUID<Model>(DraggedModelGuid, DraggedModelPath);
                        } else if (!DraggedModelPath.empty()) {
                            loadedModel = ResourceManager::GetInstance().GetResource<Model>(DraggedModelPath);
                        }

                        if (loadedModel) {
                            std::cout << "[Inspector] Model loaded successfully!" << std::endl;
                            modelRenderer.model = loadedModel;
                            modelRenderer.modelGUID = DraggedModelGuid;

                            // Load default shader if not already set
                            if (!modelRenderer.shader) {
                                modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(
                                    ResourceManager::GetPlatformShaderPath("default"));
                                modelRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(
                                    ResourceManager::GetPlatformShaderPath("default"));
                            }
                        } else {
                            std::cerr << "[Inspector] Failed to load model!" << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[Inspector] Exception loading model: " << e.what() << std::endl;
                        std::cerr << "[Inspector] Model may have corrupted material references. Please check the .obj file." << std::endl;
                    }

                    // Clear the drag state
                    DraggedModelGuid = {0, 0};
                    DraggedModelPath.clear();

                    EditorComponents::EndDragDropTarget();
                    return true;
                }
                EditorComponents::EndDragDropTarget();
            }

            return false;
        });

    // Shader GUID drag-drop
    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "shaderGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Shader:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = shaderPath.empty() ? "None" : shaderPath.substr(shaderPath.find_last_of("/\\") + 1);

            ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

            // TODO: Add shader drag-drop support when available

            return false;
        });

    // Material GUID drag-drop
    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "materialGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Material:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            std::string materialPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = materialPath.empty() ? "None" : materialPath.substr(materialPath.find_last_of("/\\") + 1);

            EditorComponents::DrawDragDropButton(displayText.c_str(), -1);

            // Material drag-drop target
            if (EditorComponents::BeginDragDropTarget()) {
                ImGui::SetTooltip("Drop material here to apply to model");
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_DRAG")) {
                    *guid = DraggedMaterialGuid;
                    EditorComponents::EndDragDropTarget();
                    return true;
                }
                EditorComponents::EndDragDropTarget();
            }

            return false;
        });

    // Sprite texture GUID
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "textureGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Texture:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = texPath.empty() ? "None" : texPath.substr(texPath.find_last_of("/\\") + 1);

            ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

            // TODO: Add texture drag-drop support when available

            return false;
        });

    // Particle texture GUID
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "textureGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Texture:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = texPath.empty() ? "None" : texPath.substr(texPath.find_last_of("/\\") + 1);

            ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

            // TODO: Add texture drag-drop support when available

            return false;
        });

    // Text font GUID
    ReflectionRenderer::RegisterFieldRenderer("TextRenderComponent", "fontGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Font:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = fontPath.empty() ? "None" : fontPath.substr(fontPath.find_last_of("/\\") + 1);

            ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FONT")) {
                    *guid = DraggedFontGuid;
                    ImGui::EndDragDropTarget();
                    return true;
                }
                ImGui::EndDragDropTarget();
            }

            return false;
        });

    // Audio GUID
    ReflectionRenderer::RegisterFieldRenderer("AudioComponent", "audioGUID",
        [](const char* name, void* ptr, Entity entity, ECSManager& ecs) {
            GUID_128* guid = static_cast<GUID_128*>(ptr);

            ImGui::Text("Audio File:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);

            std::string audioPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
            std::string displayText = audioPath.empty() ? "None" : audioPath.substr(audioPath.find_last_of("/\\") + 1);

            ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_DRAG")) {
                    *guid = DraggedAudioGuid;
                    ImGui::EndDragDropTarget();
                    return true;
                }
                ImGui::EndDragDropTarget();
            }

            return false;
        });

    // ==================== PARTICLE COMPONENT ====================
    // Add Play/Pause/Stop buttons at the beginning of ParticleComponent rendering

    ReflectionRenderer::RegisterComponentRenderer("ParticleComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            ParticleComponent& particle = *static_cast<ParticleComponent*>(componentPtr);

            // Play/Pause/Stop buttons for editor preview
            float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            if (EditorComponents::DrawPlayButton(particle.isPlayingInEditor && !particle.isPausedInEditor, buttonWidth)) {
                particle.isPlayingInEditor = true;
                particle.isPausedInEditor = false;
            }

            ImGui::SameLine();

            if (EditorComponents::DrawPauseButton(particle.isPausedInEditor, buttonWidth)) {
                if (particle.isPlayingInEditor) {
                    particle.isPausedInEditor = !particle.isPausedInEditor;
                }
            }

            if (EditorComponents::DrawStopButton()) {
                particle.isPlayingInEditor = false;
                particle.isPausedInEditor = false;
                particle.particles.clear();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Show active particle count
            ImGui::Text("Active Particles: %zu / %d", particle.particles.size(), particle.maxParticles);

            // Is Emitting checkbox (not in reflection, so we render it manually)
            ImGui::Checkbox("Is Emitting", &particle.isEmitting);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Whether the particle system is actively emitting new particles");
            }

            ImGui::Separator();

            // Continue with normal field rendering
            return false;  // Return false to continue with default field rendering
        });

    // ==================== DIRECTIONAL LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("DirectionalLightComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            DirectionalLightComponent& light = *static_cast<DirectionalLightComponent*>(componentPtr);

            // Basic properties
            ImGui::Checkbox("Enabled", &light.enabled);
            ImGui::ColorEdit3("Color", &light.color.x);
            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

            ImGui::Separator();
            ImGui::Text("Direction");

            // Direction controls with visual helper
            ImGui::DragFloat3("##Direction", &light.direction.x, 0.01f, -1.0f, 1.0f);

            // Direction visualization
            ImGui::SameLine();
            if (ImGui::Button("Normalize")) {
                light.direction = light.direction.Normalized();
            }

            // Show direction as normalized vector
            Vector3D normalizedDir = light.direction.Normalized();
            ImGui::Text("Normalized: (%.2f, %.2f, %.2f)", normalizedDir.x, normalizedDir.y, normalizedDir.z);

            // Common direction presets
            ImGui::Text("Presets:");
            if (ImGui::Button("Down")) light.direction = Vector3D(0.0f, -1.0f, 0.0f);
            ImGui::SameLine();
            if (ImGui::Button("Forward-Down")) light.direction = Vector3D(-0.2f, -1.0f, -0.3f);
            ImGui::SameLine();
            if (ImGui::Button("Side-Down")) light.direction = Vector3D(-1.0f, -1.0f, 0.0f);

            // Visual direction indicator
            ImGui::Text("Direction Visualization:");
            ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
            ImVec2 canvas_size = ImVec2(100, 100);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            // Draw a circle representing the "world"
            ImVec2 center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
            draw_list->AddCircle(center, 40.0f, IM_COL32(100, 100, 100, 255), 0, 2.0f);

            // Draw direction arrow (project 3D direction to 2D)
            Vector3D dir = light.direction.Normalized();
            ImVec2 arrow_end = ImVec2(center.x + dir.x * 35.0f, center.y + dir.y * 35.0f);
            draw_list->AddLine(center, arrow_end, IM_COL32(255, 255, 0, 255), 3.0f);

            // Arrow head
            ImVec2 arrowDir = ImVec2(arrow_end.x - center.x, arrow_end.y - center.y);
            float arrowLength = sqrt(arrowDir.x * arrowDir.x + arrowDir.y * arrowDir.y);
            if (arrowLength > 0) {
                arrowDir.x /= arrowLength;
                arrowDir.y /= arrowLength;
                ImVec2 perpendicular = ImVec2(-arrowDir.y, arrowDir.x);
                ImVec2 arrowHead1 = ImVec2(arrow_end.x - arrowDir.x * 8 + perpendicular.x * 4,
                                           arrow_end.y - arrowDir.y * 8 + perpendicular.y * 4);
                ImVec2 arrowHead2 = ImVec2(arrow_end.x - arrowDir.x * 8 - perpendicular.x * 4,
                                           arrow_end.y - arrowDir.y * 8 - perpendicular.y * 4);
                draw_list->AddLine(arrow_end, arrowHead1, IM_COL32(255, 255, 0, 255), 2.0f);
                draw_list->AddLine(arrow_end, arrowHead2, IM_COL32(255, 255, 0, 255), 2.0f);
            }

            ImGui::Dummy(canvas_size);

            ImGui::Separator();
            ImGui::Text("Lighting Properties");
            ImGui::ColorEdit3("Ambient", &light.ambient.x);
            ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
            ImGui::ColorEdit3("Specular", &light.specular.x);

            return true;  // Return true to skip default field rendering
        });

    // ==================== POINT LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("PointLightComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            PointLightComponent& light = *static_cast<PointLightComponent*>(componentPtr);

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

            return true;  // Return true to skip default field rendering
        });

    // ==================== SPOT LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("SpotLightComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            SpotLightComponent& light = *static_cast<SpotLightComponent*>(componentPtr);

            ImGui::Checkbox("Enabled", &light.enabled);
            ImGui::ColorEdit3("Color", &light.color.x);
            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat3("Direction", &light.direction.x, 0.1f, -1.0f, 1.0f);

            ImGui::Separator();
            ImGui::Text("Cone Settings");

            // Convert from cosine to degrees for easier editing
            float cutOffDegrees = glm::degrees(glm::acos(light.cutOff));
            float outerCutOffDegrees = glm::degrees(glm::acos(light.outerCutOff));

            if (ImGui::DragFloat("Inner Cutoff (degrees)", &cutOffDegrees, 1.0f, 0.0f, 90.0f)) {
                light.cutOff = glm::cos(glm::radians(cutOffDegrees));
            }
            if (ImGui::DragFloat("Outer Cutoff (degrees)", &outerCutOffDegrees, 1.0f, 0.0f, 90.0f)) {
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

            return true;
        });

    ReflectionRenderer::RegisterComponentRenderer("AnimationComponent",
        [](void* componentPtr, TypeDescriptor_Struct* typeDesc, Entity entity, ECSManager& ecs) {
            AnimationComponent& animComp = *static_cast<AnimationComponent*>(componentPtr);

            ImGui::Text("Animation Clips");

            int prevClipCount = animComp.clipCount;
            if (ImGui::InputInt("Size", &animComp.clipCount, 1, 1)) {
                if (animComp.clipCount < 0) animComp.clipCount = 0;
                if (animComp.clipCount != prevClipCount) {
                    animComp.SetClipCount(animComp.clipCount);
                }
            }

            for (int i = 0; i < animComp.clipCount; ++i) {
                ImGui::PushID(i);

                std::string slotLabel = "Element " + std::to_string(i);
                ImGui::Text("%s", slotLabel.c_str());
                ImGui::SameLine();

                std::string clipName = animComp.clipPaths[i].empty() ? "None (Animation)" : animComp.clipPaths[i];

                size_t lastSlash = clipName.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    clipName = clipName.substr(lastSlash + 1);
                }

                float buttonWidth = ImGui::GetContentRegionAvail().x;
                EditorComponents::DrawDragDropButton(clipName.c_str(), buttonWidth);

                if (EditorComponents::BeginDragDropTarget()) {
                    ImGui::SetTooltip("Drop .fbx animation file here");

                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_DRAG")) {
                        animComp.clipPaths[i] = DraggedModelPath;
                        animComp.clipGUIDs[i] = DraggedModelGuid;

                        if (ecs.HasComponent<ModelRenderComponent>(entity)) {
                            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                            if (modelComp.model) {
                                animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount());
                            }
                        }
                    }
                    EditorComponents::EndDragDropTarget();
                }

                if (!animComp.clipPaths[i].empty()) {
                    ImGui::SameLine();
                    ImGui::PushID("clear");
                    if (ImGui::SmallButton(ICON_FA_XMARK)) {
                        animComp.clipPaths[i].clear();
                        animComp.clipGUIDs[i] = {0, 0};

                        if (ecs.HasComponent<ModelRenderComponent>(entity)) {
                            auto& modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                            if (modelComp.model) {
                                animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount());
                            }
                        }
                    }
                    ImGui::PopID();
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Clear Animation");
                    }
                }

                ImGui::PopID();
            }

            const auto& clips = animComp.GetClips();
            size_t activeClipIndex = animComp.GetActiveClipIndex();

            if (!clips.empty()) {
                ImGui::Separator();
                ImGui::Text("Active Clip");

                int currentClip = static_cast<int>(activeClipIndex);
                if (ImGui::SliderInt("##ActiveClip", &currentClip, 0, static_cast<int>(clips.size()) - 1)) {
                    animComp.SetClip(currentClip);
                }

                const Animation& clip = animComp.GetClip(activeClipIndex);
                ImGui::Text("Duration: %.2f ticks", clip.GetDuration());
                ImGui::Text("Ticks Per Second: %.2f", clip.GetTicksPerSecond());
            }

            ImGui::Separator();
            ImGui::Text("Playback Controls");

            float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            if (EditorComponents::DrawPlayButton(animComp.isPlay, buttonWidth)) {
                animComp.Play();
            }

            ImGui::SameLine();

            if (EditorComponents::DrawPauseButton(!animComp.isPlay, buttonWidth)) {
                animComp.Pause();
            }

            if (EditorComponents::DrawStopButton()) {
                animComp.Stop();
            }

            if (!clips.empty() && activeClipIndex < clips.size()) {
                const Animator* animator = animComp.GetAnimatorPtr();
                if (animator) {
                    float currentTime = animator->GetCurrentTime();
                    const Animation& clip = animComp.GetClip(activeClipIndex);
                    float duration = clip.GetDuration();

                    ImGui::Separator();
                    ImGui::Text("Current Time: %.2f / %.2f", currentTime, duration);

                    float progress = duration > 0.0f ? (currentTime / duration) : 0.0f;
                    ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
                }
            }

            ImGui::Separator();

            return false;
        });
}
