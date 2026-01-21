/* Start Header ************************************************************************/
/*!
\file       InspectorCustomRenderers.cpp
\author     Lucas Yee
\date       2025
\brief      Custom field renderers for Inspector components that need special handling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include "Logging.hpp"
#include "ReflectionRenderer.hpp"
#include "ECS/ECSManager.hpp"
#include "Transform/TransformSystem.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Physics/CollisionLayers.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Math/Vector3D.hpp"
#include <glm/glm.hpp>
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"
#include "Panels/SpriteAnimationEditorWindow.hpp"
#include "Graphics/Particle/ParticleComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Sound/AudioComponent.hpp"
#include "SnapshotManager.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "Utilities/GUID.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
#include "EditorState.hpp"
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include "ECS/SortingLayerManager.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Animation/AnimatorController.hpp"
#include "Panels/AnimatorEditorWindow.hpp"
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <shobjidl.h>
// Undefine Windows macros that conflict with our code
#undef GetCurrentTime
#endif
#include "Game AI/BrainComponent.hpp"
#include "Game AI/BrainFactory.hpp"
#include "Script/ScriptComponentData.hpp"
#include "UI/Button/ButtonComponent.hpp"
#include "UI/Slider/SliderComponent.hpp"
#include "UI/Anchor/UIAnchorComponent.hpp"
#include "Scripting.h"
#include "ScriptInspector.h"
#include "Panels/TagsLayersPanel.hpp"
#include "Panels/PanelManager.hpp"
#include "GUIManager.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#include "imgui.h"
#include "EditorComponents.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include "UndoableWidgets.hpp"
#include <glm/glm.hpp>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <Panels/MaterialInspector.hpp>

// External drag-drop state
extern GUID_128 DraggedModelGuid;
extern std::string DraggedModelPath;
extern GUID_128 DraggedMaterialGuid;
extern std::string DraggedMaterialPath;
extern GUID_128 DraggedAudioGuid;
extern std::string DraggedAudioPath;
extern GUID_128 DraggedFontGuid;
extern std::string DraggedFontPath;
extern GUID_128 DraggedScriptGuid;
extern std::string DraggedScriptPath;

// Helper function to determine asset type from field name
enum class AssetType { None, Audio, Model, Texture, Material, Font, Script };
AssetType GetAssetTypeFromFieldName(const std::string& fieldName) {
    std::string lowerName = fieldName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (lowerName.find("audio") != std::string::npos || lowerName.find("sfx") != std::string::npos || lowerName.find("sound") != std::string::npos) {
        return AssetType::Audio;
    }
    if (lowerName.find("model") != std::string::npos) {
        return AssetType::Model;
    }
    if (lowerName.find("texture") != std::string::npos || lowerName.find("sprite") != std::string::npos) {
        return AssetType::Texture;
    }
    if (lowerName.find("material") != std::string::npos) {
        return AssetType::Material;
    }
    if (lowerName.find("font") != std::string::npos) {
        return AssetType::Font;
    }
    if (lowerName.find("script") != std::string::npos) {
        return AssetType::Script;
    }
    return AssetType::None;
}

// Helper function to check if a string is a valid GUID
bool IsValidGUID(const std::string& str) {
    if (str.length() != 36) return false;
    // Basic GUID format check: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    return str[8] == '-' && str[13] == '-' && str[18] == '-' && str[23] == '-';
}

// Helper function to render asset drag-drop for a single GUID
bool RenderAssetField(const std::string& fieldName, std::string& guidStr, AssetType assetType, float width = -1.0f) {
    // Commented out to fix warning C4100 - unreferenced parameter
    // Remove this line when 'fieldName' is used
    (void)fieldName;

    bool modified = false;
    std::string displayText;
    
    switch (assetType) {
        case AssetType::Audio: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Audio File)" : path.substr(path.find_last_of("/\\") + 1);
            break;
        }
        case AssetType::Model: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Model)" : path.substr(path.find_last_of("/\\") + 1);
            break;
        }
        case AssetType::Texture: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Texture)" : path.substr(path.find_last_of("/\\") + 1);
            break;
        }
        case AssetType::Material: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Material)" : path.substr(path.find_last_of("/\\") + 1);
            break;
        }
        case AssetType::Font: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Font)" : path.substr(path.find_last_of("/\\") + 1);
            break;
        }
        case AssetType::Script: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Script)" : path.substr(path.find_last_of("/\\") + 1);
            break;
        }
        default:
            return false;
    }
    
    EditorComponents::DrawDragDropButton(displayText.c_str(), width);
    
    // Handle drag-drop
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = nullptr;
        const char* payloadType = nullptr;
        switch (assetType) {
            case AssetType::Audio: payloadType = "AUDIO_DRAG"; break;
            case AssetType::Model: payloadType = "MODEL_DRAG"; break;
            case AssetType::Texture: payloadType = "TEXTURE_PAYLOAD"; break;
            case AssetType::Material: payloadType = "MATERIAL_DRAG"; break;
            case AssetType::Font: payloadType = "FONT_DRAG"; break;
            case AssetType::Script: payloadType = "SCRIPT_PAYLOAD"; break;
            default: break;
        }
        
        if (payloadType && (payload = ImGui::AcceptDragDropPayload(payloadType))) {
            GUID_128 newGuid;
            if (assetType == AssetType::Texture) {
                // For texture, get path from payload data
                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());
                newGuid = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
            } else {
                // For others, use the extern variables
                switch (assetType) {
                    case AssetType::Audio: newGuid = DraggedAudioGuid; break;
                    case AssetType::Model: newGuid = DraggedModelGuid; break;
                    case AssetType::Material: newGuid = DraggedMaterialGuid; break;
                    case AssetType::Font: newGuid = DraggedFontGuid; break;
                    case AssetType::Script: newGuid = DraggedScriptGuid; break;
                    default: break;
                }
            }
            guidStr = GUIDUtilities::ConvertGUID128ToString(newGuid);
            modified = true;
        }
        ImGui::EndDragDropTarget();
    }
    
    return modified;
}

// Forward declaration for sprite animation inspector
void RegisterSpriteAnimationInspector();

void RegisterInspectorCustomRenderers()
{
    // ==================== CUSTOM TYPE RENDERERS ====================
    // Register custom renderer for glm::vec3 (used by CameraComponent and others)

    ReflectionRenderer::RegisterCustomRenderer("glm::vec3",
    [](const char *name, void *ptr, Entity, ECSManager &)
    {
        glm::vec3 *vec = static_cast<glm::vec3 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Convert field name from camelCase to "Proper Case"
        std::string displayName = name;
        if (!displayName.empty())
        {
            displayName[0] = static_cast<char>(std::toupper(displayName[0]));
            for (size_t i = 1; i < displayName.size(); ++i)
            {
                if (std::isupper(displayName[i]) && i > 0 && std::islower(displayName[i - 1]))
                {
                    displayName.insert(i, " ");
                    i++;
                }
            }
        }

        ImGui::Text("%s", displayName.c_str());
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        float values[3] = {vec->x, vec->y, vec->z};
        std::string id = std::string("##") + name + "_" + std::to_string(reinterpret_cast<uintptr_t>(ptr));

        // Use UndoableWidgets wrapper - handles undo/redo automatically!
        bool modified = UndoableWidgets::DragFloat3(id.c_str(), values, 0.1f);

        if (modified)
        {
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
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        ecs;
        NameComponent &nameComp = *static_cast<NameComponent *>(componentPtr);

        if (ecs.HasComponent<ActiveComponent>(entity))
        {
            auto &activeComp = ecs.GetComponent<ActiveComponent>(entity);

            // Style the checkbox to be smaller with white checkmark
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));                  // Smaller padding
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));      // White checkmark
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));        // Dark gray background
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Lighter on hover
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Even lighter when clicking

            // Use UndoableWidgets wrapper for automatic undo/redo
            UndoableWidgets::Checkbox("##EntityActive", &activeComp.isActive);

            ImGui::PopStyleColor(4); // Pop all 4 colors
            ImGui::PopStyleVar();    // Pop padding

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Enable/Disable Entity");
            }
            ImGui::SameLine();
        }

        // Simple text input for name (no collapsing header)
        char buf[128] = {};
        std::snprintf(buf, sizeof(buf), "%s", nameComp.name.c_str());
        if (UndoableWidgets::InputText("Name", buf, sizeof(buf)))
        {
            nameComp.name = buf;
        }

        return true; // Skip default rendering (we rendered everything)
    });

    // ==================== TAG COMPONENT ====================
    // Tag component uses TagManager dropdown (rendered inline with Layer)

    ReflectionRenderer::RegisterComponentRenderer("TagComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        TagComponent &tagComp = *static_cast<TagComponent *>(componentPtr);

        // Get available tags
        const auto &availableTags = TagManager::GetInstance().GetAllTags();

        // Create items for combo box, including "Add Tag..." option
        std::vector<std::string> tagItems;
        tagItems.reserve(availableTags.size() + 1);
        for (const auto &tag : availableTags)
        {
        tagItems.push_back(tag);
        }
        tagItems.push_back("Add Tag...");

        // Convert to const char* array for ImGui
        std::vector<const char *> tagItemPtrs;
        tagItemPtrs.reserve(tagItems.size());
        for (const auto &item : tagItems)
        {
        tagItemPtrs.push_back(item.c_str());
        }

        // Ensure tagIndex is valid
        if (tagComp.tagIndex < 0 || tagComp.tagIndex >= static_cast<int>(availableTags.size()))
        {
        tagComp.tagIndex = 0; // Default to first tag
        }

        // Inline rendering (no label, just combo) - using UndoableWidgets
        ImGui::Text("Tag");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        int currentTag = tagComp.tagIndex;
        if (UndoableWidgets::Combo("##Tag", &currentTag, tagItemPtrs.data(), static_cast<int>(tagItemPtrs.size())))
        {
        if (currentTag >= 0 && currentTag < static_cast<int>(availableTags.size()))
        {
        tagComp.tagIndex = currentTag;
        }
        else if (currentTag == static_cast<int>(availableTags.size()))
        {
        // "Add Tag..." was selected - open Tags & Layers window
        auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
        if (tagsLayersPanel) {
            tagsLayersPanel->SetOpen(true);
        }
        // Reset selection to current tag
        currentTag = tagComp.tagIndex;
        }
        }

        ImGui::SameLine(); // Keep Layer on same line

        return true; // Skip default rendering
    });

    // ==================== LAYER COMPONENT ====================
    // Layer component uses LayerManager dropdown (rendered inline with Tag)

    ReflectionRenderer::RegisterComponentRenderer("LayerComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        LayerComponent &layerComp = *static_cast<LayerComponent *>(componentPtr);

        // Get available layers
        const auto &availableLayers = LayerManager::GetInstance().GetAllLayers();

        // Create items for combo box (only show named layers)
        std::vector<std::string> layerItems;
        std::vector<int> layerIndices;
        for (int i = 0; i < LayerManager::MAX_LAYERS; ++i)
        {
            const std::string &layerName = availableLayers[i];
            if (!layerName.empty())
            {
                layerItems.push_back(std::to_string(i) + ": " + layerName);
                layerIndices.push_back(i);
            }
        }

        // Add "Add Layer..." option
        layerItems.push_back("Add Layer...");
        std::vector<int> tempIndices = layerIndices;
        tempIndices.push_back(-1); // Special value for "Add Layer..."

        // Convert to const char* array for ImGui
        std::vector<const char *> layerItemPtrs;
        layerItemPtrs.reserve(layerItems.size());
        for (const auto &item : layerItems)
        {
            layerItemPtrs.push_back(item.c_str());
        }

        // Ensure layerIndex is valid
        if (layerComp.layerIndex < 0 || layerComp.layerIndex >= LayerManager::MAX_LAYERS)
        {
            layerComp.layerIndex = 0; // Default to first layer
        }

        // Find current selection index in our filtered list
        int currentSelection = -1;
        for (size_t i = 0; i < layerIndices.size(); ++i)
        {
            if (layerIndices[i] == layerComp.layerIndex)
            {
                currentSelection = static_cast<int>(i);
                break;
            }
        }

        // If current layer is not in the named list, default to first
        if (currentSelection == -1 && !layerIndices.empty())
        {
            currentSelection = 0;
            layerComp.layerIndex = layerIndices[0];
        }

        // Inline rendering (continues from Tag on same line) - using UndoableWidgets
        ImGui::Text("Layer");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (UndoableWidgets::Combo("##Layer", &currentSelection, layerItemPtrs.data(), static_cast<int>(layerItemPtrs.size())))
        {
            if (currentSelection >= 0 && currentSelection < static_cast<int>(tempIndices.size()))
            {
                int selectedIndex = tempIndices[currentSelection];
                if (selectedIndex != -1)
                {
                    layerComp.layerIndex = selectedIndex;
                }
                else
                {
                    // "Add Layer..." was selected - open Tags & Layers window
                    auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
                    if (tagsLayersPanel) {
                        tagsLayersPanel->SetOpen(true);
                    }
                    // Reset selection to current layer
                    currentSelection = -1; // or find the current
                    for (size_t i = 0; i < layerIndices.size(); ++i)
                    {
                        if (layerIndices[i] == layerComp.layerIndex)
                        {
                            currentSelection = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }
        }

        ImGui::Separator(); // Add separator after Tag/Layer line

        return true; // Skip default rendering
    });

    // ==================== TRANSFORM COMPONENT ====================
    // Transform needs to use TransformSystem for setting values

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localPosition",
    [](const char *name, void *ptr, Entity entity, ECSManager &ecs)
    {
        // Commented out to fix warning C4100 - unreferenced parameters
        // Remove these lines when 'name' and 'ecs' are used
        (void)name;
        (void)ecs;
        Vector3D *pos = static_cast<Vector3D *>(ptr);
        float arr[3] = {pos->x, pos->y, pos->z};
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Position");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Use UndoableWidgets wrapper for automatic undo/redo
        bool changed = UndoableWidgets::DragFloat3("##Position", arr, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");

        if (changed)
        {
        ecs.transformSystem->SetLocalPosition(entity, {arr[0], arr[1], arr[2]});
        return true;
        }
        return false;
    });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localRotation",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Quaternion *quat = static_cast<Quaternion *>(ptr);
        Vector3D euler = quat->ToEulerDegrees();
        float arr[3] = {euler.x, euler.y, euler.z};
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Rotation");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Use UndoableWidgets wrapper for automatic undo/redo
        bool changed = UndoableWidgets::DragFloat3("##Rotation", arr, 1.0f, -180.0f, 180.0f, "%.1f");

        if (changed)
        {
        ecs.transformSystem->SetLocalRotation(entity, {arr[0], arr[1], arr[2]});
        return true;
        }
        return false;
    });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localScale",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Vector3D *scale = static_cast<Vector3D *>(ptr);
        float arr[3] = {scale->x, scale->y, scale->z};
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Scale");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Use UndoableWidgets wrapper for automatic undo/redo
        bool changed = UndoableWidgets::DragFloat3("##Scale", arr, 0.1f, 0.001f, FLT_MAX, "%.3f");

        if (changed)
        {
            ecs.transformSystem->SetLocalScale(entity, {arr[0], arr[1], arr[2]});
            return true;
        }
        return false;
    });

    // ==================== COLLIDER COMPONENT ====================
    // ColliderComponent needs custom rendering for shape type and parameters

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "shapeTypeID",
    [](const char *, void *, Entity entity, ECSManager &ecs)
    {
        ecs;
        auto &collider = ecs.GetComponent<ColliderComponent>(entity);
        auto &rc = ecs.GetComponent<ModelRenderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Shape Type");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char *shapeTypes[] = {"Box", "Sphere", "Capsule", "Cylinder", "MeshShape"};
        int currentShapeType = static_cast<int>(collider.shapeType);

        EditorComponents::PushComboColors();
        bool changed = UndoableWidgets::Combo("##ShapeType", &currentShapeType, shapeTypes, 5);
        EditorComponents::PopComboColors();

        ImGui::Text("Collider Offset");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        float colliderOffset[3] = { collider.offset.x, collider.offset.y, collider.offset.z };
        if (UndoableWidgets::DragFloat3("##ColliderOffset", colliderOffset, 0.05f, -FLT_MAX, FLT_MAX, "%.2f"))
        {
            collider.offset = Vector3D(colliderOffset[0], colliderOffset[1], colliderOffset[2]);
        }



        if (changed)
        {
            collider.shapeType = static_cast<ColliderShapeType>(currentShapeType);
            collider.shapeTypeID = currentShapeType;
            collider.version++;
        }

        // Shape Parameters based on type
        bool shapeParamsChanged = false;
        Vector3D halfExtent = rc.CalculateModelHalfExtent(*rc.model);
        float radius = rc.CalculateModelRadius(*rc.model);

        switch (collider.shapeType)
        {
        case ColliderShapeType::Box:
        {
            ImGui::Text("Half Extents");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            //collider.boxHalfExtents = halfExtent;
            float halfExtents[3] = {collider.boxHalfExtents.x, collider.boxHalfExtents.y, collider.boxHalfExtents.z};
            if (UndoableWidgets::DragFloat3("##HalfExtents", halfExtents, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                collider.boxHalfExtents = Vector3D(halfExtents[0], halfExtents[1], halfExtents[2]);
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::Sphere:
        {
            ImGui::Text("Radius");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            //collider.sphereRadius = radius;
            if (UndoableWidgets::DragFloat("##SphereRadius", &collider.sphereRadius, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::Capsule:
        {            
            ImGui::Text("Radius");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            //collider.capsuleRadius = std::min(halfExtent.x, halfExtent.z);
            if (UndoableWidgets::DragFloat("##CapsuleRadius", &collider.capsuleRadius, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            ImGui::Text("Half Height");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            //collider.capsuleHalfHeight = halfExtent.y;
            if (UndoableWidgets::DragFloat("##CapsuleHalfHeight", &collider.capsuleHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::Cylinder:
        {
            ImGui::Text("Radius");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            //collider.cylinderRadius = std::min(halfExtent.x, halfExtent.z);
            if (UndoableWidgets::DragFloat("##CylinderRadius", &collider.cylinderRadius, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            ImGui::Text("Half Height");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            //collider.cylinderHalfHeight = halfExtent.y;
            if (UndoableWidgets::DragFloat("##CylinderHalfHeight", &collider.cylinderHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::MeshShape:
        {
            ImGui::Text("Mesh Shape");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            ImGui::TextDisabled("Uses Model Geometry");
        }
        }

        if (shapeParamsChanged)
        {
            collider.version++;
        }

        return changed || shapeParamsChanged;
    });

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "layerID",
    [](const char *, void *, Entity entity, ECSManager &ecs)
    {
        ecs;
        auto &collider = ecs.GetComponent<ColliderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Layer");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char *layers[] = {"Non-Moving", "Moving", "Sensor", "Debris"};
        int currentLayer = static_cast<int>(collider.layer);

        EditorComponents::PushComboColors();
        bool changed = UndoableWidgets::Combo("##Layer", &currentLayer, layers, 4);
        EditorComponents::PopComboColors();

        if (changed)
        {
            collider.layer = static_cast<JPH::ObjectLayer>(currentLayer);
            collider.layerID = currentLayer;
            collider.version++;
        }

        return changed;
    });

    // Skip non-reflected fields (these are handled with shapeTypeID)
    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "boxHalfExtents",
    [](const char *, void *, Entity, ECSManager &ecs)
    { 
        ecs;
        return false; 
    });

    // ==================== RIGIDBODY COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("RigidBodyComponent",
    [](void *, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)  
    {
        ecs;
        auto &rigidBody = ecs.GetComponent<RigidBodyComponent>(entity);
        auto &transform = ecs.GetComponent<Transform>(entity); // for info tab

        ImGui::PushID("RigidBodyComponent");
        const float labelWidth = EditorComponents::GetLabelWidth();

        // --- Motion Type dropdown ---
        ImGui::Text("Motion");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char *motionTypes[] = {"Static", "Kinematic", "Dynamic"};
        int currentMotion = rigidBody.motionID;
        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##MotionType", &currentMotion, motionTypes, IM_ARRAYSIZE(motionTypes)))
        {
            rigidBody.motion = static_cast<Motion>(currentMotion);
            rigidBody.motionID = currentMotion;
            rigidBody.motion_dirty = true; // mark for recreation
        }
        EditorComponents::PopComboColors();

        // --- Is Trigger checkbox ---
        ImGui::AlignTextToFramePadding();
        UndoableWidgets::Checkbox("##IsTrigger", &rigidBody.isTrigger);
        ImGui::SameLine();
        ImGui::Text("Is Trigger");

        if (rigidBody.motion == Motion::Dynamic)
        {
            // --- CCD checkbox ---
            ImGui::AlignTextToFramePadding();
            if (UndoableWidgets::Checkbox("##CCD", &rigidBody.ccd))
            {
                rigidBody.motion_dirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("CCD");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Continuous Collision Detection - prevents fast-moving objects from tunneling");

            // --- Linear & Angular Damping ---
            ImGui::Text("Linear Damping");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##LinearDamping", &rigidBody.linearDamping, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            ImGui::Text("Angular Damping");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##AngularDamping", &rigidBody.angularDamping, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            // --- Gravity Factor ---
            ImGui::Text("Gravity Factor");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##GravityFactor", &rigidBody.gravityFactor, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");
        }

        // --- Info Section (Read-only) ---
        if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginDisabled();

            // Position
            ImGui::Text("Position");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            float position[3] = {transform.localPosition.x, transform.localPosition.y, transform.localPosition.z};
            ImGui::DragFloat3("##Position", position, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");

            // Rotation
            ImGui::Text("Rotation");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            float rotation[3] = {transform.localRotation.x, transform.localRotation.y, transform.localRotation.z};
            ImGui::DragFloat3("##Rotation", rotation, 1.0f, -180.0f, 180.0f, "%.3f");

            // Linear Velocity
            ImGui::Text("Linear Velocity");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            float linearVel[3] = {rigidBody.linearVel.x, rigidBody.linearVel.y, rigidBody.linearVel.z};
            ImGui::DragFloat3("##LinearVelocity", linearVel, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            // Angular Velocity
            ImGui::Text("Angular Velocity");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            float angularVel[3] = {rigidBody.angularVel.x, rigidBody.angularVel.y, rigidBody.angularVel.z};
            ImGui::DragFloat3("##AngularVelocity", angularVel, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            ImGui::EndDisabled();
        }

        ImGui::PopID();
        return true; // skip default reflection
    });

    // ==================== CAMERA COMPONENT ====================
    // Camera needs special handling for enum and glm::vec3 properties

    ReflectionRenderer::RegisterComponentRenderer("CameraComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        CameraComponent &camera = *static_cast<CameraComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Manually render the non-reflected properties first

        // Projection Type dropdown - using UndoableWidgets
        ImGui::Text("Projection");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char *projTypes[] = {"Perspective", "Orthographic"};
        int currentProj = static_cast<int>(camera.projectionType);

        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##Projection", &currentProj, projTypes, 2))
        {
            camera.projectionType = static_cast<ProjectionType>(currentProj);
        }
        EditorComponents::PopComboColors();

        // Target (glm::vec3)
        ImGui::Text("Target");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float target[3] = {camera.target.x, camera.target.y, camera.target.z};
        if (UndoableWidgets::DragFloat3("##Target", target, 0.1f))
        {
            camera.target = glm::vec3(target[0], target[1], target[2]);
        }

        // Up (glm::vec3)
        ImGui::Text("Up");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float up[3] = {camera.up.x, camera.up.y, camera.up.z};
        if (UndoableWidgets::DragFloat3("##Up", up, 0.1f))
        {
            camera.up = glm::vec3(up[0], up[1], up[2]);
        }

        ImGui::Text("Clear Flags");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char* clearFlagsOptions[] = {"Skybox", "Solid Color", "Depth Only", "Don't Clear"};
        int currentClearFlags = static_cast<int>(camera.clearFlags);
        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##ClearFlags", &currentClearFlags, clearFlagsOptions, 4))
        {
            camera.clearFlags = static_cast<CameraClearFlags>(currentClearFlags);
        }
        EditorComponents::PopComboColors();

        ImGui::Text("Background");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float bgColor[3] = {camera.backgroundColor.r, camera.backgroundColor.g, camera.backgroundColor.b};
        if (UndoableWidgets::ColorEdit3("##Background", bgColor))
        {
            camera.backgroundColor = glm::vec3(bgColor[0], bgColor[1], bgColor[2]);
        }

        return false;
    });

    // ==================== GUID FIELDS WITH DRAG-DROP ====================
    // Model GUID drag-drop

    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "modelGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Model");
        ImGui::SameLine(labelWidth);

        // Display current model path or "None"
        std::string modelPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = modelPath.empty() ? "None (Model)" : modelPath.substr(modelPath.find_last_of("/\\") + 1);

        // Use EditorComponents for better drag-drop visual feedback
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        // Drag-drop target with proper payload type
        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop .obj, .fbx, .dae, or .3ds model here");
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MODEL_DRAG"))
            {
                // Take snapshot before changing model
                SnapshotManager::GetInstance().TakeSnapshot("Assign Model");

                // Load and apply the model
                auto &modelRenderer = ecs.GetComponent<ModelRenderComponent>(entity);

                std::cout << "[Inspector] Applying model - GUID: {" << DraggedModelGuid.high << ", " << DraggedModelGuid.low << "}, Path: " << DraggedModelPath << std::endl;

                try
                {
                    // Load model using ResourceManager
                    std::shared_ptr<Model> loadedModel = nullptr;
                    if (DraggedModelGuid.high != 0 || DraggedModelGuid.low != 0)
                    {
                        loadedModel = ResourceManager::GetInstance().GetResourceFromGUID<Model>(DraggedModelGuid, DraggedModelPath);
                    }
                    else if (!DraggedModelPath.empty())
                    {
                        loadedModel = ResourceManager::GetInstance().GetResource<Model>(DraggedModelPath);
                    }

                    if (loadedModel)
                    {
                        std::cout << "[Inspector] Model loaded successfully!" << std::endl;
                        modelRenderer.model = loadedModel;
                        modelRenderer.modelGUID = DraggedModelGuid;

                        // Load default shader if not already set
                        if (!modelRenderer.shader)
                        {
                            modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(
                                ResourceManager::GetPlatformShaderPath("default"));
                            modelRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(
                                ResourceManager::GetPlatformShaderPath("default"));
                        }

                        if (loadedModel->meshes[0].material)
                        {
                            modelRenderer.material = loadedModel->meshes[0].material;
                            std::string materialPath = AssetManager::GetInstance().GetAssetPathFromAssetName(modelRenderer.material->GetName() + ".mat");
                            modelRenderer.materialGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(materialPath);
                        }
                    }
                    else
                    {
                        std::cerr << "[Inspector] Failed to load model!" << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
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
    [](const char *, void *ptr, Entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Shader");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = shaderPath.empty() ? "None" : shaderPath.substr(shaderPath.find_last_of("/\\") + 1);

        ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

        // TODO: Add shader drag-drop support when available

        return false;
    });

    // Material GUID drag-drop
    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "materialGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Material");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        std::string materialPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = materialPath.empty() ? "None" : materialPath.substr(materialPath.find_last_of("/\\") + 1);

        EditorComponents::DrawDragDropButton(displayText.c_str(), -1);

        // Material drag-drop target
        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop material here to apply to model");
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MATERIAL_DRAG"))
            {
                // Take snapshot before changing material
                SnapshotManager::GetInstance().TakeSnapshot("Assign Material");
                *guid = DraggedMaterialGuid;
                // Try GUID first, then fallback to path
                if (DraggedMaterialGuid.high != 0 || DraggedMaterialGuid.low != 0) {
                    MaterialInspector::ApplyMaterialToModel(entity, DraggedMaterialGuid);
                }
                else {
                    MaterialInspector::ApplyMaterialToModelByPath(entity, DraggedMaterialPath);
                }
                EditorComponents::EndDragDropTarget();
                return true;
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Sprite texture GUID
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "textureGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Texture");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = texPath.empty() ? "None (Texture)" : texPath.substr(texPath.find_last_of("/\\") + 1);

        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop texture file here");

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD"))
            {
                // Take snapshot before changing texture
                SnapshotManager::GetInstance().TakeSnapshot("Assign Texture");

                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                GUID_128 textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                *guid = textureGUID;

                // Load texture immediately
                auto &spriteComp = ecs.GetComponent<SpriteRenderComponent>(entity);
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
                spriteComp.texturePath = newTexturePath;
                spriteComp.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, newTexturePath);

                EditorComponents::EndDragDropTarget();
                return true; // Field was modified
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Hide childBonesSaved from ModelRenderComponent (should not be modified in editor).
    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "childBonesSaved",
        [](const char*, void*, Entity, ECSManager&)
        { return true; });

    // Hide position, scale, rotation from SpriteRenderComponent (controlled by Transform)
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "position",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "scale",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "rotation",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "saved3DPosition",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    // Sprite sorting layer dropdown
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "sortingLayer",
    [](const char*, void* ptr, Entity, ECSManager& ecs)
    {
        ecs;
        int* sortingLayerID = static_cast<int*>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Sorting Layer");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Get all sorting layers from the manager
        const auto& sortingLayers = SortingLayerManager::GetInstance().GetAllLayers();

        // Find current layer name
        std::string currentLayerName = SortingLayerManager::GetInstance().GetLayerName(*sortingLayerID);
        if (currentLayerName.empty()) {
            currentLayerName = "Default";
            *sortingLayerID = 0; // Reset to default if invalid
        }

        EditorComponents::PushComboColors();
        bool changed = false;
        if (ImGui::BeginCombo("##SpriteSortingLayer", currentLayerName.c_str()))
        {
            // Show all existing sorting layers
            for (const auto& layer : sortingLayers) {
                bool isSelected = (*sortingLayerID == layer.id);
                if (ImGui::Selectable(layer.name.c_str(), isSelected)) {
                    SnapshotManager::GetInstance().TakeSnapshot("Change Sorting Layer");
                    *sortingLayerID = layer.id;
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::Separator();

            // "Add Sorting Layer..." option
            if (ImGui::Selectable("Add Sorting Layer...")) {
                // Open the Tags & Layers panel
                auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
                if (tagsLayersPanel) {
                    tagsLayersPanel->SetOpen(true);
                }
            }

            ImGui::EndCombo();
        }
        EditorComponents::PopComboColors();

        return changed;
    });

    // Camera skybox texture GUID
    ReflectionRenderer::RegisterFieldRenderer("CameraComponent", "skyboxTextureGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Skybox Texture");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = texPath.empty() ? "None (Texture)" : texPath.substr(texPath.find_last_of("/\\") + 1);

        bool hasTexture = (guid->high != 0 || guid->low != 0);
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float buttonWidth = hasTexture ? availableWidth - 30.0f : availableWidth;

        ImGui::SetNextItemWidth(buttonWidth);
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop texture file here");

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD"))
            {
                // Take snapshot before changing texture
                SnapshotManager::GetInstance().TakeSnapshot("Assign Skybox Texture");

                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                GUID_128 textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                *guid = textureGUID;

                // Load texture immediately
                auto &cameraComp = ecs.GetComponent<CameraComponent>(entity);
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
                cameraComp.skyboxTexturePath = newTexturePath;
                cameraComp.skyboxTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, newTexturePath);

                EditorComponents::EndDragDropTarget();
                return true; // Field was modified
            }
            EditorComponents::EndDragDropTarget();
        }

        auto &cameraComp = ecs.GetComponent<CameraComponent>(entity);

        if (guid->high != 0 || guid->low != 0)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_XMARK "##ClearSkybox"))
            {
                SnapshotManager::GetInstance().TakeSnapshot("Clear Skybox Texture");

                *guid = GUID_128{0, 0};
                cameraComp.skyboxTexturePath.clear();
                cameraComp.skyboxTexture = nullptr;

                return true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear skybox texture");
            }

            if (!cameraComp.skyboxTexture || cameraComp.skyboxTexturePath.empty())
            {
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
                if (!newTexturePath.empty())
                {
                    cameraComp.skyboxTexturePath = newTexturePath;
                    cameraComp.skyboxTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(*guid, newTexturePath);
                }
            }
        }
        else
        {
            if (cameraComp.skyboxTexture != nullptr || !cameraComp.skyboxTexturePath.empty())
            {
                cameraComp.skyboxTexturePath.clear();
                cameraComp.skyboxTexture = nullptr;
            }
        }

        return false;
    });

    // Custom color picker for SpriteRenderComponent
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "color",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Vector3D *color = static_cast<Vector3D *>(ptr);
        auto &sprite = ecs.GetComponent<SpriteRenderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Convert to 0-255 range for display, combine with alpha
        float colorRGBA[4] = {
            color->x,
            color->y,
            color->z,
            sprite.alpha};

        ImGui::Text("Color:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        if (UndoableWidgets::ColorEdit4("##Color", colorRGBA, ImGuiColorEditFlags_Uint8))
        {
            color->x = colorRGBA[0];
            color->y = colorRGBA[1];
            color->z = colorRGBA[2];
            sprite.alpha = colorRGBA[3];
        }

        return true; // Skip default rendering
    });

    // Hide alpha from SpriteRenderComponent (it's in the color picker now)
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "alpha",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    // Particle texture GUID
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "textureGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Texture");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = texPath.empty() ? "None (Texture)" : texPath.substr(texPath.find_last_of("/\\") + 1);

        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop texture file here");

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD"))
            {
                // Take snapshot before changing texture
                SnapshotManager::GetInstance().TakeSnapshot("Assign Texture");

                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                GUID_128 textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                *guid = textureGUID;

                // Load texture immediately
                auto &particleComp = ecs.GetComponent<ParticleComponent>(entity);
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
                particleComp.particleTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, newTexturePath);

                EditorComponents::EndDragDropTarget();
                return true; // Field was modified
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Text font GUID
    ReflectionRenderer::RegisterFieldRenderer("TextRenderComponent", "fontGUID",
    [](const char *, void *ptr, Entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Font");
        ImGui::SameLine(labelWidth);

        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = fontPath.empty() ? "None (Font)" : fontPath.substr(fontPath.find_last_of("/\\") + 1);

        // Use EditorComponents for better drag-drop visual feedback
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        // Drag-drop target with proper payload type
        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop .ttf font here");
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("FONT_PAYLOAD"))
            {
                // Take snapshot before changing font
                SnapshotManager::GetInstance().TakeSnapshot("Assign Font");
                *guid = DraggedFontGuid;
                EditorComponents::EndDragDropTarget();
                return true;
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Text sorting layer dropdown
    ReflectionRenderer::RegisterFieldRenderer("TextRenderComponent", "sortingLayer",
    [](const char*, void* ptr, Entity, ECSManager& ecs)
    {
        ecs;
        int* sortingLayerID = static_cast<int*>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Sorting Layer");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Get all sorting layers from the manager
        const auto& sortingLayers = SortingLayerManager::GetInstance().GetAllLayers();

        // Find current layer name
        std::string currentLayerName = SortingLayerManager::GetInstance().GetLayerName(*sortingLayerID);
        if (currentLayerName.empty()) {
            currentLayerName = "Default";
            *sortingLayerID = 0; // Reset to default if invalid
        }

        EditorComponents::PushComboColors();
        bool changed = false;
        if (ImGui::BeginCombo("##SortingLayer", currentLayerName.c_str()))
        {
            // Show all existing sorting layers
            for (const auto& layer : sortingLayers) {
                bool isSelected = (*sortingLayerID == layer.id);
                if (ImGui::Selectable(layer.name.c_str(), isSelected)) {
                    SnapshotManager::GetInstance().TakeSnapshot("Change Sorting Layer");
                    *sortingLayerID = layer.id;
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::Separator();

            // "Add Sorting Layer..." option
            if (ImGui::Selectable("Add Sorting Layer...")) {
                // Open the Tags & Layers panel
                auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
                if (tagsLayersPanel) {
                    tagsLayersPanel->SetOpen(true);
                }
            }

            ImGui::EndCombo();
        }
        EditorComponents::PopComboColors();

        return changed;
    });

    // Text alignment icon buttons
    ReflectionRenderer::RegisterFieldRenderer("TextRenderComponent", "alignmentInt",
    [](const char*, void* ptr, Entity, ECSManager& ecs)
    {
        ecs;
        int* alignmentInt = static_cast<int*>(ptr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Alignment");
        ImGui::SameLine(labelWidth);

        bool changed = false;

        // Calculate button size for even distribution
        float availWidth = ImGui::GetContentRegionAvail().x;
        float buttonWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

        // Left align button
        ImVec4 leftColor = (*alignmentInt == 0) ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, leftColor);
        if (ImGui::Button("Left", ImVec2(buttonWidth, 0))) {
            if (*alignmentInt != 0) {
                SnapshotManager::GetInstance().TakeSnapshot("Change Text Alignment");
                *alignmentInt = 0;
                changed = true;
            }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Center align button
        ImVec4 centerColor = (*alignmentInt == 1) ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, centerColor);
        if (ImGui::Button("Center", ImVec2(buttonWidth, 0))) {
            if (*alignmentInt != 1) {
                SnapshotManager::GetInstance().TakeSnapshot("Change Text Alignment");
                *alignmentInt = 1;
                changed = true;
            }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Right align button
        ImVec4 rightColor = (*alignmentInt == 2) ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, rightColor);
        if (ImGui::Button("Right", ImVec2(buttonWidth, 0))) {
            if (*alignmentInt != 2) {
                SnapshotManager::GetInstance().TakeSnapshot("Change Text Alignment");
                *alignmentInt = 2;
                changed = true;
            }
        }
        ImGui::PopStyleColor();

        return changed;
    });

    // Audio GUID
    ReflectionRenderer::RegisterComponentRenderer("AudioComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        AudioComponent &audio = *static_cast<AudioComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Audio Resource field
        ImGui::Text("Audio File:");
        ImGui::SameLine(labelWidth);
        std::string audioPath = AssetManager::GetInstance().GetAssetPathFromGUID(audio.audioGUID);
        std::string displayText = audioPath.empty() ? "None (Audio File)" : audioPath.substr(audioPath.find_last_of("/\\") + 1);
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("AUDIO_DRAG"))
            {
                // Take snapshot before changing audio clip
                SnapshotManager::GetInstance().TakeSnapshot("Assign Audio Clip");
                audio.SetClip(DraggedAudioGuid);
                ImGui::EndDragDropTarget();
                return true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();

        // Output section
        ImGui::Text("Output");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        char outputBuf[128];
        std::snprintf(outputBuf, sizeof(outputBuf), "%s", audio.OutputAudioMixerGroup.empty() ? "None (Audio Mixer Group)" : audio.OutputAudioMixerGroup.c_str());
        if (UndoableWidgets::InputText("##Output", outputBuf, sizeof(outputBuf)))
        {
            audio.OutputAudioMixerGroup = outputBuf;
        }

        // Checkboxes (aligned with labels) - using UndoableWidgets
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Mute");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Mute", &audio.Mute);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Bypass Listener Effects");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##BypassListenerEffects", &audio.bypassListenerEffects);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Play On Awake");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##PlayOnAwake", &audio.PlayOnAwake);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Loop");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Loop", &audio.Loop);

        ImGui::Separator();

        // Priority (editable drag)
        EditorComponents::DrawSliderWithInput("Priority", &audio.Priority, 0, 256, true, labelWidth);
        // Volume (editable drag)
        EditorComponents::DrawSliderWithInput("Volume", &audio.Volume, 0.0f, 1.0f, false, labelWidth);
        // Pitch (editable drag)
        EditorComponents::DrawSliderWithInput("Pitch", &audio.Pitch, 0.1f, 3.0f, false, labelWidth);
        // Stereo Pan (editable drag)
        EditorComponents::DrawSliderWithInput("Stereo Pan", &audio.StereoPan, -1.0f, 1.0f, false, labelWidth);
        // Reverb Zone Mix (editable drag)
        EditorComponents::DrawSliderWithInput("Reverb Zone Mix", &audio.reverbZoneMix, 0.0f, 1.0f, false, labelWidth);

        // 3D Sound Settings (collapsible)
        if (ImGui::CollapsingHeader("3D Sound Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            ImGui::Text("Spatialize");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::Checkbox("##Spatialize", &audio.Spatialize);

            if (audio.Spatialize)
            {
                // Spatial Blend (editable dra with undo support)
                if (EditorComponents::DrawSliderWithInput("Spatial Blend", &audio.SpatialBlend, 0.0f, 1.0f, false, labelWidth))
                {
                    audio.SetSpatialBlend(audio.SpatialBlend);
                }

                // Doppler Level (editable drag)
                EditorComponents::DrawSliderWithInput("Doppler Level", &audio.DopplerLevel, 0.0f, 5.0f, false, labelWidth);

                //// Spread (placeholder, editable drag)
                // static float spread = 0.0f;
                // ImGui::Text("Spread");
                // ImGui::SameLine(labelWidth);
                // ImGui::SetNextItemWidth(-1);
                // ImGui::DragInt("##Spread", reinterpret_cast<int*>(&spread), 1.0f, 0, 360);

                //// Volume Rolloff dropdown
                // ImGui::Text("Volume Rolloff");
                // ImGui::SameLine(labelWidth);
                // ImGui::SetNextItemWidth(-1);
                // const char* rolloffModes[] = { "Logarithmic Rolloff", "Linear Rolloff", "Custom Rolloff" };
                // static int currentRolloff = 0;
                // EditorComponents::PushComboColors();
                // ImGui::Combo("##VolumeRolloff", &currentRolloff, rolloffModes, 3);
                // EditorComponents::PopComboColors();

                // Min Distance (editable drag)
                ImGui::Text("Min Distance");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);
                UndoableWidgets::DragFloat("##MinDistance", &audio.MinDistance, 0.1f, 0.0f, audio.MaxDistance, "%.2f");

                // Max Distance (editable drag)
                ImGui::Text("Max Distance");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);
                UndoableWidgets::DragFloat("##MaxDistance", &audio.MaxDistance, 0.1f, audio.MinDistance, 10000.0f, "%.2f");
            }
            ImGui::Unindent();
        }
        return true; // Skip default rendering
    });

    ReflectionRenderer::RegisterFieldRenderer("AudioListenerComponent", "isMainListener",
    [](const char *, void *, Entity, ECSManager &ecs)
    {
        ecs;
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("If enabled, this Audio Listener will be the primary listener for 3D audio rendering.");
        }
        return false;
    });

    // ==================== AUDIO REVERB ZONE COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("AudioReverbZoneComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        AudioReverbZoneComponent &reverbZone = *static_cast<AudioReverbZoneComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Enabled checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        ImGui::Checkbox("##Enabled", &reverbZone.enabled);

        ImGui::Separator();

        // Zone Distance Settings
        ImGui::Text("Zone Distance");
        ImGui::Spacing();

        // Min Distance (editable drag)
        ImGui::Text("Min Distance");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##MinDistance", &reverbZone.MinDistance, 0.1f, 0.0f, reverbZone.MaxDistance, "%.2f"))
        {
            reverbZone.MinDistance = std::max(0.0f, reverbZone.MinDistance);
        }

        // Max Distance (editable drag)
        ImGui::Text("Max Distance");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##MaxDistance", &reverbZone.MaxDistance, 0.1f, reverbZone.MinDistance, 10000.0f, "%.2f"))
        {
            reverbZone.MaxDistance = std::max(reverbZone.MinDistance, reverbZone.MaxDistance);
        }

        ImGui::Separator();

        // Reverb Preset Dropdown
        ImGui::Text("Reverb Preset");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char *presetNames[] = {
            "Off", "Generic", "Padded Cell", "Room", "Bathroom", "Living Room",
            "Stone Room", "Auditorium", "Concert Hall", "Cave", "Arena", "Hangar",
            "Carpetted Hallway", "Hallway", "Stone Corridor", "Alley", "Forest",
            "City", "Mountains", "Quarry", "Plain", "Parking Lot", "Sewer Pipe",
            "Underwater", "Drugged", "Dizzy", "Psychotic", "Custom"};

        int currentPresetIndex = reverbZone.reverbPresetIndex;
        EditorComponents::PushComboColors();
        if (ImGui::Combo("##ReverbPreset", &currentPresetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
        {
            reverbZone.SetReverbPresetByIndex(currentPresetIndex);
        }
        EditorComponents::PopComboColors();

        ImGui::Separator();

        // Advanced Reverb Parameters (collapsible)
        if (ImGui::CollapsingHeader("Advanced Reverb Parameters", ImGuiTreeNodeFlags_None))
        {
            ImGui::Indent();

            ImGui::Text("Decay Time (s)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##DecayTime", &reverbZone.decayTime, 0.01f, 0.1f, 20.0f, "%.2f");

            ImGui::Text("Early Delay (s)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##EarlyDelay", &reverbZone.earlyDelay, 0.001f, 0.0f, 0.3f, "%.3f");

            ImGui::Text("Late Delay (s)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LateDelay", &reverbZone.lateDelay, 0.001f, 0.0f, 0.1f, "%.3f");

            ImGui::Text("HF Reference (Hz)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##HFReference", &reverbZone.hfReference, 10.0f, 20.0f, 20000.0f, "%.0f");

            ImGui::Text("HF Decay Ratio");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##HFDecayRatio", &reverbZone.hfDecayRatio, 0.01f, 0.1f, 2.0f, "%.2f");

            ImGui::Text("Diffusion (%)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##Diffusion", &reverbZone.diffusion, 1.0f, 0.0f, 100.0f, "%.0f");

            ImGui::Text("Density (%)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##Density", &reverbZone.density, 1.0f, 0.0f, 100.0f, "%.0f");

            ImGui::Text("Low Shelf Freq (Hz)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LowShelfFreq", &reverbZone.lowShelfFrequency, 10.0f, 20.0f, 1000.0f, "%.0f");

            ImGui::Text("Low Shelf Gain (dB)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LowShelfGain", &reverbZone.lowShelfGain, 0.1f, -36.0f, 12.0f, "%.1f");

            ImGui::Text("High Cut (Hz)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##HighCut", &reverbZone.highCut, 10.0f, 20.0f, 20000.0f, "%.0f");

            ImGui::Text("Early/Late Mix (%)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##EarlyLateMix", &reverbZone.earlyLateMix, 1.0f, 0.0f, 100.0f, "%.0f");

            ImGui::Text("Wet Level (dB)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##WetLevel", &reverbZone.wetLevel, 0.1f, -80.0f, 20.0f, "%.1f");

            ImGui::Unindent();
        }

        // Note about preset changes
        if (reverbZone.reverbPresetIndex != static_cast<int>(AudioReverbZoneComponent::ReverbPreset::Custom))
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: Changing advanced parameters will set preset to 'Custom'");
        }

        return true; // Skip default rendering
    });

    // ==================== PARTICLE COMPONENT ====================
    // Add Play/Pause/Stop buttons at the beginning of ParticleComponent rendering

    ReflectionRenderer::RegisterComponentRenderer("ParticleComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        ParticleComponent &particle = *static_cast<ParticleComponent *>(componentPtr);

        // Play/Pause/Stop buttons for editor preview
        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (EditorComponents::DrawPlayButton(particle.isPlayingInEditor && !particle.isPausedInEditor, buttonWidth))
        {
            SnapshotManager::GetInstance().TakeSnapshot("Play Particles");
            particle.isPlayingInEditor = true;
            particle.isPausedInEditor = false;
        }

        ImGui::SameLine();

        if (EditorComponents::DrawPauseButton(particle.isPausedInEditor, buttonWidth))
        {
            if (particle.isPlayingInEditor)
            {
                SnapshotManager::GetInstance().TakeSnapshot("Pause Particles");
                particle.isPausedInEditor = !particle.isPausedInEditor;
            }
        }

        if (EditorComponents::DrawStopButton())
        {
            SnapshotManager::GetInstance().TakeSnapshot("Stop Particles");
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
        UndoableWidgets::Checkbox("Is Emitting", &particle.isEmitting);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Whether the particle system is actively emitting new particles");
        }

        ImGui::Separator();

        // Continue with normal field rendering
        return false; // Return false to continue with default field rendering
    });

    // ==================== DIRECTIONAL LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("DirectionalLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
            ecs;
        DirectionalLightComponent &light = *static_cast<DirectionalLightComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Basic properties with automatic undo/redo
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Enabled", &light.enabled);

        ImGui::Text("Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Color", &light.color.x);

        ImGui::Text("Intensity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

        // Note: Direction is controlled via Transform rotation
        ImGui::Separator();
        ImGui::Text("Lighting Properties");

        ImGui::Text("Ambient");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Ambient", &light.ambient.x);

        ImGui::Text("Diffuse");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Diffuse", &light.diffuse.x);

        ImGui::Text("Specular");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Specular", &light.specular.x);

        return true; // Return true to skip default field rendering
    });

    // ==================== POINT LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("PointLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
            ecs;
        PointLightComponent &light = *static_cast<PointLightComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Enabled", &light.enabled);

        ImGui::Text("Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Color", &light.color.x);

        ImGui::Text("Intensity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Attenuation");

        ImGui::Text("Constant");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Constant", &light.constant, 0.01f, 0.0f, 2.0f);

        ImGui::Text("Linear");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Linear", &light.linear, 0.01f, 0.0f, 1.0f);

        ImGui::Text("Quadratic");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Lighting Properties");

        ImGui::Text("Ambient");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Ambient", &light.ambient.x);

        ImGui::Text("Diffuse");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Diffuse", &light.diffuse.x);

        ImGui::Text("Specular");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Specular", &light.specular.x);

        return true; // Return true to skip default field rendering
    });

    // ==================== SPOT LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("SpotLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
            ecs;
        SpotLightComponent &light = *static_cast<SpotLightComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Enabled", &light.enabled);

        ImGui::Text("Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Color", &light.color.x);

        ImGui::Text("Intensity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

        // Note: Direction is controlled via Transform rotation
        ImGui::Separator();
        ImGui::Text("Cone Settings");

        // Convert from cosine to degrees for easier editing
        float cutOffDegrees = glm::degrees(glm::acos(light.cutOff));
        float outerCutOffDegrees = glm::degrees(glm::acos(light.outerCutOff));

        ImGui::Text("Inner Cutoff (degrees)");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (UndoableWidgets::DragFloat("##InnerCutoff", &cutOffDegrees, 1.0f, 0.0f, 90.0f))
        {
            light.cutOff = glm::cos(glm::radians(cutOffDegrees));
        }

        ImGui::Text("Outer Cutoff (degrees)");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (UndoableWidgets::DragFloat("##OuterCutoff", &outerCutOffDegrees, 1.0f, 0.0f, 90.0f))
        {
            light.outerCutOff = glm::cos(glm::radians(outerCutOffDegrees));
        }

        ImGui::Separator();
        ImGui::Text("Attenuation");

        ImGui::Text("Constant");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Constant", &light.constant, 0.01f, 0.0f, 2.0f);

        ImGui::Text("Linear");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Linear", &light.linear, 0.01f, 0.0f, 1.0f);

        ImGui::Text("Quadratic");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Lighting Properties");

        ImGui::Text("Ambient");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Ambient", &light.ambient.x);

        ImGui::Text("Diffuse");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Diffuse", &light.diffuse.x);

        ImGui::Text("Specular");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::ColorEdit3("##Specular", &light.specular.x);

        return true;
    });

    // ==================== ANIMATION COMPONENT (Unity-style) ====================
    ReflectionRenderer::RegisterComponentRenderer("AnimationComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        (void)ecs;
        AnimationComponent &animComp = *static_cast<AnimationComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Preview state tracking
        enum class PreviewState { Stopped, Playing, Paused };
        static std::unordered_map<Entity, PreviewState> previewState;
        if (previewState.find(entity) == previewState.end()) {
            previewState[entity] = PreviewState::Stopped;
        }

        // Handle preview animation in edit mode
        if (EditorState::GetInstance().GetState() == EditorState::State::EDIT_MODE) {
            Animator *animator = animComp.GetAnimatorPtr();
            if (animator && !animComp.GetClips().empty()) {
                const auto& clips = animComp.GetClips();
                size_t activeClipIndex = animComp.GetActiveClipIndex();

                if (activeClipIndex < clips.size()) {
                    if (previewState[entity] == PreviewState::Playing && animComp.enabled) {
                        const Animation& clip = *clips[activeClipIndex];
                        float tps = clip.GetTicksPerSecond();
                        if (tps <= 0.0f) tps = 25.0f;

                        animComp.editorPreviewTime += tps * ImGui::GetIO().DeltaTime * animComp.speed;
                        float duration = clip.GetDuration();
                        if (animComp.isLoop) {
                            animComp.editorPreviewTime = fmod(animComp.editorPreviewTime, duration);
                        } else if (animComp.editorPreviewTime > duration) {
                            animComp.editorPreviewTime = duration;
                            previewState[entity] = PreviewState::Paused;
                        }
                    }
                    animator->SetCurrentTime(animComp.editorPreviewTime, entity);
                }
            }
        }

        // ===== CONTROLLER FIELD (Unity-style) =====
        ImGui::Text("Controller");
        ImGui::SameLine(labelWidth);

        AnimationStateMachine* sm = animComp.GetStateMachine();
        bool hasController = sm && !sm->GetAllStates().empty();

        // Determine display text - show controller file name
        std::string displayText;
        if (!animComp.controllerPath.empty()) {
            // Show file name from controller path
            std::filesystem::path p(animComp.controllerPath);
            displayText = p.stem().string();
            if (!hasController) {
                displayText += " (not loaded)";
            }
        } else if (hasController) {
            displayText = "Controller";
        } else {
            displayText = "None (Animator Controller)";
        }

        float fieldWidth = ImGui::GetContentRegionAvail().x - 25;
        EditorComponents::DrawDragDropButton(displayText.c_str(), fieldWidth);

        // Double-click to open Animator Editor
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            AnimatorEditorWindow* animatorEditor = GetAnimatorEditor();
            if (animatorEditor) {
                animatorEditor->OpenForEntity(entity, &animComp);
            }
        }

        // Drag-drop target for .animator files
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ANIMATOR_PAYLOAD")) {
                const char* droppedPath = static_cast<const char*>(payload->Data);
                std::string pathStr(droppedPath);

                AnimatorController controller;
                if (controller.LoadFromFile(pathStr)) {
                    // Save the controller path for serialization
                    animComp.controllerPath = pathStr;

                    // Apply state machine configuration
                    AnimationStateMachine* stateMachine = animComp.EnsureStateMachine();
                    controller.ApplyToStateMachine(stateMachine);

                    // Copy clip paths from controller to component
                    const auto& ctrlClipPaths = controller.GetClipPaths();
                    animComp.clipPaths = ctrlClipPaths;
                    animComp.clipCount = static_cast<int>(ctrlClipPaths.size());
                    animComp.clipGUIDs.resize(ctrlClipPaths.size(), {0, 0});

                    // Load clips from controller paths if model is available
                    if (ecs.HasComponent<ModelRenderComponent>(entity)) {
                        auto &modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                        if (modelComp.model) {
                            animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount(), entity);
                            Animator* animator = animComp.EnsureAnimator();
                            modelComp.SetAnimator(animator);

                            // Play the entry state's animation clip (not just first clip)
                            if (!animComp.GetClips().empty() && stateMachine) {
                                std::string entryState = stateMachine->GetEntryState();
                                const AnimStateConfig* entryConfig = stateMachine->GetState(entryState);
                                size_t clipToPlay = 0;
                                if (entryConfig && entryConfig->clipIndex < animComp.GetClips().size()) {
                                    clipToPlay = entryConfig->clipIndex;
                                }
                                animComp.SetClip(clipToPlay, entity);
                                animator->PlayAnimation(animComp.GetClips()[clipToPlay].get(), entity);
                            }
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Picker button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_CIRCLE_DOT "##PickController", ImVec2(22, 0))) {
            #ifdef _WIN32
            HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(hrCo)) {
                IFileOpenDialog* pFileOpen = nullptr;
                HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
                if (SUCCEEDED(hr) && pFileOpen) {
                    const COMDLG_FILTERSPEC fileTypes[] = {
                        { L"Animator Controller (*.animator)", L"*.animator" },
                        { L"All Files (*.*)", L"*.*" }
                    };
                    pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
                    pFileOpen->SetTitle(L"Select Animator Controller");

                    DWORD options = 0;
                    if (SUCCEEDED(pFileOpen->GetOptions(&options))) {
                        pFileOpen->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
                    }

                    hr = pFileOpen->Show(nullptr);
                    if (SUCCEEDED(hr)) {
                        IShellItem* pItem = nullptr;
                        if (SUCCEEDED(pFileOpen->GetResult(&pItem)) && pItem) {
                            PWSTR pszFilePath = nullptr;
                            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath) {
                                std::filesystem::path p(pszFilePath);
                                std::string controllerPath = p.string();
                                CoTaskMemFree(pszFilePath);

                                AnimatorController controller;
                                if (controller.LoadFromFile(controllerPath)) {
                                    // Save the controller path for serialization
                                    animComp.controllerPath = controllerPath;

                                    // Apply state machine configuration
                                    AnimationStateMachine* stateMachine = animComp.EnsureStateMachine();
                                    controller.ApplyToStateMachine(stateMachine);

                                    // Copy clip paths from controller to component
                                    const auto& ctrlClipPaths = controller.GetClipPaths();
                                    animComp.clipPaths = ctrlClipPaths;
                                    animComp.clipCount = static_cast<int>(ctrlClipPaths.size());
                                    animComp.clipGUIDs.resize(ctrlClipPaths.size(), {0, 0});

                                    // Load clips from controller paths if model is available
                                    if (ecs.HasComponent<ModelRenderComponent>(entity)) {
                                        auto &modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                                        if (modelComp.model) {
                                            animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount(), entity);
                                            Animator* animator = animComp.EnsureAnimator();
                                            modelComp.SetAnimator(animator);

                                            // Play the entry state's animation clip (not just first clip)
                                            if (!animComp.GetClips().empty() && stateMachine) {
                                                std::string entryState = stateMachine->GetEntryState();
                                                const AnimStateConfig* entryConfig = stateMachine->GetState(entryState);
                                                size_t clipToPlay = 0;
                                                if (entryConfig && entryConfig->clipIndex < animComp.GetClips().size()) {
                                                    clipToPlay = entryConfig->clipIndex;
                                                }
                                                animComp.SetClip(clipToPlay, entity);
                                                animator->PlayAnimation(animComp.GetClips()[clipToPlay].get(), entity);
                                            }
                                        }
                                    }
                                }
                            }
                            pItem->Release();
                        }
                    }
                    pFileOpen->Release();
                }
                CoUninitialize();
            }
            #endif
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select Animator Controller");
        }

        // ===== CURRENT STATE (read-only, from state machine) =====
        if (hasController) {
            ImGui::Spacing();
            ImGui::Text("Current State");
            ImGui::SameLine(labelWidth);
            std::string currentState = sm->GetCurrentState();
            if (currentState.empty()) currentState = sm->GetEntryState();
            ImGui::TextDisabled("%s", currentState.c_str());
        }

        // ===== ANIMATION CLIP SELECTOR =====
        if (!animComp.clipPaths.empty()) {
            ImGui::Spacing();
            ImGui::Text("Animation Clip");
            ImGui::SameLine(labelWidth);

            // Get current clip name
            size_t activeClipIndex = animComp.GetActiveClipIndex();
            std::string currentClipName = "(None)";
            if (activeClipIndex < animComp.clipPaths.size()) {
                std::filesystem::path clipPath(animComp.clipPaths[activeClipIndex]);
                currentClipName = clipPath.stem().string();
            }

            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##AnimClipSelect", currentClipName.c_str())) {
                for (size_t i = 0; i < animComp.clipPaths.size(); i++) {
                    std::filesystem::path clipPath(animComp.clipPaths[i]);
                    std::string clipName = clipPath.stem().string();
                    bool isSelected = (i == activeClipIndex);

                    if (ImGui::Selectable(clipName.c_str(), isSelected)) {
                        // Change to the selected animation
                        if (i < animComp.GetClips().size()) {
                            animComp.SetClip(i, entity);
                            animComp.editorPreviewTime = 0.0f;
                            // Reset animator to play from beginning
                            Animator* animator = animComp.GetAnimatorPtr();
                            if (animator) {
                                animator->PlayAnimation(animComp.GetClips()[i].get(), entity);
                            }
                        }
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    // Show full path as tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", animComp.clipPaths[i].c_str());
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ===== PREVIEW CONTROLS =====
        bool isEditMode = (EditorState::GetInstance().GetState() == EditorState::State::EDIT_MODE);
        ImGui::BeginDisabled(!isEditMode || animComp.GetClips().empty());

        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        bool isPlaying = (previewState[entity] == PreviewState::Playing);

        if (EditorComponents::DrawPlayButton(isPlaying, buttonWidth)) {
            previewState[entity] = PreviewState::Playing;
        }
        ImGui::SameLine();
        if (EditorComponents::DrawPauseButton(!isPlaying, buttonWidth)) {
            previewState[entity] = PreviewState::Paused;
        }
        if (EditorComponents::DrawStopButton()) {
            previewState[entity] = PreviewState::Stopped;
            animComp.ResetPreview(entity);
        }

        ImGui::EndDisabled();

        // Progress bar
        const auto &clips = animComp.GetClips();
        if (!clips.empty()) {
            size_t activeClipIndex = animComp.GetActiveClipIndex();
            if (activeClipIndex < clips.size()) {
                const Animator *animator = animComp.GetAnimatorPtr();
                if (animator) {
                    float currentTime = animator->GetCurrentTime();
                    const Animation &clip = animComp.GetClip(activeClipIndex);
                    float duration = clip.GetDuration();
                    float progress = duration > 0.0f ? (currentTime / duration) : 0.0f;

                    ImGui::Spacing();
                    ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
                }
            }
        }

        return true; // Skip default field rendering
    });

    ReflectionRenderer::RegisterComponentRenderer("BrainComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
            ecs;
        BrainComponent &brain = *static_cast<BrainComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Combo for Kind
        ImGui::Text("Kind");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        static const char *kKinds[] = {"None", "Grunt", "Boss"};
        int kindIdx = static_cast<int>(brain.kind);
        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##Kind", &kindIdx, kKinds, IM_ARRAYSIZE(kKinds)))
        {
            brain.kind = static_cast<BrainKind>(kindIdx);
            brain.kindInt = kindIdx;
            // Mark as needing rebuild (optional UX)
        }
        EditorComponents::PopComboColors();

        // Read-only current state
        ImGui::Text("Active State: %s", brain.activeState.empty() ? "None" : brain.activeState.c_str());

        // Build / Rebuild
        if (ImGui::Button(brain.impl && brain.started ? "Rebuild" : "Build"))
        {
            if (brain.impl && brain.started)
                brain.impl->onExit(ecs, entity);

            brain.enabled = true; // ensure init system will start it
            brain.impl.reset();
            brain.started = false;

            brain.impl = game_ai::CreateFor(ecs, entity, brain.kind); // optional pre-create
        }

        // Stop
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            if (brain.impl && brain.started)
                brain.impl->onExit(ecs, entity);

            brain.enabled = false; // <-- prevents re-entry
            brain.impl.reset();
            brain.started = false;
            brain.activeState.clear(); // shows "None"
        }

        return true;
    });

    // ==================== SCRIPT COMPONENT ====================
    // Old field renderers - no longer used (fields moved to scripts vector)

    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "scriptPath",
    [](const char *, void *, Entity, ECSManager &)
    {
        return true; // Hidden - handled by component renderer
    });

    // Hide internal/runtime fields from inspector
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "instanceId",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "instanceCreated",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "pendingInstanceState",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    // ==================== SCRIPT COMPONENT - AUTOMATIC PROPERTY EXPOSURE ====================

    ReflectionRenderer::RegisterComponentRenderer("ScriptComponentData",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        ScriptComponentData &scriptComp = *static_cast<ScriptComponentData *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Get lua state
        lua_State* L = Scripting::GetLuaState();
        if (!L)
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Scripting runtime not initialized");
            return true;
        }

        // Use static maps to store preview instances per entity+script index
        static std::unordered_map<std::string, int> editorPreviewInstances; // key: "entity_scriptIndex"
        static std::unordered_map<std::string, std::string> editorPreviewScriptPaths;

        // Track state transitions to detect when we need to invalidate cached instances
        static EditorState::State lastEditorState = EditorState::GetInstance().GetState();
        EditorState::State currentEditorState = EditorState::GetInstance().GetState();
        // Commented out to fix warning C4189 - unused variable
        // bool isInPlayMode = (currentEditorState == EditorState::State::PLAY_MODE ||
        //                     currentEditorState == EditorState::State::PAUSED);

        // Clear all cached preview instances when transitioning between modes
        // This is necessary because scene deserialization creates new instances with new registry refs
        if (lastEditorState != currentEditorState)
        {
            // Save the current state of all preview instances to preserve edited values
            // This happens for ALL transitions to ensure values persist across multiple play/stop cycles
            for (auto& [key, instanceRef] : editorPreviewInstances)
            {
                if (Scripting::IsValidInstance(instanceRef))
                {
                    // Parse the key to get entity and script index
                    size_t underscorePos = key.find('_');
                    if (underscorePos != std::string::npos)
                    {
                        // Renamed to fix warning C4457 - entity hides function parameter
                        Entity parsedEntity = static_cast<Entity>(std::stoi(key.substr(0, underscorePos)));
                        size_t scriptIdx = std::stoi(key.substr(underscorePos + 1));

                        // Get the script component and save the state
                        if (ecs.HasComponent<ScriptComponentData>(parsedEntity))
                        {
                            auto& scriptCompToSave = ecs.GetComponent<ScriptComponentData>(parsedEntity);
                            if (scriptIdx < scriptCompToSave.scripts.size())
                            {
                                // Always preserve the current state - either from preview or runtime instance
                                std::string currentState = Scripting::SerializeInstanceToJson(instanceRef);
                                if (!currentState.empty())
                                {
                                    scriptCompToSave.scripts[scriptIdx].pendingInstanceState = currentState;
                                    ENGINE_PRINT("Preserved instance state for entity ", parsedEntity, " script ", scriptIdx,
                                               " (transition: ", static_cast<int>(lastEditorState), " -> ",
                                               static_cast<int>(currentEditorState), ")");
                                }
                            }
                        }
                    }
                }
            }

            // Destroy all cached preview instances as their references are now invalid
            for (auto& [key, instanceRef] : editorPreviewInstances)
            {
                if (Scripting::IsValidInstance(instanceRef))
                {
                    Scripting::DestroyInstance(instanceRef);
                }
            }
            editorPreviewInstances.clear();
            editorPreviewScriptPaths.clear();
        }
        lastEditorState = currentEditorState;

        // Render each script in the vector
        int scriptIndexToRemove = -1;
        for (size_t scriptIdx = 0; scriptIdx < scriptComp.scripts.size(); ++scriptIdx)
        {
            ScriptData& scriptData = scriptComp.scripts[scriptIdx];
            std::string uniqueKey = std::to_string(entity) + "_" + std::to_string(scriptIdx);

            ImGui::PushID(static_cast<int>(scriptIdx));

            // Render script header with remove button
            ImGui::Separator();
            ImGui::Text("Script %zu", scriptIdx + 1);
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_TRASH "##RemoveScript"))
            {
                scriptIndexToRemove = static_cast<int>(scriptIdx);
                ImGui::PopID();
                continue;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove this script");
            }

            // Script path display
            std::string displayText = scriptData.scriptPath.empty() ? "None (Lua Script)" :
                                      scriptData.scriptPath.substr(scriptData.scriptPath.find_last_of("/\\") + 1);

            ImGui::SetNextItemWidth(-1);
            float dragDropWidth = ImGui::GetContentRegionAvail().x - 40.0f; // Leave space for reload button
            EditorComponents::DrawDragDropButton(displayText.c_str(), dragDropWidth);

            // Double-click to open
            if (!scriptData.scriptPath.empty() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                // Cache project root to avoid repeated filesystem operations
                static std::filesystem::path cachedProjectRoot;
                static bool projectRootCached = false;

                if (!projectRootCached) {
                    std::filesystem::path currentPath = std::filesystem::current_path();

                    // Find the project root by looking for the expected project structure
                    // This works regardless of which build subfolder we're in
                    cachedProjectRoot = currentPath;
                    while (cachedProjectRoot.has_parent_path()) {
                        // Check if this directory has the expected project structure
                        if (std::filesystem::exists(cachedProjectRoot / "Build") &&
                            std::filesystem::exists(cachedProjectRoot / "Resources") &&
                            std::filesystem::exists(cachedProjectRoot / "Engine")) {
                            break;
                        }
                        cachedProjectRoot = cachedProjectRoot.parent_path();
                    }
                    projectRootCached = true;
                }

                // Construct the correct path to the script file
                std::filesystem::path scriptFullPath;
                if (scriptData.scriptPath.find("Resources/") == 0) {
                    // Path includes Resources/ prefix
                    scriptFullPath = cachedProjectRoot / scriptData.scriptPath;
                } else if (scriptData.scriptPath.find("scripts/") == 0 || scriptData.scriptPath.find("Scripts/") == 0) {
                    // Path includes scripts/ prefix
                    scriptFullPath = cachedProjectRoot / "Resources" / scriptData.scriptPath;
                } else {
                    // Just the script filename
                    scriptFullPath = cachedProjectRoot / "Resources" / "scripts" / scriptData.scriptPath;
                }

                // Ensure the parent directory exists, create if necessary
                std::filesystem::path parentDir = scriptFullPath.parent_path();
                if (!std::filesystem::exists(parentDir)) {
                    std::filesystem::create_directories(parentDir);
                }

                // Check if file exists, but still proceed with opening (VS Code can create new files)
                if (!std::filesystem::exists(scriptFullPath)) {
                    ENGINE_PRINT("Warning: Script file does not exist, VS Code will create it: ", scriptFullPath.string().c_str());
                }

                #ifdef _WIN32
                    std::string command = "code \"" + scriptFullPath.string() + "\"";
                #elif __linux__
                    std::string command = "code \"" + scriptFullPath.string() + "\" &";
                #elif __APPLE__
                    std::string command = "code \"" + scriptFullPath.string() + "\"";
                #endif
                system(command.c_str());
            }

            if (ImGui::IsItemHovered() && !scriptData.scriptPath.empty())
            {
                ImGui::SetTooltip("Double-click to open in VS Code");
            }

            // Drag-drop support
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SCRIPT_PAYLOAD"))
                {
                    SnapshotManager::GetInstance().TakeSnapshot("Assign Script");
                    const char *droppedPath = (const char *)payload->Data;
                    std::string pathStr(droppedPath, payload->DataSize);
                    pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                    scriptData.scriptGuid = DraggedScriptGuid;
                    scriptData.scriptGuidStr = GUIDUtilities::ConvertGUID128ToString(scriptData.scriptGuid);
                    scriptData.scriptPath = pathStr;
                    scriptData.instanceCreated = false;
                    scriptData.instanceId = -1;

                    // Clear preview instance for this script
                    editorPreviewInstances.erase(uniqueKey);
                    editorPreviewScriptPaths.erase(uniqueKey);
                }
                ImGui::EndDragDropTarget();
            }

            // Add reload button beside the drag-drop field
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_ROTATE_RIGHT "##ReloadScripts")) {
                Scripting::RequestReloadNow();
                if (Scripting::GetLuaState()) Scripting::Tick(0.0f);
                ENGINE_PRINT("Requested script reload from inspector for script: ", scriptData.scriptPath.c_str());
            }

            // If no script assigned, skip field rendering
            if (scriptData.scriptPath.empty())
            {
                ImGui::PopID();
                continue;
            }

            // Handle instance selection based on editor state
            int instanceToInspect = -1;
            bool usingPreviewInstance = false;

            // Try to use runtime instance first if available and valid
            if (scriptData.instanceCreated && scriptData.instanceId != -1 &&
                Scripting::IsValidInstance(scriptData.instanceId))
            {
                // Validate the runtime instance is still a proper Lua table
                lua_State* validateL = Scripting::GetLuaState();
                bool isRuntimeValid = false;
                if (validateL)
                {
                    lua_rawgeti(validateL, LUA_REGISTRYINDEX, scriptData.instanceId);
                    isRuntimeValid = lua_istable(validateL, -1);
                    lua_pop(validateL, 1);
                }

                if (isRuntimeValid)
                {
                    // Use the valid runtime instance
                    instanceToInspect = scriptData.instanceId;
                    usingPreviewInstance = false;

                    // Sync pendingInstanceState with runtime state to preserve any runtime changes
                    std::string runtimeState = Scripting::SerializeInstanceToJson(scriptData.instanceId);
                    if (!runtimeState.empty())
                    {
                        scriptData.pendingInstanceState = runtimeState;
                    }
                }
                else
                {
                    // Runtime instance is invalid, fall through to create preview
                    scriptData.instanceCreated = false;
                    scriptData.instanceId = -1;
                }
            }

            // If no valid runtime instance, create or use preview instance
            if (instanceToInspect == -1)
            {
                // Check if the script path changed
                auto pathIt = editorPreviewScriptPaths.find(uniqueKey);
                if (pathIt != editorPreviewScriptPaths.end() && pathIt->second != scriptData.scriptPath)
                {
                    editorPreviewInstances.erase(uniqueKey);
                    editorPreviewScriptPaths.erase(uniqueKey);
                }

                // Check if we already have a preview instance and validate it
                auto it = editorPreviewInstances.find(uniqueKey);
                if (it != editorPreviewInstances.end())
                {
                    // Validate the instance is still a proper Lua table
                    lua_State* validateL = Scripting::GetLuaState();
                    bool isValid = false;
                    if (validateL && Scripting::IsValidInstance(it->second))
                    {
                        lua_rawgeti(validateL, LUA_REGISTRYINDEX, it->second);
                        isValid = lua_istable(validateL, -1);
                        lua_pop(validateL, 1);
                    }

                    if (isValid)
                    {
                        instanceToInspect = it->second;
                        usingPreviewInstance = true;
                    }
                    else
                    {
                        // Instance is invalid, remove it and create a new one
                        if (Scripting::IsValidInstance(it->second))
                        {
                            Scripting::DestroyInstance(it->second);
                        }
                        editorPreviewInstances.erase(uniqueKey);
                        editorPreviewScriptPaths.erase(uniqueKey);
                    }
                }

                if (instanceToInspect == -1)
                {
                    // Create new preview instance
                    int previewInstance = Scripting::CreateInstanceFromFile(scriptData.scriptPath);
                    if (Scripting::IsValidInstance(previewInstance))
                    {
                        editorPreviewInstances[uniqueKey] = previewInstance;
                        editorPreviewScriptPaths[uniqueKey] = scriptData.scriptPath;
                        instanceToInspect = previewInstance;
                        usingPreviewInstance = true;

                        // ALWAYS restore pending state to preserve edited values
                        // This is critical for behavior where inspector edits persist
                        if (!scriptData.pendingInstanceState.empty())
                        {
                            bool restored = Scripting::DeserializeJsonToInstance(previewInstance, scriptData.pendingInstanceState);
                            if (restored)
                            {
                                ENGINE_PRINT("Restored pendingInstanceState for ", scriptData.scriptPath.c_str());
                            }
                            else
                            {
                                ENGINE_PRINT("Failed to restore pendingInstanceState for ", scriptData.scriptPath.c_str());
                            }
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load script for preview");
                        ImGui::Text("Path: %s", scriptData.scriptPath.c_str());
                        ImGui::PopID();
                        continue;
                    }
                }
            }

            if (!Scripting::IsValidInstance(instanceToInspect))
            {
                // If using a preview instance that's no longer valid, clean it up
                if (usingPreviewInstance)
                {
                    editorPreviewInstances.erase(uniqueKey);
                    editorPreviewScriptPaths.erase(uniqueKey);
                }
                ImGui::PopID();
                continue;
            }

            // Use ScriptInspector to get fields
            static Scripting::ScriptInspector inspector;
            std::vector<Scripting::FieldInfo> fields;

        try {
            fields = inspector.InspectInstance(L, instanceToInspect, scriptData.scriptPath, 1.0);
        } catch (const std::exception& e) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to inspect script: %s", e.what());
            // Clean up the invalid preview instance
            if (usingPreviewInstance)
            {
                editorPreviewInstances.erase(uniqueKey);
                editorPreviewScriptPaths.erase(uniqueKey);
            }
            ImGui::PopID();
            continue;
        }

        // If InspectInstance returned empty fields and we're using a preview instance,
        // it might be invalid, so clean it up
        if (fields.empty() && usingPreviewInstance)
        {
            editorPreviewInstances.erase(uniqueKey);
            editorPreviewScriptPaths.erase(uniqueKey);
        }

        // Struct to hold field info including comments and default values
        struct FieldParseInfo {
            std::string name;
            std::string comment;       // Comment associated with this field (either inline or preceding line)
            std::string defaultValue;  // Default value from Lua file (e.g., "5", "true", "\"Player\"")
            bool isHeader = false;     // True if this is a section header (standalone comment like "-- === Section ===")
            std::string headerText;    // Text to display for section headers
        };

        // Helper lambda: Parse Lua script file to extract field declaration order and comments
        auto extractFieldOrderWithComments = [](const std::string& scriptPath) -> std::vector<FieldParseInfo> {
            std::vector<FieldParseInfo> fieldOrder;

            // Try multiple path resolutions to find the Lua script file
            // This handles cases where editor runs from build folder but scripts are in source
            std::vector<std::string> pathsToTry;

            // 1. Try the path as-is
            pathsToTry.push_back(scriptPath);

            // 2. Try with Resources/ prefix if not already present
            if (scriptPath.find("Resources/") != 0 && scriptPath.find("resources/") != 0) {
                pathsToTry.push_back("Resources/" + scriptPath);
            }

            // 3. Try relative to project source directory (for when running from Build folder)
            // Go up from Build/EditorDebug to Project level, then into Resources
            if (scriptPath.find("Resources/") == 0 || scriptPath.find("resources/") == 0) {
                pathsToTry.push_back("../../" + scriptPath);
                pathsToTry.push_back("../../../Project/" + scriptPath);
            } else {
                pathsToTry.push_back("../../Resources/" + scriptPath);
                pathsToTry.push_back("../../../Project/Resources/" + scriptPath);
            }

            // 4. Try absolute paths based on common project structures
            std::string normalizedPath = scriptPath;
            // Convert backslashes to forward slashes for consistency
            std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

            std::ifstream file;
            std::string openedPath;
            for (const auto& path : pathsToTry) {
                file.open(path);
                if (file.is_open()) {
                    openedPath = path;
                    break;
                }
            }

            if (!file.is_open()) {
                ENGINE_PRINT("WARNING: Could not find Lua script file for field parsing: ", scriptPath.c_str());
                ENGINE_PRINT("  Tried paths: ");
                for (const auto& path : pathsToTry) {
                    ENGINE_PRINT("    - ", path.c_str());
                }
                return fieldOrder;
            }

            std::string line;
            bool inFieldsTable = false;
            int braceDepth = 0;
            std::string pendingComment;  // Comment from previous line to associate with next field

            // Helper to count braces outside of strings and comments
            auto countBracesOutsideStrings = [](const std::string& text, int& depth) {
                bool inString = false;
                char stringDelim = 0;
                bool escaped = false;
                size_t commentStart = text.find("--");

                for (size_t i = 0; i < text.size(); ++i) {
                    // Stop at comment start
                    if (commentStart != std::string::npos && i >= commentStart) break;

                    char c = text[i];

                    if (escaped) {
                        escaped = false;
                        continue;
                    }

                    if (c == '\\') {
                        escaped = true;
                        continue;
                    }

                    if (inString) {
                        if (c == stringDelim) {
                            inString = false;
                        }
                    } else {
                        if (c == '"' || c == '\'') {
                            inString = true;
                            stringDelim = c;
                        } else if (c == '{') {
                            depth++;
                        } else if (c == '}') {
                            depth--;
                        }
                    }
                }
            };

            while (std::getline(file, line)) {
                // Remove leading whitespace only (preserve trailing for comment extraction)
                size_t start = line.find_first_not_of(" \t");
                if (start == std::string::npos) {
                    pendingComment.clear();  // Empty line clears pending comment
                    continue;
                }
                std::string trimmedLine = line.substr(start);

                // Look for "fields = {"
                if (!inFieldsTable && trimmedLine.find("fields") == 0 && trimmedLine.find("=") != std::string::npos) {
                    inFieldsTable = true;
                    countBracesOutsideStrings(trimmedLine, braceDepth);
                    pendingComment.clear();
                    continue;
                }

                if (inFieldsTable) {
                    // Check if this is a comment-only line
                    size_t commentPos = trimmedLine.find("--");
                    size_t eqPos = trimmedLine.find("=");

                    // If line starts with comment (or is comment-only), check if it's a section header
                    if (commentPos == 0) {
                        // Extract comment text (remove -- and trim)
                        std::string commentText = trimmedLine.substr(2);
                        size_t textStart = commentText.find_first_not_of(" \t");
                        if (textStart != std::string::npos) {
                            std::string trimmedComment = commentText.substr(textStart);

                            // Check if this is a section header (contains === or --- or [Header])
                            bool isHeader = false;
                            std::string headerText;

                            if (trimmedComment.find("===") != std::string::npos) {
                                // Header like "=== Section Name ===" - extract the text between ===
                                isHeader = true;
                                size_t start = trimmedComment.find_first_not_of("= \t");
                                size_t end = trimmedComment.find_last_not_of("= \t");
                                if (start != std::string::npos && end != std::string::npos && end >= start) {
                                    headerText = trimmedComment.substr(start, end - start + 1);
                                } else {
                                    headerText = trimmedComment;
                                }
                            } else if (trimmedComment.find("---") != std::string::npos) {
                                // Header like "--- Section Name ---"
                                isHeader = true;
                                size_t start = trimmedComment.find_first_not_of("- \t");
                                size_t end = trimmedComment.find_last_not_of("- \t");
                                if (start != std::string::npos && end != std::string::npos && end >= start) {
                                    headerText = trimmedComment.substr(start, end - start + 1);
                                } else {
                                    headerText = trimmedComment;
                                }
                            } else if (trimmedComment.front() == '[' && trimmedComment.back() == ']') {
                                // Header like "[Section Name]"
                                isHeader = true;
                                headerText = trimmedComment.substr(1, trimmedComment.size() - 2);
                            }

                            if (isHeader && !headerText.empty()) {
                                // Add as a header entry
                                FieldParseInfo headerInfo;
                                headerInfo.isHeader = true;
                                headerInfo.headerText = headerText;
                                fieldOrder.push_back(headerInfo);
                                pendingComment.clear();
                            } else {
                                // Regular comment - save for next field
                                pendingComment = trimmedComment;
                            }
                        }
                        // Still need to count braces in case comment contains them (shouldn't affect depth)
                        countBracesOutsideStrings(trimmedLine, braceDepth);
                        if (braceDepth == 0) break;
                        continue;
                    }

                    // Count braces outside strings and comments
                    countBracesOutsideStrings(trimmedLine, braceDepth);

                    // Only process lines with '=' that have the = before any comment
                    if (eqPos != std::string::npos && (commentPos == std::string::npos || eqPos < commentPos)) {
                        std::string fieldName = trimmedLine.substr(0, eqPos);

                        // Trim whitespace and commas from field name
                        size_t nameStart = fieldName.find_first_not_of(" \t\r\n");
                        size_t nameEnd = fieldName.find_last_not_of(" \t\r\n,");

                        if (nameStart != std::string::npos && nameEnd != std::string::npos && nameEnd >= nameStart) {
                            fieldName = fieldName.substr(nameStart, nameEnd - nameStart + 1);

                            // Check if valid identifier (starts with letter or underscore)
                            if (!fieldName.empty() && (std::isalpha(static_cast<unsigned char>(fieldName[0])) || fieldName[0] == '_')) {
                                FieldParseInfo info;
                                info.name = fieldName;

                                // Extract the default value (part after = and before comment or end of line)
                                size_t valueStart = eqPos + 1;
                                size_t valueEnd = (commentPos != std::string::npos) ? commentPos : trimmedLine.size();
                                if (valueStart < valueEnd) {
                                    std::string valueStr = trimmedLine.substr(valueStart, valueEnd - valueStart);
                                    // Trim whitespace and trailing comma
                                    size_t vStart = valueStr.find_first_not_of(" \t");
                                    size_t vEnd = valueStr.find_last_not_of(" \t\r\n,");
                                    if (vStart != std::string::npos && vEnd != std::string::npos && vEnd >= vStart) {
                                        info.defaultValue = valueStr.substr(vStart, vEnd - vStart + 1);
                                    }
                                }

                                // Extract inline comment if present
                                if (commentPos != std::string::npos && commentPos > eqPos) {
                                    std::string inlineComment = trimmedLine.substr(commentPos + 2);
                                    size_t textStart = inlineComment.find_first_not_of(" \t");
                                    if (textStart != std::string::npos) {
                                        info.comment = inlineComment.substr(textStart);
                                    }
                                }

                                // If no inline comment, use pending comment from previous line
                                if (info.comment.empty() && !pendingComment.empty()) {
                                    info.comment = pendingComment;
                                }

                                fieldOrder.push_back(info);
                                pendingComment.clear();
                            }
                        }
                    }

                    // Exit fields table when braces close
                    if (braceDepth == 0) break;
                }
            }
            return fieldOrder;
        };

        // Filter fields to show only editable fields from the 'fields' table
        // This implements behavior where only serialized fields are shown
        std::vector<Scripting::FieldInfo> filteredFields;
        bool hasFieldsTable = false;
        std::vector<FieldParseInfo> parsedFields;
        std::unordered_map<std::string, std::string> fieldComments;  // Store comments for each field

        // Build a map for quick lookup of FieldInfo by name
        std::unordered_map<std::string, Scripting::FieldInfo> fieldMap;
        for (const auto& field : fields)
        {
            fieldMap[field.name] = field;
        }

        // Parse the Lua script file to get field declaration order and comments
        parsedFields = extractFieldOrderWithComments(scriptData.scriptPath);

        // Build comment map for later tooltip display
        for (const auto& pf : parsedFields) {
            if (!pf.comment.empty()) {
                fieldComments[pf.name] = pf.comment;
            }
        }

        // TEMPORARY: Always print debug info to help diagnose field visibility issue
        // TODO: Remove this after fixing the issue
        static int debugCounter = 0;
        if (debugCounter < 5) {  // Only print first 5 times to avoid spam
            debugCounter++;

            // Build debug message
            std::string debugMsg = "\n[SCRIPT DEBUG] Script: " + scriptData.scriptPath + "\n";
            debugMsg += "  Parsed from file: " + std::to_string(parsedFields.size()) + " fields\n";
            debugMsg += "  Instance has: " + std::to_string(fields.size()) + " fields\n";

            if (!parsedFields.empty()) {
                debugMsg += "  Parsed field names: ";
                for (size_t i = 0; i < parsedFields.size() && i < 15; i++) {
                    debugMsg += parsedFields[i].name + ", ";
                }
                if (parsedFields.size() > 15) debugMsg += "...(+" + std::to_string(parsedFields.size() - 15) + " more)";
                debugMsg += "\n";
            }

            debugMsg += "  Instance field names: ";
            int count = 0;
            for (const auto& pair : fieldMap) {
                if (count++ < 15) debugMsg += pair.first + ", ";
            }
            if (fieldMap.size() > 15) debugMsg += "...(+" + std::to_string(fieldMap.size() - 15) + " more)";
            debugMsg += "\n";

            // Output to log file and console
            ENGINE_LOG_INFO(debugMsg);

            // Also try standard output for console visibility
            printf("%s", debugMsg.c_str());
            fflush(stdout);
        }

        // Debug output (only once per entity per mode to avoid spam)
        // Use a composite key: entity + isPlayMode + scriptPath to track what we've debugged
        static std::unordered_set<std::string> debuggedKeys;
        bool isPlayMode = Engine::IsPlayMode();
        std::string debugKey = std::to_string(entity) + "_" + (isPlayMode ? "play" : "edit") + "_" + scriptData.scriptPath;
        bool isFirstTimeForKey = (debuggedKeys.find(debugKey) == debuggedKeys.end());
        if (isFirstTimeForKey) {
            debuggedKeys.insert(debugKey);

            ENGINE_PRINT("=== Script Inspector Debug [", isPlayMode ? "PLAY" : "EDIT", " MODE] ===");
            ENGINE_PRINT("  Entity: ", entity, " Script: ", scriptData.scriptPath.c_str());
            ENGINE_PRINT("  Instance type: ", usingPreviewInstance ? "PREVIEW" : "RUNTIME");

            if (!parsedFields.empty()) {
                ENGINE_PRINT("  File parsing: SUCCESS (", parsedFields.size(), " fields)");
                std::string debugMsg = "  Parsed fields: ";
                for (const auto& pf : parsedFields) {
                    debugMsg += pf.name + ", ";
                }
                ENGINE_PRINT(debugMsg.c_str());
            } else {
                ENGINE_PRINT("  File parsing: FAILED - could not parse fields from file");
            }

            ENGINE_PRINT("  Instance inspection: ", fields.size(), " fields");
            std::string debugMsg = "  Instance fields: ";
            for (const auto& pair : fieldMap) {
                debugMsg += pair.first + ", ";
            }
            ENGINE_PRINT(debugMsg.c_str());
            ENGINE_PRINT("===========================================");
        }

        // Check if we successfully parsed field order from the script file
        // (Don't check the instance for a fields table, because Component mixin flattens them)
        hasFieldsTable = !parsedFields.empty();

        // WORKAROUND: In edit mode, preview instances may be incomplete because Lua modules
        // don't load properly. If we parsed fields from the file but the instance has
        // fewer fields, try to get field values from pendingInstanceState JSON.
        // We consider the instance incomplete if it has fewer fields than we parsed from file
        bool previewInstanceIncomplete = usingPreviewInstance &&
                                          hasFieldsTable &&
                                          (fieldMap.size() < parsedFields.size());

        // Debug output for workaround detection (always log to help diagnose)
        if (isFirstTimeForKey) {
            ENGINE_PRINT("WORKAROUND CHECK: previewInstanceIncomplete = ", previewInstanceIncomplete ? "TRUE" : "FALSE");
            ENGINE_PRINT("  usingPreviewInstance = ", usingPreviewInstance ? "TRUE" : "FALSE");
            ENGINE_PRINT("  hasFieldsTable = ", hasFieldsTable ? "TRUE" : "FALSE");
            ENGINE_PRINT("  fieldMap.size() = ", fieldMap.size(), ", parsedFields.size() = ", parsedFields.size());
            ENGINE_PRINT("  pendingInstanceState.size() = ", scriptData.pendingInstanceState.size());
        }

        // If preview instance is incomplete, try to parse pendingInstanceState to get field values
        std::unordered_map<std::string, std::string> savedFieldValues;
        if (previewInstanceIncomplete && !scriptData.pendingInstanceState.empty())
        {
            try {
                rapidjson::Document stateDoc;
                stateDoc.Parse(scriptData.pendingInstanceState.c_str());
                if (!stateDoc.HasParseError() && stateDoc.IsObject())
                {
                    for (auto it = stateDoc.MemberBegin(); it != stateDoc.MemberEnd(); ++it)
                    {
                        std::string fieldName = it->name.GetString();
                        // Convert value to string representation
                        rapidjson::StringBuffer buffer;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                        it->value.Accept(writer);
                        savedFieldValues[fieldName] = buffer.GetString();
                    }
                }
            } catch (...) {
                // Ignore JSON parsing errors
            }
        }

        // Build filtered fields in declaration order
        if (hasFieldsTable)
        {
            // Use the parsed field order from the script file
            for (const auto& parsedField : parsedFields)
            {
                // Handle section headers
                if (parsedField.isHeader)
                {
                    Scripting::FieldInfo headerField;
                    headerField.name = "__HEADER__";
                    headerField.type = Scripting::FieldType::Nil;  // Special marker for headers
                    headerField.meta.displayName = parsedField.headerText;
                    filteredFields.push_back(headerField);
                    continue;
                }

                auto it = fieldMap.find(parsedField.name);
                if (it != fieldMap.end())
                {
                    const auto& field = it->second;

                    // Skip functions (Start, Update, etc.)
                    if (field.type == Scripting::FieldType::Function) {
                        continue;
                    }

                    // Skip private fields
                    if (!field.name.empty() && field.name[0] == '_')
                        continue;

                    // Add field in declaration order from file
                    filteredFields.push_back(field);
                }
                else if (previewInstanceIncomplete)
                {
                    // WORKAROUND: Preview instance is incomplete, create a synthetic field entry
                    // using the saved value from pendingInstanceState if available
                    // Skip private fields
                    if (!parsedField.name.empty() && parsedField.name[0] == '_')
                        continue;

                    Scripting::FieldInfo syntheticField;
                    syntheticField.name = parsedField.name;
                    // Mark as synthetic by using a special meta tooltip prefix
                    syntheticField.meta.tooltip = "__SYNTHETIC__";

                    // Try to get saved value and determine type
                    auto savedIt = savedFieldValues.find(parsedField.name);
                    if (savedIt != savedFieldValues.end())
                    {
                        const std::string& val = savedIt->second;
                        syntheticField.defaultValueSerialized = val;

                        // Guess type from JSON value
                        if (val == "true" || val == "false") {
                            syntheticField.type = Scripting::FieldType::Boolean;
                        } else if (!val.empty() && (val[0] == '"')) {
                            syntheticField.type = Scripting::FieldType::String;
                        } else if (!val.empty() && (val[0] == '{' || val[0] == '[')) {
                            syntheticField.type = Scripting::FieldType::Table;
                        } else if (!val.empty() && (std::isdigit(val[0]) || val[0] == '-' || val[0] == '.')) {
                            syntheticField.type = Scripting::FieldType::Number;
                        } else {
                            syntheticField.type = Scripting::FieldType::Other;
                        }
                    }
                    else
                    {
                        // No saved value - use default value from Lua file if available
                        const std::string& luaDefault = parsedField.defaultValue;

                        if (!luaDefault.empty()) {
                            // Determine type and value from Lua default
                            if (luaDefault == "true" || luaDefault == "false") {
                                syntheticField.type = Scripting::FieldType::Boolean;
                                syntheticField.defaultValueSerialized = luaDefault;
                            } else if (luaDefault.front() == '"' && luaDefault.back() == '"') {
                                // Lua string literal - keep as-is for String type
                                syntheticField.type = Scripting::FieldType::String;
                                syntheticField.defaultValueSerialized = luaDefault;
                            } else if (luaDefault.front() == '\'' && luaDefault.back() == '\'') {
                                // Lua single-quoted string - convert to double-quoted
                                syntheticField.type = Scripting::FieldType::String;
                                syntheticField.defaultValueSerialized = "\"" + luaDefault.substr(1, luaDefault.size() - 2) + "\"";
                            } else if (luaDefault.front() == '{') {
                                // Lua table - can't easily convert, treat as Table
                                syntheticField.type = Scripting::FieldType::Table;
                                syntheticField.defaultValueSerialized = "{}";  // Placeholder
                            } else {
                                // Assume it's a number
                                syntheticField.type = Scripting::FieldType::Number;
                                syntheticField.defaultValueSerialized = luaDefault;
                            }
                        } else {
                            // No default value found, default to Number with 0
                            syntheticField.type = Scripting::FieldType::Number;
                            syntheticField.defaultValueSerialized = "0";
                        }
                    }

                    filteredFields.push_back(syntheticField);
                }
            }
        }
        else if (!hasFieldsTable)
        {
            // DEBUG
            if (isFirstTimeForKey) {
                ENGINE_PRINT("No fields table found (hasFieldsTable=false), using basic filtering");
            }

            for (const auto& field : fields)
            {
                // Skip functions (Start, Update, etc.)
                if (field.type == Scripting::FieldType::Function)
                    continue;

                // Skip private fields (starting with underscore)
                if (!field.name.empty() && field.name[0] == '_')
                    continue;

                // Skip special tables
                if (field.name == "__editor" || field.name == "mixins" || field.name == "fields")
                    continue;

                // Include all other fields
                filteredFields.push_back(field);
            }
        }

        // If no fields found after filtering, nothing to show
        if (filteredFields.empty())
        {
            ImGui::PopID();
            continue; // Skip to next script
        }

        // Render each field
        bool anyModified = false;
        for (const auto& field : filteredFields)
        {
            // Handle section headers
            if (field.name == "__HEADER__" && field.type == Scripting::FieldType::Nil)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", field.meta.displayName.c_str());
                ImGui::Separator();
                ImGui::Spacing();
                continue;
            }

            // Create display name (use metadata if available, otherwise use field name)
            std::string displayName = field.meta.displayName.empty() ? field.name : field.meta.displayName;

            // Convert field name from camelCase to "Proper Case" if no display name
            if (field.meta.displayName.empty() && !displayName.empty())
            {
                displayName[0] = static_cast<char>(std::toupper(displayName[0]));
                for (size_t i = 1; i < displayName.size(); ++i)
                {
                    if (std::isupper(displayName[i]) && i > 0 && std::islower(displayName[i - 1]))
                    {
                        displayName.insert(i, " ");
                        i++;
                    }
                }
            }

            ImGui::PushID(field.name.c_str());

            bool fieldModified = false;
            std::string newValue;

            // Get the comment for this field (for tooltip display)
            std::string fieldComment;
            auto commentIt = fieldComments.find(field.name);
            if (commentIt != fieldComments.end()) {
                fieldComment = commentIt->second;
            }

            // Helper lambda to render field label with tooltip
            auto renderLabelWithTooltip = [&displayName, &fieldComment]() {
                ImGui::Text("%s", displayName.c_str());
                if (!fieldComment.empty() && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", fieldComment.c_str());
                }
            };

            // Render appropriate widget based on field type
            try {
                switch (field.type)
                {
                case Scripting::FieldType::Number:
                {
                    float value = std::stof(field.defaultValueSerialized);
                    renderLabelWithTooltip();
                    ImGui::SameLine(labelWidth);
                    ImGui::SetNextItemWidth(-1);

                    if (ImGui::DragFloat(("##" + field.name).c_str(), &value, 0.1f))
                    {
                        newValue = std::to_string(value);
                        fieldModified = true;
                    }
                    break;
                }

                case Scripting::FieldType::Boolean:
                {
                    bool value = (field.defaultValueSerialized == "true" || field.defaultValueSerialized == "1");
                    renderLabelWithTooltip();
                    ImGui::SameLine(labelWidth);
                    ImGui::SetNextItemWidth(-1);

                    if (ImGui::Checkbox(("##" + field.name).c_str(), &value))
                    {
                        newValue = value ? "true" : "false";
                        fieldModified = true;
                    }
                    break;
                }

                case Scripting::FieldType::String:
                {
                    std::string currentValue = field.defaultValueSerialized;
                    if (currentValue.size() > 1 && currentValue.front() == '"' && currentValue.back() == '"')
                    {
                        currentValue = currentValue.substr(1, currentValue.size() - 2);
                    }

                    // Check if this is an asset GUID field
                    AssetType assetType = GetAssetTypeFromFieldName(field.name);
                    if (assetType != AssetType::None && IsValidGUID(currentValue))
                    {
                        // Render as asset drag-drop
                        renderLabelWithTooltip();
                        ImGui::SameLine(labelWidth);
                        ImGui::SetNextItemWidth(-1);

                        std::string guidStr = currentValue;
                        if (RenderAssetField(field.name, guidStr, assetType))
                        {
                            newValue = "\"" + guidStr + "\"";
                            fieldModified = true;
                        }
                    }
                    else
                    {
                        // Render as regular text input
                        static std::unordered_map<std::string, std::vector<char>> stringBuffers;
                        auto& buffer = stringBuffers[field.name];
                        if (buffer.size() < 256) buffer.resize(256);

                        size_t copyLen = std::min(currentValue.size(), size_t(255));
                        std::memcpy(buffer.data(), currentValue.c_str(), copyLen);
                        buffer[copyLen] = '\0';

                        renderLabelWithTooltip();
                        ImGui::SameLine(labelWidth);
                        ImGui::SetNextItemWidth(-1);

                        if (ImGui::InputText(("##" + field.name).c_str(), buffer.data(), 256))
                        {
                            newValue = std::string("\"") + buffer.data() + "\"";
                            fieldModified = true;
                        }
                    }
                    break;
                }

                case Scripting::FieldType::Table:
                {
                    // Try to parse as vector3 (table with x, y, z fields)
                    bool isVector3 = false;
                    float vec3[3] = {0.0f, 0.0f, 0.0f};

                    try {
                        rapidjson::Document doc;
                        doc.Parse(field.defaultValueSerialized.c_str());

                        if (doc.IsObject() && doc.HasMember("x") && doc.HasMember("y") && doc.HasMember("z"))
                        {
                            if (doc["x"].IsNumber() && doc["y"].IsNumber() && doc["z"].IsNumber())
                            {
                                vec3[0] = static_cast<float>(doc["x"].GetDouble());
                                vec3[1] = static_cast<float>(doc["y"].GetDouble());
                                vec3[2] = static_cast<float>(doc["z"].GetDouble());
                                isVector3 = true;
                            }
                        }
                    } catch (std::exception e) {
                        isVector3 = false;
                    }

                    if (isVector3)
                    {
                        // Render as vector3
                        renderLabelWithTooltip();
                        ImGui::SameLine(labelWidth);
                        ImGui::SetNextItemWidth(-1);

                        if (ImGui::DragFloat3(("##" + field.name).c_str(), vec3, 0.1f))
                        {
                            // Reconstruct JSON
                            rapidjson::Document doc;
                            doc.SetObject();
                            auto& alloc = doc.GetAllocator();
                            doc.AddMember("x", vec3[0], alloc);
                            doc.AddMember("y", vec3[1], alloc);
                            doc.AddMember("z", vec3[2], alloc);

                            rapidjson::StringBuffer buffer;
                            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                            doc.Accept(writer);

                            newValue = buffer.GetString();
                            fieldModified = true;
                        }
                    }
                    else
                    {
                        // Check if it's an array (JSON array)
                        rapidjson::Document doc;
                        doc.Parse(field.defaultValueSerialized.c_str());
                        if (doc.HasParseError())
                        {
                            ImGui::Text("%s: [Invalid JSON data]", displayName.c_str());
                            if (!fieldComment.empty() && ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("%s", fieldComment.c_str());
                            }
                        }
                        else if (doc.IsArray())
                        {
                            // Determine asset type for array elements
                            AssetType assetType = GetAssetTypeFromFieldName(field.name);

                            if (assetType != AssetType::None)
                            {
                                // Render as array of assets
                                renderLabelWithTooltip();
                                bool arrayModified = false;
                                rapidjson::Document newDoc;
                                newDoc.SetArray();
                                auto& alloc = newDoc.GetAllocator();

                                for (size_t i = 0; i < doc.Size(); ++i)
                                {
                                    ImGui::PushID(static_cast<int>(i));

                                    try {
                                        std::string guidStr;
                                        if (doc[i].IsString())
                                        {
                                            guidStr = doc[i].GetString();
                                        }
                                        else
                                        {
                                            guidStr = "00000000-0000-0000-0000-000000000000"; // Empty GUID
                                        }

                                        ImGui::Text("[%zu]", i + 1);
                                        ImGui::SameLine();
                                        
                                        std::string tempGuid = guidStr;
                                        if (RenderAssetField(field.name, tempGuid, assetType, ImGui::GetContentRegionAvail().x - 30.0f))
                                        {
                                            guidStr = tempGuid;
                                            arrayModified = true;
                                        }

                                        ImGui::SameLine();
                                        if (ImGui::SmallButton((std::string(ICON_FA_MINUS) + "##remove" + std::to_string(i)).c_str()))
                                        {
                                            // Skip this element (remove it)
                                            arrayModified = true;
                                        }
                                        else
                                        {
                                            // Add to new array
                                            newDoc.PushBack(rapidjson::Value(guidStr.c_str(), alloc), alloc);
                                        }
                                    } catch (std::exception e) {
                                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error rendering array element %zu", i);
                                    }

                                    ImGui::PopID();
                                }

                                // Add new element button
                                if (ImGui::Button((std::string(ICON_FA_PLUS) + "##add_" + field.name).c_str()))
                                {
                                    newDoc.PushBack(rapidjson::Value("00000000-0000-0000-0000-000000000000", alloc), alloc);
                                    arrayModified = true;
                                }

                                if (arrayModified)
                                {
                                    // Serialize new array
                                    rapidjson::StringBuffer buffer;
                                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                                    newDoc.Accept(writer);
                                    newValue = buffer.GetString();
                                    fieldModified = true;
                                }
                            }
                            else
                            {
                                // Generic array - show as text for now
                                ImGui::Text("%s: [Array with %zu elements]", displayName.c_str(), doc.Size());
                                if (!field.meta.tooltip.empty() && ImGui::IsItemHovered())
                                {
                                    ImGui::SetTooltip("%s", field.meta.tooltip.c_str());
                                }
                            }
                        }
                        else
                        {
                            // Check if it's an array-like table (JSON object with numeric string keys)
                            bool isArrayLike = false;
                            if (doc.IsObject())
                            {
                                isArrayLike = true;
                                size_t expectedIndex = 1;
                                for (auto& m : doc.GetObject())
                                {
                                    if (!m.name.IsString()) { isArrayLike = false; break; }
                                    std::string key = m.name.GetString();
                                    if (key != std::to_string(expectedIndex)) { isArrayLike = false; break; }
                                    expectedIndex++;
                                }
                            }

                            if (isArrayLike)
                            {
                                // Render as array of assets (same as JSON array case)
                                AssetType assetType = GetAssetTypeFromFieldName(field.name);

                                if (assetType != AssetType::None)
                                {
                                    // Render as array of assets
                                    renderLabelWithTooltip();
                                    bool arrayModified = false;
                                    rapidjson::Document newDoc;
                                    newDoc.SetArray();
                                    auto& alloc = newDoc.GetAllocator();

                                    size_t arraySize = doc.GetObject().MemberCount();
                                    for (size_t i = 0; i < arraySize; ++i)
                                    {
                                        std::string key = std::to_string(i + 1);
                                        auto it = doc.FindMember(key.c_str());
                                        if (it == doc.MemberEnd()) continue;

                                        ImGui::PushID(static_cast<int>(i));

                                        try {
                                            std::string guidStr;
                                            if (it->value.IsString())
                                            {
                                                guidStr = it->value.GetString();
                                            }
                                            else
                                            {
                                                guidStr = "00000000-0000-0000-0000-000000000000"; // Empty GUID
                                            }

                                            ImGui::Text("[%zu]", i + 1);
                                            ImGui::SameLine();
                                            
                                            std::string tempGuid = guidStr;
                                            if (RenderAssetField(field.name, tempGuid, assetType, ImGui::GetContentRegionAvail().x - 30.0f))
                                            {
                                                guidStr = tempGuid;
                                                arrayModified = true;
                                            }

                                            ImGui::SameLine();
                                            if (ImGui::SmallButton((std::string(ICON_FA_MINUS) + "##remove" + std::to_string(i)).c_str()))
                                            {
                                                // Skip this element (remove it)
                                                arrayModified = true;
                                            }
                                            else
                                            {
                                                // Add to new array
                                                newDoc.PushBack(rapidjson::Value(guidStr.c_str(), alloc), alloc);
                                            }
                                        } catch (std::exception e) {
                                            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error rendering array element %zu", i);
                                        }

                                        ImGui::PopID();
                                    }

                                    // Add new element button
                                    if (ImGui::Button((std::string(ICON_FA_PLUS) + "##add_" + field.name).c_str()))
                                    {
                                        newDoc.PushBack(rapidjson::Value("00000000-0000-0000-0000-000000000000", alloc), alloc);
                                        arrayModified = true;
                                    }

                                    if (arrayModified)
                                    {
                                        // Serialize new array
                                        rapidjson::StringBuffer buffer;
                                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                                        newDoc.Accept(writer);
                                        newValue = buffer.GetString();
                                        fieldModified = true;
                                    }
                                }
                                else
                                {
                                    // Generic array-like table - show as text for now
                                    ImGui::Text("%s: [Array with %zu elements]", displayName.c_str(), doc.GetObject().MemberCount());
                                    if (ImGui::IsItemHovered()) {
                                        std::string tooltip = !fieldComment.empty() ? fieldComment : field.meta.tooltip;
                                        if (!tooltip.empty()) ImGui::SetTooltip("%s", tooltip.c_str());
                                    }
                                }
                            }
                            else
                            {
                                // Generic table
                                ImGui::Text("%s: [Table]", displayName.c_str());
                                if (ImGui::IsItemHovered()) {
                                    std::string tooltip = !fieldComment.empty() ? fieldComment : field.meta.tooltip;
                                    if (!tooltip.empty()) ImGui::SetTooltip("%s", tooltip.c_str());
                                }
                            }
                        }
                    }
                    break;
                }

                default:
                {
                    ImGui::Text("%s: %s", displayName.c_str(), field.defaultValueSerialized.c_str());
                    if (ImGui::IsItemHovered()) {
                        std::string tooltip = !fieldComment.empty() ? fieldComment : field.meta.tooltip;
                        if (!tooltip.empty()) ImGui::SetTooltip("%s", tooltip.c_str());
                    }
                    break;
                }
            }
            }
            catch (const std::exception& e) {
                // Commented out to fix warning C4101 - unreferenced local variable
                // Remove this line when 'e' is used
                (void)e;

                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error rendering field %s", field.name.c_str());
            }

            // Show tooltip if available (prioritize Lua file comment over __editor metadata)
            // Don't show __SYNTHETIC__ marker as tooltip
            if (!fieldModified && (field.type != Scripting::FieldType::Table) && ImGui::IsItemHovered())
            {
                std::string tooltip = !fieldComment.empty() ? fieldComment : field.meta.tooltip;
                if (!tooltip.empty() && tooltip != "__SYNTHETIC__") {
                    ImGui::SetTooltip("%s", tooltip.c_str());
                }
            }

            // Check if this is a synthetic field (from incomplete preview instance)
            bool isSyntheticField = (field.meta.tooltip == "__SYNTHETIC__");

            // If field was modified, update the Lua instance or pendingInstanceState
            if (fieldModified && !newValue.empty())
            {
                if (isSyntheticField)
                {
                    // SYNTHETIC FIELD: Cannot update Lua instance, directly modify pendingInstanceState JSON
                    anyModified = true;

                    try {
                        rapidjson::Document stateDoc;
                        if (!scriptData.pendingInstanceState.empty()) {
                            stateDoc.Parse(scriptData.pendingInstanceState.c_str());
                        }
                        if (stateDoc.HasParseError() || !stateDoc.IsObject()) {
                            stateDoc.SetObject();
                        }

                        auto& alloc = stateDoc.GetAllocator();

                        // Remove existing field if present
                        if (stateDoc.HasMember(field.name.c_str())) {
                            stateDoc.RemoveMember(field.name.c_str());
                        }

                        // Parse the new value and add it
                        rapidjson::Document valueDoc;
                        valueDoc.Parse(newValue.c_str());
                        if (!valueDoc.HasParseError()) {
                            rapidjson::Value nameVal(field.name.c_str(), alloc);
                            rapidjson::Value valCopy(valueDoc, alloc);
                            stateDoc.AddMember(nameVal, valCopy, alloc);
                        }

                        // Serialize back
                        rapidjson::StringBuffer buffer;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                        stateDoc.Accept(writer);
                        scriptData.pendingInstanceState = buffer.GetString();

                        ENGINE_PRINT("SYNTHETIC FIELD UPDATE: '", field.name.c_str(), "' = ", newValue.c_str());
                    } catch (...) {
                        ENGINE_PRINT("Error updating synthetic field: ", field.name.c_str());
                    }

                    // Take snapshot for undo
                    SnapshotManager::GetInstance().TakeSnapshot("Modify Script Property: " + field.name);
                }
                else if (inspector.SetFieldFromString(L, instanceToInspect, field, newValue))
                {
                    anyModified = true;

                    // Always save state to pendingInstanceState for persistence across mode transitions
                    // This ensures edited values persist when entering play mode (like Unity)
                    scriptData.pendingInstanceState = Scripting::SerializeInstanceToJson(instanceToInspect);
                    ENGINE_PRINT("SAVE DEBUG: Updated pendingInstanceState for field '", field.name.c_str(), "' to: ", newValue.c_str());
                    ENGINE_PRINT("  pendingInstanceState.size = ", scriptData.pendingInstanceState.size());

                    // Take snapshot for undo
                    SnapshotManager::GetInstance().TakeSnapshot("Modify Script Property: " + field.name);
                }
            }

            ImGui::PopID();
        } // End of for loop over FIELDS

        ImGui::PopID(); // Pop script index ID
        } // End of for loop over scripts

        // Handle script removal (do this after the loop to avoid iterator invalidation)
        if (scriptIndexToRemove >= 0 && scriptIndexToRemove < static_cast<int>(scriptComp.scripts.size()))
        {
            SnapshotManager::GetInstance().TakeSnapshot("Remove Script");

            // Clean up preview instance for the removed script
            std::string uniqueKey = std::to_string(entity) + "_" + std::to_string(scriptIndexToRemove);
            editorPreviewInstances.erase(uniqueKey);
            editorPreviewScriptPaths.erase(uniqueKey);

            // Remove the script
            scriptComp.scripts.erase(scriptComp.scripts.begin() + scriptIndexToRemove);
        }

        return true; // Skip default rendering
    });

    // Hide the "scripts" field - we render it ourselves in the component renderer
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "scripts",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; }); // Hidden

    // Hide internal fields from old structure (for safety, though they no longer exist)
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "enabled",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "preserveKeys",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "entryFunction",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "autoInvokeEntry",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    // ==================== BUTTON COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("ButtonComponent",
    [](void* componentPtr, TypeDescriptor_Struct*, Entity, ECSManager&) -> bool
    {
        ButtonComponent& buttonComp = *static_cast<ButtonComponent*>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Helper lambda: Parse Lua script to extract function names
        auto extractLuaFunctions = [](const std::string& scriptPath) -> std::vector<std::string> {
            std::vector<std::string> functions;
            if (scriptPath.empty()) return functions;

            // Normalize path separators for comparison
            std::string normalizedPath = scriptPath;
            std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

            // Convert to lowercase for case-insensitive comparison
            std::string lowerPath = normalizedPath;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

            // Get full path to script - try multiple strategies
            std::filesystem::path fullPath;
            std::string rootDir = AssetManager::GetInstance().GetRootAssetDirectory();
            std::filesystem::path projectRoot = std::filesystem::path(rootDir).parent_path();

            // Strategy 1: Check if path is already absolute
            if (std::filesystem::path(normalizedPath).is_absolute()) {
                fullPath = normalizedPath;
            }
            // Strategy 2: Path starts with Resources/ or resources/
            else if (lowerPath.find("resources/") == 0) {
                fullPath = projectRoot / normalizedPath;
            }
            // Strategy 3: Path starts with scripts/ (without Resources prefix)
            else if (lowerPath.find("scripts/") == 0) {
                fullPath = projectRoot / "Resources" / normalizedPath;
            }
            // Strategy 4: Just the filename - try common locations
            else {
                // Try Resources/Scripts/
                fullPath = projectRoot / "Resources" / "Scripts" / normalizedPath;
                if (!std::filesystem::exists(fullPath)) {
                    // Try rootDir (which is typically Resources)
                    fullPath = std::filesystem::path(rootDir) / "Scripts" / normalizedPath;
                }
                if (!std::filesystem::exists(fullPath)) {
                    // Try project root
                    fullPath = projectRoot / normalizedPath;
                }
            }

            std::ifstream file(fullPath);
            if (!file.is_open()) {
                // Debug: Log the attempted path if file not found
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ButtonComponent] Could not open script file: ", fullPath.string().c_str());
                return functions;
            }

            std::string line;
            while (std::getline(file, line)) {
                std::string funcName;

                // Pattern 1: Traditional function definition
                // function ClassName:FunctionName() or function ClassName.FunctionName() or function FunctionName()
                size_t funcPos = line.find("function ");
                if (funcPos != std::string::npos) {
                    size_t start = funcPos + 9; // After "function "
                    size_t colonPos = line.find(':', start);
                    size_t dotPos = line.find('.', start);
                    size_t parenPos = line.find('(', start);

                    if (parenPos != std::string::npos) {
                        if (colonPos != std::string::npos && colonPos < parenPos) {
                            // ClassName:FunctionName pattern
                            funcName = line.substr(colonPos + 1, parenPos - colonPos - 1);
                        } else if (dotPos != std::string::npos && dotPos < parenPos) {
                            // ClassName.FunctionName pattern
                            funcName = line.substr(dotPos + 1, parenPos - dotPos - 1);
                        } else {
                            // Just FunctionName pattern
                            funcName = line.substr(start, parenPos - start);
                        }
                    }
                }

                // Pattern 2: Anonymous function assignment (common in Lua table definitions)
                // FunctionName = function( or FunctionName=function(
                if (funcName.empty()) {
                    size_t eqFuncPos = line.find("= function(");
                    if (eqFuncPos == std::string::npos) {
                        eqFuncPos = line.find("=function(");
                    }
                    if (eqFuncPos != std::string::npos) {
                        // Extract the name before the '='
                        size_t nameEnd = eqFuncPos;
                        // Skip whitespace before '='
                        while (nameEnd > 0 && (line[nameEnd - 1] == ' ' || line[nameEnd - 1] == '\t')) {
                            nameEnd--;
                        }
                        // Find the start of the name (scan backwards for non-identifier chars)
                        size_t nameStart = nameEnd;
                        while (nameStart > 0 && (std::isalnum(line[nameStart - 1]) || line[nameStart - 1] == '_')) {
                            nameStart--;
                        }
                        if (nameStart < nameEnd) {
                            funcName = line.substr(nameStart, nameEnd - nameStart);
                        }
                    }
                }

                // Process the extracted function name
                if (!funcName.empty()) {
                    // Trim whitespace
                    funcName.erase(0, funcName.find_first_not_of(" \t"));
                    if (!funcName.empty()) {
                        funcName.erase(funcName.find_last_not_of(" \t") + 1);
                    }

                    // Skip internal/lifecycle functions
                    if (!funcName.empty() &&
                        funcName != "new" && funcName != "New" &&
                        funcName != "Awake" && funcName != "Start" &&
                        funcName != "Update" && funcName != "FixedUpdate" &&
                        funcName != "OnDestroy" && funcName != "OnEnable" &&
                        funcName != "OnDisable" && funcName != "fields") {
                        functions.push_back(funcName);
                    }
                }
            }
            return functions;
        };

        // Cache for script functions
        static std::unordered_map<std::string, std::vector<std::string>> scriptFunctionsCache;

        // Interactable toggle
        ImGui::Text("Interactable");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::Checkbox("##Interactable", &buttonComp.interactable);

        ImGui::Separator();
        ImGui::Text("On Click ()");

        // Render existing bindings
        int bindingToRemove = -1;
        for (size_t i = 0; i < buttonComp.bindings.size(); ++i) {
            ButtonBinding& binding = buttonComp.bindings[i];
            ImGui::PushID(static_cast<int>(i));

            // Binding header with remove button
            ImGui::BeginGroup();

            // Script field with drag-drop
            std::string scriptDisplayName = binding.scriptPath.empty() ? "None (Script)" :
                std::filesystem::path(binding.scriptPath).stem().string();

            ImGui::Text("Script");
            ImGui::SameLine(labelWidth);
            float fieldWidth = ImGui::GetContentRegionAvail().x - 25.0f;
            EditorComponents::DrawDragDropButton(scriptDisplayName.c_str(), fieldWidth);

            // Drag-drop target for scripts
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PAYLOAD")) {
                    SnapshotManager::GetInstance().TakeSnapshot("Assign Button Script");
                    const char* droppedPath = (const char*)payload->Data;
                    std::string pathStr(droppedPath, payload->DataSize);
                    pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                    binding.scriptGuidStr = GUIDUtilities::ConvertGUID128ToString(DraggedScriptGuid);
                    binding.scriptPath = pathStr;
                    binding.functionName = ""; // Reset function when script changes

                    // Invalidate cache for this script
                    scriptFunctionsCache.erase(pathStr);
                }
                ImGui::EndDragDropTarget();
            }

            // Remove button
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_TRASH "##RemoveBinding")) {
                bindingToRemove = static_cast<int>(i);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove this binding");
            }

            // Function dropdown (only if script is assigned)
            if (!binding.scriptPath.empty()) {
                // Get or cache functions for this script
                auto cacheIt = scriptFunctionsCache.find(binding.scriptPath);
                if (cacheIt == scriptFunctionsCache.end()) {
                    scriptFunctionsCache[binding.scriptPath] = extractLuaFunctions(binding.scriptPath);
                    cacheIt = scriptFunctionsCache.find(binding.scriptPath);
                }

                const std::vector<std::string>& functions = cacheIt->second;

                ImGui::Text("Function");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);

                std::string previewFunc = binding.functionName.empty() ? "No Function" : binding.functionName;
                EditorComponents::PushComboColors();
                if (ImGui::BeginCombo("##Function", previewFunc.c_str())) {
                    // "No Function" option
                    if (ImGui::Selectable("No Function", binding.functionName.empty())) {
                        binding.functionName = "";
                    }

                    // Available functions
                    for (const auto& funcName : functions) {
                        bool isSelected = (binding.functionName == funcName);
                        if (ImGui::Selectable(funcName.c_str(), isSelected)) {
                            SnapshotManager::GetInstance().TakeSnapshot("Set Button Function");
                            binding.functionName = funcName;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    if (functions.empty()) {
                        ImGui::TextDisabled("No functions found in script");
                    }

                    ImGui::EndCombo();
                }
                EditorComponents::PopComboColors();
            }

            ImGui::EndGroup();
            ImGui::Separator();
            ImGui::PopID();
        }

        // Remove binding if requested
        if (bindingToRemove >= 0 && bindingToRemove < static_cast<int>(buttonComp.bindings.size())) {
            SnapshotManager::GetInstance().TakeSnapshot("Remove Button Binding");
            buttonComp.bindings.erase(buttonComp.bindings.begin() + bindingToRemove);
        }

        // Add binding button
        if (ImGui::Button(ICON_FA_PLUS " Add Binding", ImVec2(-1, 0))) {
            SnapshotManager::GetInstance().TakeSnapshot("Add Button Binding");
            ButtonBinding newBinding;
            buttonComp.bindings.push_back(newBinding);
        }

        return true; // Skip default reflection rendering
    });

    // Hide ButtonComponent fields from default rendering (we handle them in the custom renderer)
    ReflectionRenderer::RegisterFieldRenderer("ButtonComponent", "bindings",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("ButtonComponent", "interactable",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });

    // ==================== SLIDER COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("SliderComponent",
    [](void* componentPtr, TypeDescriptor_Struct*, Entity, ECSManager& ecs) -> bool
    {
        SliderComponent& sliderComp = *static_cast<SliderComponent*>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Helper lambda: Parse Lua script to extract function names
        auto extractLuaFunctions = [](const std::string& scriptPath) -> std::vector<std::string> {
            std::vector<std::string> functions;
            if (scriptPath.empty()) return functions;

            std::string normalizedPath = scriptPath;
            std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

            std::string lowerPath = normalizedPath;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

            std::filesystem::path fullPath;
            std::string rootDir = AssetManager::GetInstance().GetRootAssetDirectory();
            std::filesystem::path projectRoot = std::filesystem::path(rootDir).parent_path();

            if (std::filesystem::path(normalizedPath).is_absolute()) {
                fullPath = normalizedPath;
            }
            else if (lowerPath.find("resources/") == 0) {
                fullPath = projectRoot / normalizedPath;
            }
            else if (lowerPath.find("scripts/") == 0) {
                fullPath = projectRoot / "Resources" / normalizedPath;
            }
            else {
                fullPath = projectRoot / "Resources" / "Scripts" / normalizedPath;
                if (!std::filesystem::exists(fullPath)) {
                    fullPath = std::filesystem::path(rootDir) / "Scripts" / normalizedPath;
                }
                if (!std::filesystem::exists(fullPath)) {
                    fullPath = projectRoot / normalizedPath;
                }
            }

            std::ifstream file(fullPath);
            if (!file.is_open()) {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[SliderComponent] Could not open script file: ", fullPath.string().c_str());
                return functions;
            }

            std::string line;
            while (std::getline(file, line)) {
                std::string funcName;

                size_t funcPos = line.find("function ");
                if (funcPos != std::string::npos) {
                    size_t start = funcPos + 9;
                    size_t colonPos = line.find(':', start);
                    size_t dotPos = line.find('.', start);
                    size_t parenPos = line.find('(', start);

                    if (parenPos != std::string::npos) {
                        if (colonPos != std::string::npos && colonPos < parenPos) {
                            funcName = line.substr(colonPos + 1, parenPos - colonPos - 1);
                        } else if (dotPos != std::string::npos && dotPos < parenPos) {
                            funcName = line.substr(dotPos + 1, parenPos - dotPos - 1);
                        } else {
                            funcName = line.substr(start, parenPos - start);
                        }
                    }
                }

                if (funcName.empty()) {
                    size_t eqFuncPos = line.find("= function(");
                    if (eqFuncPos == std::string::npos) {
                        eqFuncPos = line.find("=function(");
                    }
                    if (eqFuncPos != std::string::npos) {
                        size_t nameEnd = eqFuncPos;
                        while (nameEnd > 0 && (line[nameEnd - 1] == ' ' || line[nameEnd - 1] == '\t')) {
                            nameEnd--;
                        }
                        size_t nameStart = nameEnd;
                        while (nameStart > 0 && (std::isalnum(line[nameStart - 1]) || line[nameStart - 1] == '_')) {
                            nameStart--;
                        }
                        if (nameStart < nameEnd) {
                            funcName = line.substr(nameStart, nameEnd - nameStart);
                        }
                    }
                }

                if (!funcName.empty()) {
                    funcName.erase(0, funcName.find_first_not_of(" \t"));
                    if (!funcName.empty()) {
                        funcName.erase(funcName.find_last_not_of(" \t") + 1);
                    }

                    if (!funcName.empty() &&
                        funcName != "new" && funcName != "New" &&
                        funcName != "Awake" && funcName != "Start" &&
                        funcName != "Update" && funcName != "FixedUpdate" &&
                        funcName != "OnDestroy" && funcName != "OnEnable" &&
                        funcName != "OnDisable" && funcName != "fields") {
                        functions.push_back(funcName);
                    }
                }
            }
            return functions;
        };

        static std::unordered_map<std::string, std::vector<std::string>> scriptFunctionsCache;

        // Core slider fields
        ImGui::Text("Min Value");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##SliderMin", &sliderComp.minValue, 0.1f);

        ImGui::Text("Max Value");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##SliderMax", &sliderComp.maxValue, 0.1f);

        if (sliderComp.maxValue < sliderComp.minValue) {
            std::swap(sliderComp.maxValue, sliderComp.minValue);
        }

        ImGui::Text("Value");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##SliderValue", &sliderComp.value, 0.1f, sliderComp.minValue, sliderComp.maxValue);

        sliderComp.value = std::max(sliderComp.minValue, std::min(sliderComp.maxValue, sliderComp.value));
        if (sliderComp.wholeNumbers) {
            sliderComp.value = std::round(sliderComp.value);
        }

        ImGui::Text("Whole Numbers");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::Checkbox("##SliderWhole", &sliderComp.wholeNumbers);

        ImGui::Text("Interactable");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::Checkbox("##SliderInteractable", &sliderComp.interactable);

        ImGui::Text("Horizontal");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::Checkbox("##SliderHorizontal", &sliderComp.horizontal);

        // Show track/handle info
        auto showChildName = [&](const char* label, const GUID_128& guid) {
            ImGui::Text("%s", label);
            ImGui::SameLine(labelWidth);
            std::string display = "Missing";
            if (guid.high != 0 || guid.low != 0) {
                Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
                if (child != static_cast<Entity>(-1) && ecs.HasComponent<NameComponent>(child)) {
                    display = ecs.GetComponent<NameComponent>(child).name;
                }
            }
            ImGui::TextDisabled("%s", display.c_str());
        };

        ImGui::Separator();
        showChildName("Track", sliderComp.trackEntityGuid);
        showChildName("Handle", sliderComp.handleEntityGuid);

        ImGui::Separator();
        ImGui::Text("On Value Changed ()");

        int bindingToRemove = -1;
        for (size_t i = 0; i < sliderComp.onValueChanged.size(); ++i) {
            SliderBinding& binding = sliderComp.onValueChanged[i];
            ImGui::PushID(static_cast<int>(i));

            std::string scriptDisplayName = binding.scriptPath.empty() ? "None (Script)" :
                std::filesystem::path(binding.scriptPath).stem().string();

            ImGui::Text("Script");
            ImGui::SameLine(labelWidth);
            float fieldWidth = ImGui::GetContentRegionAvail().x - 25.0f;
            EditorComponents::DrawDragDropButton(scriptDisplayName.c_str(), fieldWidth);

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PAYLOAD")) {
                    SnapshotManager::GetInstance().TakeSnapshot("Assign Slider Script");
                    const char* droppedPath = (const char*)payload->Data;
                    std::string pathStr(droppedPath, payload->DataSize);
                    pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                    binding.scriptGuidStr = GUIDUtilities::ConvertGUID128ToString(DraggedScriptGuid);
                    binding.scriptPath = pathStr;
                    binding.functionName = "";

                    scriptFunctionsCache.erase(pathStr);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_TRASH "##RemoveSliderBinding")) {
                bindingToRemove = static_cast<int>(i);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove this binding");
            }

            if (!binding.scriptPath.empty()) {
                auto cacheIt = scriptFunctionsCache.find(binding.scriptPath);
                if (cacheIt == scriptFunctionsCache.end()) {
                    scriptFunctionsCache[binding.scriptPath] = extractLuaFunctions(binding.scriptPath);
                    cacheIt = scriptFunctionsCache.find(binding.scriptPath);
                }

                const std::vector<std::string>& functions = cacheIt->second;

                ImGui::Text("Function");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);

                std::string previewFunc = binding.functionName.empty() ? "No Function" : binding.functionName;
                EditorComponents::PushComboColors();
                if (ImGui::BeginCombo("##SliderFunction", previewFunc.c_str())) {
                    if (ImGui::Selectable("No Function", binding.functionName.empty())) {
                        binding.functionName = "";
                    }

                    for (const auto& funcName : functions) {
                        bool isSelected = (binding.functionName == funcName);
                        if (ImGui::Selectable(funcName.c_str(), isSelected)) {
                            SnapshotManager::GetInstance().TakeSnapshot("Set Slider Function");
                            binding.functionName = funcName;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    if (functions.empty()) {
                        ImGui::TextDisabled("No functions found in script");
                    }

                    ImGui::EndCombo();
                }
                EditorComponents::PopComboColors();
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (bindingToRemove >= 0 && bindingToRemove < static_cast<int>(sliderComp.onValueChanged.size())) {
            SnapshotManager::GetInstance().TakeSnapshot("Remove Slider Binding");
            sliderComp.onValueChanged.erase(sliderComp.onValueChanged.begin() + bindingToRemove);
        }

        if (ImGui::Button(ICON_FA_PLUS " Add Binding", ImVec2(-1, 0))) {
            SnapshotManager::GetInstance().TakeSnapshot("Add Slider Binding");
            SliderBinding newBinding;
            sliderComp.onValueChanged.push_back(newBinding);
        }

        return true; // Skip default reflection rendering
    });

    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "onValueChanged",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "minValue",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "maxValue",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "value",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "wholeNumbers",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "interactable",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "horizontal",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "trackEntityGuid",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SliderComponent", "handleEntityGuid",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });

    // ==================== UI ANCHOR COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("UIAnchorComponent",
    [](void* componentPtr, TypeDescriptor_Struct*, Entity, ECSManager&) -> bool
    {
        UIAnchorComponent& anchor = *static_cast<UIAnchorComponent*>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Anchor Preset dropdown
        ImGui::Text("Preset");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char* presetNames[] = {
            "Custom", "Top Left", "Top Center", "Top Right",
            "Middle Left", "Center", "Middle Right",
            "Bottom Left", "Bottom Center", "Bottom Right"
        };
        int currentPreset = static_cast<int>(anchor.GetCurrentPreset());
        if (ImGui::Combo("##AnchorPreset", &currentPreset, presetNames, IM_ARRAYSIZE(presetNames))) {
            anchor.SetPreset(static_cast<UIAnchorPreset>(currentPreset));
            SnapshotManager::GetInstance().TakeSnapshot("Change Anchor Preset");
        }

        ImGui::Separator();

        // Anchor position
        ImGui::Text("Anchor X");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (UndoableWidgets::SliderFloat("##AnchorX", &anchor.anchorX, 0.0f, 1.0f, "%.2f")) {
            // Value changed
        }

        ImGui::Text("Anchor Y");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (UndoableWidgets::SliderFloat("##AnchorY", &anchor.anchorY, 0.0f, 1.0f, "%.2f")) {
            // Value changed
        }

        ImGui::Separator();

        // Offset
        ImGui::Text("Offset X");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##OffsetX", &anchor.offsetX, 1.0f, -10000.0f, 10000.0f, "%.1f");

        ImGui::Text("Offset Y");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        UndoableWidgets::DragFloat("##OffsetY", &anchor.offsetY, 1.0f, -10000.0f, 10000.0f, "%.1f");

        ImGui::Separator();

        // Size Mode dropdown
        ImGui::Text("Size Mode");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char* sizeModeNames[] = {
            "Fixed", "Stretch X", "Stretch Y", "Stretch Both", "Scale Uniform"
        };
        int currentSizeMode = static_cast<int>(anchor.sizeMode);
        if (ImGui::Combo("##SizeMode", &currentSizeMode, sizeModeNames, IM_ARRAYSIZE(sizeModeNames))) {
            anchor.sizeMode = static_cast<UISizeMode>(currentSizeMode);
            SnapshotManager::GetInstance().TakeSnapshot("Change Size Mode");
        }

        // Show margins for stretch modes
        if (anchor.sizeMode == UISizeMode::StretchX ||
            anchor.sizeMode == UISizeMode::StretchY ||
            anchor.sizeMode == UISizeMode::StretchBoth) {

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Margins");

            ImGui::Text("Left");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##MarginLeft", &anchor.marginLeft, 1.0f, 0.0f, 10000.0f, "%.0f");

            ImGui::Text("Right");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##MarginRight", &anchor.marginRight, 1.0f, 0.0f, 10000.0f, "%.0f");

            ImGui::Text("Top");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##MarginTop", &anchor.marginTop, 1.0f, 0.0f, 10000.0f, "%.0f");

            ImGui::Text("Bottom");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##MarginBottom", &anchor.marginBottom, 1.0f, 0.0f, 10000.0f, "%.0f");
        }

        // Show reference resolution for ScaleUniform mode
        if (anchor.sizeMode == UISizeMode::ScaleUniform) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Reference Resolution");

            ImGui::Text("Width");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##RefWidth", &anchor.referenceWidth, 1.0f, 1.0f, 10000.0f, "%.0f");

            ImGui::Text("Height");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::DragFloat("##RefHeight", &anchor.referenceHeight, 1.0f, 1.0f, 10000.0f, "%.0f");
        }

        return true; // Skip default reflection rendering
    });

    // Hide UIAnchorComponent fields from default rendering
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "anchorX",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "anchorY",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "offsetX",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "offsetY",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "marginLeft",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "marginRight",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "marginTop",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "marginBottom",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "referenceWidth",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("UIAnchorComponent", "referenceHeight",
                                              [](const char*, void*, Entity, ECSManager&)
                                              { return true; });

    // ==================== SPRITE ANIMATION COMPONENT ====================
    // Register the sprite animation inspector (defined in SpriteAnimationInspector.cpp)
    RegisterSpriteAnimationInspector();
}
