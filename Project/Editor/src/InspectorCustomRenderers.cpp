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
#include <unordered_map>
#include "Logging.hpp"
#include "ReflectionRenderer.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "UndoSystem.hpp"
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
#include "Video/VideoComponent.hpp"
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
extern GUID_128 DraggedTextGuid;
extern std::string DraggedTextPath;


// Helper function to determine asset type from field name
enum class AssetType { None, Audio, Model, Texture, Material, Font, Script, Text };
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
    if (lowerName.find("text") != std::string::npos ||
        lowerName.find("config") != std::string::npos ||
        lowerName.find("cutscene") != std::string::npos ||
        lowerName.find("data") != std::string::npos) {
        return AssetType::Text;
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
        case AssetType::Text: {
            GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
            std::string path = AssetManager::GetInstance().GetAssetPathFromGUID(guid);
            displayText = path.empty() ? "None (Text)" : path.substr(path.find_last_of("/\\") + 1);
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
            case AssetType::Text: payloadType = "TEXT_PAYLOAD"; break;
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
                    case AssetType::Text: newGuid = DraggedTextGuid; break;
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

// Helper function to convert simple Lua table string to JSON string
// Handles: {x = -2, y = 1, z = -2} -> {"x":-2,"y":1,"z":-2}
// Handles: {"EnemyAI", "FlyingEnemyLogic"} -> ["EnemyAI","FlyingEnemyLogic"]
// Handles: {} -> []
std::string ConvertLuaTableToJson(const std::string& luaTable) {
    if (luaTable.empty() || luaTable.front() != '{' || luaTable.back() != '}') {
        return "{}";
    }

    // Remove outer braces
    std::string content = luaTable.substr(1, luaTable.size() - 2);

    // Trim whitespace
    size_t start = content.find_first_not_of(" \t\n\r");
    size_t end = content.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "[]"; // Empty table
    }
    content = content.substr(start, end - start + 1);

    if (content.empty()) {
        return "[]"; // Empty table
    }

    // Check if it's an array-style table (no "=" signs means array)
    bool isArray = (content.find('=') == std::string::npos);

    rapidjson::Document doc;
    auto& alloc = doc.GetAllocator();

    if (isArray) {
        // Parse as array: {"val1", "val2", 3, 4}
        doc.SetArray();

        size_t pos = 0;
        while (pos < content.size()) {
            // Skip whitespace
            while (pos < content.size() && std::isspace(content[pos])) pos++;
            if (pos >= content.size()) break;

            std::string value;
            bool inString = false;
            char stringDelim = 0;

            while (pos < content.size()) {
                char c = content[pos];

                if (!inString) {
                    if (c == '"' || c == '\'') {
                        inString = true;
                        stringDelim = c;
                    } else if (c == ',') {
                        pos++; // Skip comma
                        break;
                    } else {
                        value += c;
                    }
                } else {
                    if (c == stringDelim) {
                        inString = false;
                    } else {
                        value += c;
                    }
                }
                pos++;
            }

            // Trim value
            size_t vstart = value.find_first_not_of(" \t\n\r");
            size_t vend = value.find_last_not_of(" \t\n\r");
            if (vstart != std::string::npos) {
                value = value.substr(vstart, vend - vstart + 1);
            } else {
                value = "";
            }

            if (!value.empty()) {
                // Try to parse as number
                try {
                    size_t processed = 0;
                    double num = std::stod(value, &processed);
                    if (processed == value.size()) {
                        doc.PushBack(rapidjson::Value(num), alloc);
                    } else {
                        doc.PushBack(rapidjson::Value(value.c_str(), alloc), alloc);
                    }
                } catch (...) {
                    doc.PushBack(rapidjson::Value(value.c_str(), alloc), alloc);
                }
            }
        }
    } else {
        // Parse as object: {x = -2, y = 1, z = -2}
        doc.SetObject();

        size_t pos = 0;
        while (pos < content.size()) {
            // Skip whitespace
            while (pos < content.size() && std::isspace(content[pos])) pos++;
            if (pos >= content.size()) break;

            // Parse key
            std::string key;
            while (pos < content.size() && content[pos] != '=' && content[pos] != ',' && !std::isspace(content[pos])) {
                key += content[pos++];
            }

            // Skip whitespace and '='
            while (pos < content.size() && (std::isspace(content[pos]) || content[pos] == '=')) pos++;

            if (key.empty()) {
                // Skip to next comma
                while (pos < content.size() && content[pos] != ',') pos++;
                if (pos < content.size()) pos++; // Skip comma
                continue;
            }

            // Parse value
            std::string value;
            bool inString = false;
            char stringDelim = 0;
            int braceDepth = 0;

            while (pos < content.size()) {
                char c = content[pos];

                if (!inString) {
                    if (c == '"' || c == '\'') {
                        inString = true;
                        stringDelim = c;
                        value += c;
                    } else if (c == '{') {
                        braceDepth++;
                        value += c;
                    } else if (c == '}') {
                        if (braceDepth > 0) {
                            braceDepth--;
                            value += c;
                        } else {
                            break;
                        }
                    } else if (c == ',' && braceDepth == 0) {
                        pos++; // Skip comma
                        break;
                    } else {
                        value += c;
                    }
                } else {
                    value += c;
                    if (c == stringDelim) {
                        inString = false;
                    }
                }
                pos++;
            }

            // Trim value
            size_t vstart = value.find_first_not_of(" \t\n\r");
            size_t vend = value.find_last_not_of(" \t\n\r");
            if (vstart != std::string::npos) {
                value = value.substr(vstart, vend - vstart + 1);
            } else {
                value = "";
            }

            if (!key.empty() && !value.empty()) {
                // Remove quotes from string values
                if ((value.front() == '"' && value.back() == '"') ||
                    (value.front() == '\'' && value.back() == '\'')) {
                    value = value.substr(1, value.size() - 2);
                    doc.AddMember(
                        rapidjson::Value(key.c_str(), alloc),
                        rapidjson::Value(value.c_str(), alloc),
                        alloc
                    );
                } else {
                    // Try to parse as number
                    try {
                        size_t processed = 0;
                        double num = std::stod(value, &processed);
                        if (processed == value.size()) {
                            doc.AddMember(
                                rapidjson::Value(key.c_str(), alloc),
                                rapidjson::Value(num),
                                alloc
                            );
                        } else {
                            doc.AddMember(
                                rapidjson::Value(key.c_str(), alloc),
                                rapidjson::Value(value.c_str(), alloc),
                                alloc
                            );
                        }
                    } catch (...) {
                        doc.AddMember(
                            rapidjson::Value(key.c_str(), alloc),
                            rapidjson::Value(value.c_str(), alloc),
                            alloc
                        );
                    }
                }
            }
        }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
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
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("NameComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        NameComponent &nameComp = *static_cast<NameComponent *>(componentPtr);

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, bool> startIsActive;
        static std::unordered_map<Entity, std::string> startName;
        static std::unordered_map<Entity, bool> isEditingName;

        // Initialize tracking state
        if (isEditingName.find(entity) == isEditingName.end()) isEditingName[entity] = false;

        if (ecs.HasComponent<ActiveComponent>(entity))
        {
            auto &activeComp = ecs.GetComponent<ActiveComponent>(entity);

            // Style the checkbox to be smaller with white checkmark
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));                  // Smaller padding
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));      // White checkmark
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));        // Dark gray background
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Lighter on hover
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Even lighter when clicking

            // Entity-aware checkbox for active state
            startIsActive[entity] = activeComp.isActive;
            bool isActiveVal = activeComp.isActive;
            if (ImGui::Checkbox("##EntityActive", &isActiveVal)) {
                bool oldVal = startIsActive[entity];
                bool newVal = isActiveVal;
                activeComp.isActive = newVal;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ActiveComponent>(entity)) {
                                ecs.GetComponent<ActiveComponent>(entity).isActive = newVal;
                            }
                        },
                        [entity, oldVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ActiveComponent>(entity)) {
                                ecs.GetComponent<ActiveComponent>(entity).isActive = oldVal;
                            }
                        },
                        "Toggle Entity Active"
                    );
                }
            }

            ImGui::PopStyleColor(4); // Pop all 4 colors
            ImGui::PopStyleVar();    // Pop padding

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Enable/Disable Entity");
            }
            ImGui::SameLine();
        }

        // Simple text input for name with entity-aware undo
        char buf[128] = {};
        std::snprintf(buf, sizeof(buf), "%s", nameComp.name.c_str());

        if (!isEditingName[entity]) startName[entity] = nameComp.name;
        if (ImGui::IsItemActivated()) { startName[entity] = nameComp.name; isEditingName[entity] = true; }

        if (ImGui::InputText("Name", buf, sizeof(buf)))
        {
            nameComp.name = buf;
            isEditingName[entity] = true;
        }

        if (isEditingName[entity] && !ImGui::IsItemActive()) {
            std::string oldVal = startName[entity];
            std::string newVal = nameComp.name;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<NameComponent>(entity)) {
                            ecs.GetComponent<NameComponent>(entity).name = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<NameComponent>(entity)) {
                            ecs.GetComponent<NameComponent>(entity).name = oldVal;
                        }
                    },
                    "Change Entity Name"
                );
            }
            isEditingName[entity] = false;
        }

        return true; // Skip default rendering (we rendered everything)
    });

    // ==================== TAG COMPONENT ====================
    // Tag component uses TagManager dropdown (rendered inline with Layer)
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("TagComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        TagComponent &tagComp = *static_cast<TagComponent *>(componentPtr);

        // Static tracking for entity-aware undo
        static std::unordered_map<Entity, int> startTagIndex;

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

        // Inline rendering (no label, just combo) with entity-aware undo
        ImGui::Text("Tag");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        int currentTag = tagComp.tagIndex;
        startTagIndex[entity] = currentTag;

        if (ImGui::BeginCombo("##Tag", tagItemPtrs[currentTag]))
        {
            for (int i = 0; i < static_cast<int>(tagItemPtrs.size()); i++)
            {
                bool isSelected = (currentTag == i);
                if (ImGui::Selectable(tagItemPtrs[i], isSelected))
                {
                    if (i >= 0 && i < static_cast<int>(availableTags.size()))
                    {
                        int oldVal = startTagIndex[entity];
                        int newVal = i;
                        tagComp.tagIndex = newVal;

                        if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                            UndoSystem::GetInstance().RecordLambdaChange(
                                [entity, newVal, &ecs]() {
                                    if (ecs.HasComponent<TagComponent>(entity)) {
                                        ecs.GetComponent<TagComponent>(entity).tagIndex = newVal;
                                    }
                                },
                                [entity, oldVal, &ecs]() {
                                    if (ecs.HasComponent<TagComponent>(entity)) {
                                        ecs.GetComponent<TagComponent>(entity).tagIndex = oldVal;
                                    }
                                },
                                "Change Entity Tag"
                            );
                        }
                    }
                    else if (i == static_cast<int>(availableTags.size()))
                    {
                        // "Add Tag..." was selected - open Tags & Layers window
                        auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
                        if (tagsLayersPanel) {
                            tagsLayersPanel->SetOpen(true);
                        }
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine(); // Keep Layer on same line

        return true; // Skip default rendering
    });

    // ==================== LAYER COMPONENT ====================
    // Layer component uses LayerManager dropdown (rendered inline with Tag)
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("LayerComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        LayerComponent &layerComp = *static_cast<LayerComponent *>(componentPtr);

        // Static tracking for entity-aware undo
        static std::unordered_map<Entity, int> startLayerIndex;

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

        // Store the start value for undo
        startLayerIndex[entity] = layerComp.layerIndex;

        // Inline rendering (continues from Tag on same line) with entity-aware undo
        ImGui::Text("Layer");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);

        if (ImGui::BeginCombo("##Layer", currentSelection >= 0 ? layerItemPtrs[currentSelection] : ""))
        {
            for (int i = 0; i < static_cast<int>(layerItemPtrs.size()); i++)
            {
                bool isSelected = (currentSelection == i);
                if (ImGui::Selectable(layerItemPtrs[i], isSelected))
                {
                    if (i >= 0 && i < static_cast<int>(tempIndices.size()))
                    {
                        int selectedIndex = tempIndices[i];
                        if (selectedIndex != -1)
                        {
                            int oldVal = startLayerIndex[entity];
                            int newVal = selectedIndex;
                            layerComp.layerIndex = newVal;

                            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                                UndoSystem::GetInstance().RecordLambdaChange(
                                    [entity, newVal, &ecs]() {
                                        if (ecs.HasComponent<LayerComponent>(entity)) {
                                            ecs.GetComponent<LayerComponent>(entity).layerIndex = newVal;
                                        }
                                    },
                                    [entity, oldVal, &ecs]() {
                                        if (ecs.HasComponent<LayerComponent>(entity)) {
                                            ecs.GetComponent<LayerComponent>(entity).layerIndex = oldVal;
                                        }
                                    },
                                    "Change Entity Layer"
                                );
                            }
                        }
                        else
                        {
                            // "Add Layer..." was selected - open Tags & Layers window
                            auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
                            if (tagsLayersPanel) {
                                tagsLayersPanel->SetOpen(true);
                            }
                        }
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator(); // Add separator after Tag/Layer line

        return true; // Skip default rendering
    });

    // ==================== TRANSFORM COMPONENT ====================
    // Transform needs special undo handling - must use TransformSystem, not direct memory writes
    // We track editing state per-entity and record commands that call TransformSystem on undo

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localPosition",
    [](const char *name, void *ptr, Entity entity, ECSManager &ecs)
    {
        (void)name;
        static std::unordered_map<Entity, Vector3D> startPositions;
        static std::unordered_map<Entity, bool> isEditingPosition;

        Vector3D *pos = static_cast<Vector3D *>(ptr);
        float arr[3] = {pos->x, pos->y, pos->z};
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Capture start value when not editing
        if (!isEditingPosition[entity]) {
            startPositions[entity] = *pos;
        }

        ImGui::Text("Position");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Use raw ImGui (not UndoableWidgets) - we handle undo ourselves
        bool changed = ImGui::DragFloat3("##Position", arr, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");

        // Track editing state
        if (ImGui::IsItemActivated()) {
            startPositions[entity] = *pos;
            isEditingPosition[entity] = true;
        }

        if (changed) {
            ecs.transformSystem->SetLocalPosition(entity, {arr[0], arr[1], arr[2]});
        }

        // Record undo command when editing ends
        if (isEditingPosition[entity] && !ImGui::IsItemActive()) {
            isEditingPosition[entity] = false;
            Vector3D startPos = startPositions[entity];
            Vector3D endPos = *pos;

            // Only record if actually changed
            if (startPos.x != endPos.x || startPos.y != endPos.y || startPos.z != endPos.z) {
                if (UndoSystem::GetInstance().IsEnabled()) {
                    // Capture entity and positions for lambda
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endPos]() {
                            // Redo: restore end position
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<Transform>(entity)) {
                                ecs.transformSystem->SetLocalPosition(entity, endPos);
                            }
                        },
                        [entity, startPos]() {
                            // Undo: restore start position
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<Transform>(entity)) {
                                ecs.transformSystem->SetLocalPosition(entity, startPos);
                            }
                        },
                        "Move Entity"
                    );
                }
            }
        }

        return changed;
    });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localRotation",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        static std::unordered_map<Entity, Vector3D> startRotations;
        static std::unordered_map<Entity, bool> isEditingRotation;

        Quaternion *quat = static_cast<Quaternion *>(ptr);
        Vector3D euler = quat->ToEulerDegrees();
        float arr[3] = {euler.x, euler.y, euler.z};
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Capture start value when not editing
        if (!isEditingRotation[entity]) {
            startRotations[entity] = euler;
        }

        ImGui::Text("Rotation");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Use raw ImGui - we handle undo ourselves
        bool changed = ImGui::DragFloat3("##Rotation", arr, 1.0f, -180.0f, 180.0f, "%.1f");

        // Track editing state
        if (ImGui::IsItemActivated()) {
            startRotations[entity] = euler;
            isEditingRotation[entity] = true;
        }

        if (changed) {
            ecs.transformSystem->SetLocalRotation(entity, {arr[0], arr[1], arr[2]});
        }

        // Record undo command when editing ends
        if (isEditingRotation[entity] && !ImGui::IsItemActive()) {
            isEditingRotation[entity] = false;
            Vector3D startRot = startRotations[entity];
            Vector3D endRot = {arr[0], arr[1], arr[2]};

            if (startRot.x != endRot.x || startRot.y != endRot.y || startRot.z != endRot.z) {
                if (UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endRot]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<Transform>(entity)) {
                                ecs.transformSystem->SetLocalRotation(entity, endRot);
                            }
                        },
                        [entity, startRot]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<Transform>(entity)) {
                                ecs.transformSystem->SetLocalRotation(entity, startRot);
                            }
                        },
                        "Rotate Entity"
                    );
                }
            }
        }

        return changed;
    });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localScale",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        static std::unordered_map<Entity, Vector3D> startScales;
        static std::unordered_map<Entity, bool> isEditingScale;

        Vector3D *scale = static_cast<Vector3D *>(ptr);
        float arr[3] = {scale->x, scale->y, scale->z};
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Capture start value when not editing
        if (!isEditingScale[entity]) {
            startScales[entity] = *scale;
        }

        ImGui::Text("Scale");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Use raw ImGui - we handle undo ourselves
        bool changed = ImGui::DragFloat3("##Scale", arr, 0.1f, 0.001f, FLT_MAX, "%.3f");

        // Track editing state
        if (ImGui::IsItemActivated()) {
            startScales[entity] = *scale;
            isEditingScale[entity] = true;
        }

        if (changed) {
            ecs.transformSystem->SetLocalScale(entity, {arr[0], arr[1], arr[2]});
        }

        // Record undo command when editing ends
        if (isEditingScale[entity] && !ImGui::IsItemActive()) {
            isEditingScale[entity] = false;
            Vector3D startScl = startScales[entity];
            Vector3D endScl = {arr[0], arr[1], arr[2]};

            if (startScl.x != endScl.x || startScl.y != endScl.y || startScl.z != endScl.z) {
                if (UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endScl]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<Transform>(entity)) {
                                ecs.transformSystem->SetLocalScale(entity, endScl);
                            }
                        },
                        [entity, startScl]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<Transform>(entity)) {
                                ecs.transformSystem->SetLocalScale(entity, startScl);
                            }
                        },
                        "Scale Entity"
                    );
                }
            }
        }

        return changed;
    });

    // ==================== COLLIDER COMPONENT ====================
    // ColliderComponent needs custom rendering for shape type and parameters

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "shapeTypeID",
    [](const char *, void *, Entity entity, ECSManager &ecs)
    {
        // Entity-aware undo for ColliderComponent - stores entity ID, not pointers
        static std::unordered_map<Entity, int> startShapeType;
        static std::unordered_map<Entity, Vector3D> startBoxHalfExtents;
        static std::unordered_map<Entity, float> startSphereRadius;
        static std::unordered_map<Entity, float> startCapsuleRadius;
        static std::unordered_map<Entity, float> startCapsuleHalfHeight;
        static std::unordered_map<Entity, float> startCylinderRadius;
        static std::unordered_map<Entity, float> startCylinderHalfHeight;
        static std::unordered_map<Entity, bool> isEditingCollider;

        auto &collider = ecs.GetComponent<ColliderComponent>(entity);
        auto rc = ecs.TryGetComponent<ModelRenderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Capture start values when not editing
        if (!isEditingCollider[entity]) {
            startShapeType[entity] = static_cast<int>(collider.shapeType);
            startBoxHalfExtents[entity] = collider.boxHalfExtents;
            startSphereRadius[entity] = collider.sphereRadius;
            startCapsuleRadius[entity] = collider.capsuleRadius;
            startCapsuleHalfHeight[entity] = collider.capsuleHalfHeight;
            startCylinderRadius[entity] = collider.cylinderRadius;
            startCylinderHalfHeight[entity] = collider.cylinderHalfHeight;
        }

        ImGui::Text("Shape Type");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char *shapeTypes[] = {"Box", "Sphere", "Capsule", "Cylinder", "MeshShape"};
        int currentShapeType = static_cast<int>(collider.shapeType);

        EditorComponents::PushComboColors();
        int oldShapeType = currentShapeType;
        bool shapeChanged = ImGui::Combo("##ShapeType", &currentShapeType, shapeTypes, 5);
        EditorComponents::PopComboColors();

        if (shapeChanged) {
            // Record undo for shape type change immediately (combo is instant)
            if (UndoSystem::GetInstance().IsEnabled()) {
                int capturedOld = oldShapeType;
                int capturedNew = currentShapeType;
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, capturedNew]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ColliderComponent>(entity)) {
                            auto& c = ecs.GetComponent<ColliderComponent>(entity);
                            c.shapeType = static_cast<ColliderShapeType>(capturedNew);
                            c.shapeTypeID = capturedNew;
                            c.version++;
                        }
                    },
                    [entity, capturedOld]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ColliderComponent>(entity)) {
                            auto& c = ecs.GetComponent<ColliderComponent>(entity);
                            c.shapeType = static_cast<ColliderShapeType>(capturedOld);
                            c.shapeTypeID = capturedOld;
                            c.version++;
                        }
                    },
                    "Change Collider Shape"
                );
            }
            collider.shapeType = static_cast<ColliderShapeType>(currentShapeType);
            collider.shapeTypeID = currentShapeType;
            collider.version++;
        }

        // Shape Parameters based on type
        bool shapeParamsChanged = false;

        Vector3D halfExtent = { 0.5f, 0.5f, 0.5f };
        float radius = 0.5f;
        if (rc.has_value()) {
            auto& modelComp = rc->get();
            halfExtent = modelComp.CalculateModelHalfExtent(*modelComp.model);
            radius = modelComp.CalculateModelRadius(*modelComp.model);
        }

        switch (collider.shapeType)
        {
        case ColliderShapeType::Box:
        {
            ImGui::Text("Half Extents");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            float halfExtents[3] = {collider.boxHalfExtents.x, collider.boxHalfExtents.y, collider.boxHalfExtents.z};

            if (ImGui::IsItemActivated()) isEditingCollider[entity] = true;

            if (ImGui::DragFloat3("##HalfExtents", halfExtents, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                collider.boxHalfExtents = Vector3D(halfExtents[0], halfExtents[1], halfExtents[2]);
                shapeParamsChanged = true;
                isEditingCollider[entity] = true;
            }

            // Record undo when editing ends
            if (isEditingCollider[entity] && !ImGui::IsItemActive() && !ImGui::IsAnyItemActive()) {
                Vector3D oldVal = startBoxHalfExtents[entity];
                Vector3D newVal = collider.boxHalfExtents;
                if (oldVal.x != newVal.x || oldVal.y != newVal.y || oldVal.z != newVal.z) {
                    if (UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal]() {
                                ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                                if (ecs.HasComponent<ColliderComponent>(entity)) {
                                    ecs.GetComponent<ColliderComponent>(entity).boxHalfExtents = newVal;
                                    ecs.GetComponent<ColliderComponent>(entity).version++;
                                }
                            },
                            [entity, oldVal]() {
                                ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                                if (ecs.HasComponent<ColliderComponent>(entity)) {
                                    ecs.GetComponent<ColliderComponent>(entity).boxHalfExtents = oldVal;
                                    ecs.GetComponent<ColliderComponent>(entity).version++;
                                }
                            },
                            "Edit Box Half Extents"
                        );
                    }
                }
                isEditingCollider[entity] = false;
            }
            break;
        }
        case ColliderShapeType::Sphere:
        {
            ImGui::Text("Radius");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            float oldRadius = collider.sphereRadius;

            if (ImGui::IsItemActivated()) {
                startSphereRadius[entity] = collider.sphereRadius;
                isEditingCollider[entity] = true;
            }

            if (ImGui::DragFloat("##SphereRadius", &collider.sphereRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                shapeParamsChanged = true;
                isEditingCollider[entity] = true;
            }

            if (isEditingCollider[entity] && !ImGui::IsItemActive()) {
                float startVal = startSphereRadius[entity];
                float endVal = collider.sphereRadius;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).sphereRadius = endVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        [entity, startVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).sphereRadius = startVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        "Edit Sphere Radius"
                    );
                }
                isEditingCollider[entity] = false;
            }
            break;
        }
        case ColliderShapeType::Capsule:
        {
            ImGui::Text("Radius");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            if (ImGui::IsItemActivated()) {
                startCapsuleRadius[entity] = collider.capsuleRadius;
                isEditingCollider[entity] = true;
            }

            if (ImGui::DragFloat("##CapsuleRadius", &collider.capsuleRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                shapeParamsChanged = true;
                isEditingCollider[entity] = true;
            }

            if (isEditingCollider[entity] && !ImGui::IsItemActive()) {
                float startVal = startCapsuleRadius[entity];
                float endVal = collider.capsuleRadius;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).capsuleRadius = endVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        [entity, startVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).capsuleRadius = startVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        "Edit Capsule Radius"
                    );
                }
                isEditingCollider[entity] = false;
            }

            ImGui::Text("Half Height");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            static std::unordered_map<Entity, bool> isEditingCapsuleHeight;
            if (ImGui::IsItemActivated()) {
                startCapsuleHalfHeight[entity] = collider.capsuleHalfHeight;
                isEditingCapsuleHeight[entity] = true;
            }

            if (ImGui::DragFloat("##CapsuleHalfHeight", &collider.capsuleHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                shapeParamsChanged = true;
                isEditingCapsuleHeight[entity] = true;
            }

            if (isEditingCapsuleHeight[entity] && !ImGui::IsItemActive()) {
                float startVal = startCapsuleHalfHeight[entity];
                float endVal = collider.capsuleHalfHeight;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).capsuleHalfHeight = endVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        [entity, startVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).capsuleHalfHeight = startVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        "Edit Capsule Half Height"
                    );
                }
                isEditingCapsuleHeight[entity] = false;
            }
            break;
        }
        case ColliderShapeType::Cylinder:
        {
            ImGui::Text("Radius");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            static std::unordered_map<Entity, bool> isEditingCylRadius;
            if (ImGui::IsItemActivated()) {
                startCylinderRadius[entity] = collider.cylinderRadius;
                isEditingCylRadius[entity] = true;
            }

            if (ImGui::DragFloat("##CylinderRadius", &collider.cylinderRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                shapeParamsChanged = true;
                isEditingCylRadius[entity] = true;
            }

            if (isEditingCylRadius[entity] && !ImGui::IsItemActive()) {
                float startVal = startCylinderRadius[entity];
                float endVal = collider.cylinderRadius;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).cylinderRadius = endVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        [entity, startVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).cylinderRadius = startVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        "Edit Cylinder Radius"
                    );
                }
                isEditingCylRadius[entity] = false;
            }

            ImGui::Text("Half Height");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            static std::unordered_map<Entity, bool> isEditingCylHeight;
            if (ImGui::IsItemActivated()) {
                startCylinderHalfHeight[entity] = collider.cylinderHalfHeight;
                isEditingCylHeight[entity] = true;
            }

            if (ImGui::DragFloat("##CylinderHalfHeight", &collider.cylinderHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
                shapeParamsChanged = true;
                isEditingCylHeight[entity] = true;
            }

            if (isEditingCylHeight[entity] && !ImGui::IsItemActive()) {
                float startVal = startCylinderHalfHeight[entity];
                float endVal = collider.cylinderHalfHeight;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).cylinderHalfHeight = endVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        [entity, startVal]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<ColliderComponent>(entity)) {
                                ecs.GetComponent<ColliderComponent>(entity).cylinderHalfHeight = startVal;
                                ecs.GetComponent<ColliderComponent>(entity).version++;
                            }
                        },
                        "Edit Cylinder Half Height"
                    );
                }
                isEditingCylHeight[entity] = false;
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

        if (shapeParamsChanged) {
            collider.version++;
        }

        return shapeChanged || shapeParamsChanged;
    });

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "layerID",
    [](const char *, void *, Entity entity, ECSManager &ecs)
    {
        auto &collider = ecs.GetComponent<ColliderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        ImGui::Text("Layer");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char *layers[] = {"Non-Moving", "Moving", "Sensor", "Debris"};
        int currentLayer = static_cast<int>(collider.layer);
        int oldLayer = currentLayer;

        EditorComponents::PushComboColors();
        bool changed = ImGui::Combo("##Layer", &currentLayer, layers, 4);
        EditorComponents::PopComboColors();

        if (changed) {
            // Record undo immediately for combo changes
            if (UndoSystem::GetInstance().IsEnabled()) {
                int capturedOld = oldLayer;
                int capturedNew = currentLayer;
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, capturedNew]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ColliderComponent>(entity)) {
                            auto& c = ecs.GetComponent<ColliderComponent>(entity);
                            c.layer = static_cast<JPH::ObjectLayer>(capturedNew);
                            c.layerID = capturedNew;
                            c.version++;
                        }
                    },
                    [entity, capturedOld]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ColliderComponent>(entity)) {
                            auto& c = ecs.GetComponent<ColliderComponent>(entity);
                            c.layer = static_cast<JPH::ObjectLayer>(capturedOld);
                            c.layerID = capturedOld;
                            c.version++;
                        }
                    },
                    "Change Collider Layer"
                );
            }
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

    // ColliderComponent center field - entity-aware undo
    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "center",
    [](const char*, void*, Entity entity, ECSManager& ecs)
    {
        static std::unordered_map<Entity, Vector3D> startCenter;
        static std::unordered_map<Entity, bool> isEditingCenter;

        auto& collider = ecs.GetComponent<ColliderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Initialize tracking
        if (isEditingCenter.find(entity) == isEditingCenter.end()) {
            isEditingCenter[entity] = false;
        }

        ImGui::Text("Center");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        float center[3] = { collider.center.x, collider.center.y, collider.center.z };

        // Track start value
        if (!isEditingCenter[entity]) {
            startCenter[entity] = collider.center;
        }
        if (ImGui::IsItemActivated()) {
            startCenter[entity] = collider.center;
            isEditingCenter[entity] = true;
        }

        bool changed = false;
        if (ImGui::DragFloat3("##Center", center, 0.1f)) {
            collider.center = Vector3D(center[0], center[1], center[2]);
            collider.version++;
            isEditingCenter[entity] = true;
            changed = true;
        }

        // Record undo when editing ends
        if (isEditingCenter[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startCenter[entity];
            Vector3D newVal = collider.center;
            if ((oldVal.x != newVal.x || oldVal.y != newVal.y || oldVal.z != newVal.z) &&
                UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ColliderComponent>(entity)) {
                            ecs.GetComponent<ColliderComponent>(entity).center = newVal;
                            ecs.GetComponent<ColliderComponent>(entity).version++;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ColliderComponent>(entity)) {
                            ecs.GetComponent<ColliderComponent>(entity).center = oldVal;
                            ecs.GetComponent<ColliderComponent>(entity).version++;
                        }
                    },
                    "Edit Collider Center"
                );
            }
            isEditingCenter[entity] = false;
        }

        return changed;
    });

    // ==================== RIGIDBODY COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("RigidBodyComponent",
    [](void *, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        auto &rigidBody = ecs.GetComponent<RigidBodyComponent>(entity);
        auto &transform = ecs.GetComponent<Transform>(entity); // for info tab

        ImGui::PushID("RigidBodyComponent");
        const float labelWidth = EditorComponents::GetLabelWidth();

        // --- Motion Type dropdown (entity-aware undo) ---
        ImGui::Text("Motion");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char *motionTypes[] = {"Static", "Kinematic", "Dynamic"};
        int currentMotion = rigidBody.motionID;
        int oldMotion = currentMotion;
        EditorComponents::PushComboColors();
        if (ImGui::Combo("##MotionType", &currentMotion, motionTypes, IM_ARRAYSIZE(motionTypes)))
        {
            // Record undo for motion type change
            if (UndoSystem::GetInstance().IsEnabled()) {
                int capturedOld = oldMotion;
                int capturedNew = currentMotion;
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, capturedNew]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<RigidBodyComponent>(entity)) {
                            auto& rb = ecs.GetComponent<RigidBodyComponent>(entity);
                            rb.motion = static_cast<Motion>(capturedNew);
                            rb.motionID = capturedNew;
                            rb.motion_dirty = true;
                        }
                    },
                    [entity, capturedOld]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<RigidBodyComponent>(entity)) {
                            auto& rb = ecs.GetComponent<RigidBodyComponent>(entity);
                            rb.motion = static_cast<Motion>(capturedOld);
                            rb.motionID = capturedOld;
                            rb.motion_dirty = true;
                        }
                    },
                    "Change Motion Type"
                );
            }
            rigidBody.motion = static_cast<Motion>(currentMotion);
            rigidBody.motionID = currentMotion;
            rigidBody.motion_dirty = true;
        }
        EditorComponents::PopComboColors();

        // --- Is Trigger checkbox (entity-aware undo) ---
        ImGui::AlignTextToFramePadding();
        bool oldTrigger = rigidBody.isTrigger;
        if (ImGui::Checkbox("##IsTrigger", &rigidBody.isTrigger)) {
            if (UndoSystem::GetInstance().IsEnabled()) {
                bool capturedOld = oldTrigger;
                bool capturedNew = rigidBody.isTrigger;
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, capturedNew]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<RigidBodyComponent>(entity)) {
                            ecs.GetComponent<RigidBodyComponent>(entity).isTrigger = capturedNew;
                        }
                    },
                    [entity, capturedOld]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<RigidBodyComponent>(entity)) {
                            ecs.GetComponent<RigidBodyComponent>(entity).isTrigger = capturedOld;
                        }
                    },
                    "Toggle Is Trigger"
                );
            }
        }
        ImGui::SameLine();
        ImGui::Text("Is Trigger");

        if (rigidBody.motion == Motion::Dynamic)
        {
            // --- CCD checkbox (entity-aware undo) ---
            ImGui::AlignTextToFramePadding();
            bool oldCCD = rigidBody.ccd;
            if (ImGui::Checkbox("##CCD", &rigidBody.ccd))
            {
                rigidBody.motion_dirty = true;
                if (UndoSystem::GetInstance().IsEnabled()) {
                    bool capturedOld = oldCCD;
                    bool capturedNew = rigidBody.ccd;
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, capturedNew]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<RigidBodyComponent>(entity)) {
                                auto& rb = ecs.GetComponent<RigidBodyComponent>(entity);
                                rb.ccd = capturedNew;
                                rb.motion_dirty = true;
                            }
                        },
                        [entity, capturedOld]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<RigidBodyComponent>(entity)) {
                                auto& rb = ecs.GetComponent<RigidBodyComponent>(entity);
                                rb.ccd = capturedOld;
                                rb.motion_dirty = true;
                            }
                        },
                        "Toggle CCD"
                    );
                }
            }
            ImGui::SameLine();
            ImGui::Text("CCD");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Continuous Collision Detection - prevents fast-moving objects from tunneling");

            // --- Linear Damping (entity-aware undo) ---
            static std::unordered_map<Entity, float> startLinearDamping;
            static std::unordered_map<Entity, bool> isEditingLinearDamping;

            ImGui::Text("Linear Damping");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            if (!isEditingLinearDamping[entity]) startLinearDamping[entity] = rigidBody.linearDamping;
            if (ImGui::IsItemActivated()) { startLinearDamping[entity] = rigidBody.linearDamping; isEditingLinearDamping[entity] = true; }

            ImGui::DragFloat("##LinearDamping", &rigidBody.linearDamping, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            if (isEditingLinearDamping[entity] && !ImGui::IsItemActive()) {
                float startVal = startLinearDamping[entity];
                float endVal = rigidBody.linearDamping;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<RigidBodyComponent>(entity)) ecs.GetComponent<RigidBodyComponent>(entity).linearDamping = endVal; },
                        [entity, startVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<RigidBodyComponent>(entity)) ecs.GetComponent<RigidBodyComponent>(entity).linearDamping = startVal; },
                        "Edit Linear Damping"
                    );
                }
                isEditingLinearDamping[entity] = false;
            }

            // --- Angular Damping (entity-aware undo) ---
            static std::unordered_map<Entity, float> startAngularDamping;
            static std::unordered_map<Entity, bool> isEditingAngularDamping;

            ImGui::Text("Angular Damping");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            if (!isEditingAngularDamping[entity]) startAngularDamping[entity] = rigidBody.angularDamping;
            if (ImGui::IsItemActivated()) { startAngularDamping[entity] = rigidBody.angularDamping; isEditingAngularDamping[entity] = true; }

            ImGui::DragFloat("##AngularDamping", &rigidBody.angularDamping, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            if (isEditingAngularDamping[entity] && !ImGui::IsItemActive()) {
                float startVal = startAngularDamping[entity];
                float endVal = rigidBody.angularDamping;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<RigidBodyComponent>(entity)) ecs.GetComponent<RigidBodyComponent>(entity).angularDamping = endVal; },
                        [entity, startVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<RigidBodyComponent>(entity)) ecs.GetComponent<RigidBodyComponent>(entity).angularDamping = startVal; },
                        "Edit Angular Damping"
                    );
                }
                isEditingAngularDamping[entity] = false;
            }

            // --- Gravity Factor (entity-aware undo) ---
            static std::unordered_map<Entity, float> startGravityFactor;
            static std::unordered_map<Entity, bool> isEditingGravityFactor;

            ImGui::Text("Gravity Factor");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            if (!isEditingGravityFactor[entity]) startGravityFactor[entity] = rigidBody.gravityFactor;
            if (ImGui::IsItemActivated()) { startGravityFactor[entity] = rigidBody.gravityFactor; isEditingGravityFactor[entity] = true; }

            ImGui::DragFloat("##GravityFactor", &rigidBody.gravityFactor, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");

            if (isEditingGravityFactor[entity] && !ImGui::IsItemActive()) {
                float startVal = startGravityFactor[entity];
                float endVal = rigidBody.gravityFactor;
                if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, endVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<RigidBodyComponent>(entity)) ecs.GetComponent<RigidBodyComponent>(entity).gravityFactor = endVal; },
                        [entity, startVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<RigidBodyComponent>(entity)) ecs.GetComponent<RigidBodyComponent>(entity).gravityFactor = startVal; },
                        "Edit Gravity Factor"
                    );
                }
                isEditingGravityFactor[entity] = false;
            }
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

    //ReflectionRenderer::RegisterComponentRenderer("VideoComponent",
    //    [](void* ptr, TypeDescriptor_Struct* type, Entity entity, ECSManager& ecs)
    //    {
    //        // 1. Cast to the actual component type
    //        auto& videoComp = *static_cast<VideoComponent*>(ptr);
    //        const float labelWidth = EditorComponents::GetLabelWidth();

    //        ImGui::Text("Configuration File");
    //        ImGui::SameLine(labelWidth);
    //        ImGui::SetNextItemWidth(-1);

    //        // 2. Use the path stored in the component to show the display text
    //        std::string texPath = videoComp.videoPath;
    //        videoComp.inputFilePath = texPath;
    //        std::string displayText = texPath.empty() ? "None (Text)" : texPath.substr(texPath.find_last_of("/\\") + 1);

    //        float buttonWidth = ImGui::GetContentRegionAvail().x;
    //        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

    //        if (EditorComponents::BeginDragDropTarget())
    //        {
    //            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXT_PAYLOAD"))
    //            {
    //                SnapshotManager::GetInstance().TakeSnapshot("Assign Cutscene File");

    //                const char* payloadPath = (const char*)payload->Data;
    //                std::string pathStr(payloadPath, payload->DataSize);
    //                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

    //                // 3. Update the component directly

    //                videoComp.ProcessMetaData(pathStr);     //split path accordingly.
    //                videoComp.asset_dirty = true; // Mark for reload      

    //                EditorComponents::EndDragDropTarget();
    //                return true;
    //            }
    //            EditorComponents::EndDragDropTarget();
    //        }

    //        ImGui::Spacing(); // Add some space between the two inputs

    //        // --- 2. DIALOGUE FILE (New) ---
    //        ImGui::Text("Dialogue File");
    //        ImGui::SameLine(labelWidth);
    //        ImGui::SetNextItemWidth(-1);

    //        // Assuming you add 'dialoguePath' to your VideoComponent
    //        std::string diagPath = videoComp.dialoguePath;
    //        std::string diagDisplay = diagPath.empty() ? "None (Dialogue)" : diagPath.substr(diagPath.find_last_of("/\\") + 1);

    //        EditorComponents::DrawDragDropButton(diagDisplay.c_str(), buttonWidth);

    //        if (EditorComponents::BeginDragDropTarget())
    //        {
    //            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXT_PAYLOAD"))
    //            {
    //                SnapshotManager::GetInstance().TakeSnapshot("Assign Dialogue File");
    //                const char* payloadPath = (const char*)payload->Data;
    //                std::string pathStr(payloadPath, payload->DataSize);
    //                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

    //                // Store the path and trigger the parser you wrote in the DialogueManager
    //                videoComp.dialoguePath = pathStr;   
    //                videoComp.ProcessDialogueData(pathStr);

    //                EditorComponents::EndDragDropTarget();
    //                return true;
    //            }
    //            EditorComponents::EndDragDropTarget();
    //        }

    //        return false;
    //    });


    ReflectionRenderer::RegisterFieldRenderer("VideoComponent", "videoPath",
        [](const char*, void* ptr, Entity entity, ECSManager& ecs)
        {
            std::string* pathPtr = static_cast<std::string*>(ptr);
            const float labelWidth = EditorComponents::GetLabelWidth();

            ImGui::Text("Configuration File");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            std::string displayText = pathPtr->empty() ? "None (Text)" : pathPtr->substr(pathPtr->find_last_of("/\\") + 1);
            float buttonWidth = ImGui::GetContentRegionAvail().x;
            EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

            if (EditorComponents::BeginDragDropTarget())
            {
                ImGui::SetTooltip("Drop configuration file here");
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXT_PAYLOAD"))
                {
                    // Take snapshot before changing file
                    SnapshotManager::GetInstance().TakeSnapshot("Assign Configuration File");

                    const char* filePath = (const char*)payload->Data;
                    std::string pathStr(filePath, payload->DataSize);
                    pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                    ENGINE_PRINT("Configuration PathStr is ", pathStr);

                    // Update the videoPath directly
                    *pathPtr = pathStr;

                    // Get the component and process the metadata
                    auto& videoComp = ecs.GetComponent<VideoComponent>(entity);
                    videoComp.videoPath = pathStr;
                    videoComp.ProcessMetaData(pathStr);
                    videoComp.asset_dirty = true;

                    EditorComponents::EndDragDropTarget();
                    return true; // Field was modified
                }
                EditorComponents::EndDragDropTarget();
            }

            return false;
        });




    ReflectionRenderer::RegisterFieldRenderer("VideoComponent", "dialoguePath",
        [](const char*, void* ptr, Entity entity, ECSManager& ecs)
        {
            std::string* pathPtr = static_cast<std::string*>(ptr);
            const float labelWidth = EditorComponents::GetLabelWidth();

            ImGui::Text("Dialogue File");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);

            std::string displayText = pathPtr->empty() ? "None (Dialogue)" : pathPtr->substr(pathPtr->find_last_of("/\\") + 1);
            float buttonWidth = ImGui::GetContentRegionAvail().x;
            EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

            if (EditorComponents::BeginDragDropTarget())
            {
                ImGui::SetTooltip("Drop dialogue file here");
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXT_PAYLOAD"))
                {
                    // Take snapshot before changing file
                    SnapshotManager::GetInstance().TakeSnapshot("Assign Dialogue File");

                    const char* filePath = (const char*)payload->Data;
                    std::string pathStr(filePath, payload->DataSize);
                    pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                    ENGINE_PRINT("Dialogue PathStr is ", pathStr);

                    // Update the dialoguePath directly
                    *pathPtr = pathStr;

                    // Get the component and process the dialogue data
                    auto& videoComp = ecs.GetComponent<VideoComponent>(entity);
                    videoComp.dialoguePath = pathStr;
                    videoComp.ProcessDialogueData(pathStr);

                    EditorComponents::EndDragDropTarget();
                    return true; // Field was modified
                }
                EditorComponents::EndDragDropTarget();
            }

            return false;
        });











    // ==================== CAMERA COMPONENT ====================
    // Camera needs special handling for enum and glm::vec3 properties
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("CameraComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        CameraComponent &camera = *static_cast<CameraComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, int> startProjectionType;
        static std::unordered_map<Entity, bool> isEditingProjection;
        static std::unordered_map<Entity, glm::vec3> startTarget;
        static std::unordered_map<Entity, bool> isEditingTarget;
        static std::unordered_map<Entity, glm::vec3> startUp;
        static std::unordered_map<Entity, bool> isEditingUp;
        static std::unordered_map<Entity, int> startClearFlags;
        static std::unordered_map<Entity, bool> isEditingClearFlags;
        static std::unordered_map<Entity, glm::vec3> startBgColor;
        static std::unordered_map<Entity, bool> isEditingBgColor;

        // Initialize tracking state
        if (isEditingProjection.find(entity) == isEditingProjection.end()) isEditingProjection[entity] = false;
        if (isEditingTarget.find(entity) == isEditingTarget.end()) isEditingTarget[entity] = false;
        if (isEditingUp.find(entity) == isEditingUp.end()) isEditingUp[entity] = false;
        if (isEditingClearFlags.find(entity) == isEditingClearFlags.end()) isEditingClearFlags[entity] = false;
        if (isEditingBgColor.find(entity) == isEditingBgColor.end()) isEditingBgColor[entity] = false;

        // Projection Type dropdown
        ImGui::Text("Projection");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char *projTypes[] = {"Perspective", "Orthographic"};
        int currentProj = static_cast<int>(camera.projectionType);

        if (!isEditingProjection[entity]) startProjectionType[entity] = currentProj;

        EditorComponents::PushComboColors();
        if (ImGui::BeginCombo("##Projection", projTypes[currentProj]))
        {
            if (!isEditingProjection[entity]) {
                startProjectionType[entity] = currentProj;
                isEditingProjection[entity] = true;
            }
            for (int i = 0; i < 2; i++)
            {
                bool isSelected = (currentProj == i);
                if (ImGui::Selectable(projTypes[i], isSelected))
                {
                    int oldVal = startProjectionType[entity];
                    int newVal = i;
                    camera.projectionType = static_cast<ProjectionType>(newVal);

                    if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal, &ecs]() {
                                if (ecs.HasComponent<CameraComponent>(entity)) {
                                    ecs.GetComponent<CameraComponent>(entity).projectionType = static_cast<ProjectionType>(newVal);
                                }
                            },
                            [entity, oldVal, &ecs]() {
                                if (ecs.HasComponent<CameraComponent>(entity)) {
                                    ecs.GetComponent<CameraComponent>(entity).projectionType = static_cast<ProjectionType>(oldVal);
                                }
                            },
                            "Change Camera Projection"
                        );
                    }
                    isEditingProjection[entity] = false;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        else {
            isEditingProjection[entity] = false;
        }
        EditorComponents::PopComboColors();

        // Target (glm::vec3)
        ImGui::Text("Target");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float target[3] = {camera.target.x, camera.target.y, camera.target.z};

        if (!isEditingTarget[entity]) startTarget[entity] = camera.target;
        if (ImGui::IsItemActivated()) { startTarget[entity] = camera.target; isEditingTarget[entity] = true; }

        if (ImGui::DragFloat3("##Target", target, 0.1f))
        {
            camera.target = glm::vec3(target[0], target[1], target[2]);
            isEditingTarget[entity] = true;
        }

        if (isEditingTarget[entity] && !ImGui::IsItemActive()) {
            glm::vec3 oldVal = startTarget[entity];
            glm::vec3 newVal = camera.target;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<CameraComponent>(entity)) {
                            ecs.GetComponent<CameraComponent>(entity).target = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<CameraComponent>(entity)) {
                            ecs.GetComponent<CameraComponent>(entity).target = oldVal;
                        }
                    },
                    "Change Camera Target"
                );
            }
            isEditingTarget[entity] = false;
        }

        // Up (glm::vec3)
        ImGui::Text("Up");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float up[3] = {camera.up.x, camera.up.y, camera.up.z};

        if (!isEditingUp[entity]) startUp[entity] = camera.up;
        if (ImGui::IsItemActivated()) { startUp[entity] = camera.up; isEditingUp[entity] = true; }

        if (ImGui::DragFloat3("##Up", up, 0.1f))
        {
            camera.up = glm::vec3(up[0], up[1], up[2]);
            isEditingUp[entity] = true;
        }

        if (isEditingUp[entity] && !ImGui::IsItemActive()) {
            glm::vec3 oldVal = startUp[entity];
            glm::vec3 newVal = camera.up;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<CameraComponent>(entity)) {
                            ecs.GetComponent<CameraComponent>(entity).up = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<CameraComponent>(entity)) {
                            ecs.GetComponent<CameraComponent>(entity).up = oldVal;
                        }
                    },
                    "Change Camera Up"
                );
            }
            isEditingUp[entity] = false;
        }

        // Clear Flags dropdown
        ImGui::Text("Clear Flags");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        const char* clearFlagsOptions[] = {"Skybox", "Solid Color", "Depth Only", "Don't Clear"};
        int currentClearFlags = static_cast<int>(camera.clearFlags);

        if (!isEditingClearFlags[entity]) startClearFlags[entity] = currentClearFlags;

        EditorComponents::PushComboColors();
        if (ImGui::BeginCombo("##ClearFlags", clearFlagsOptions[currentClearFlags]))
        {
            if (!isEditingClearFlags[entity]) {
                startClearFlags[entity] = currentClearFlags;
                isEditingClearFlags[entity] = true;
            }
            for (int i = 0; i < 4; i++)
            {
                bool isSelected = (currentClearFlags == i);
                if (ImGui::Selectable(clearFlagsOptions[i], isSelected))
                {
                    int oldVal = startClearFlags[entity];
                    int newVal = i;
                    camera.clearFlags = static_cast<CameraClearFlags>(newVal);

                    if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal, &ecs]() {
                                if (ecs.HasComponent<CameraComponent>(entity)) {
                                    ecs.GetComponent<CameraComponent>(entity).clearFlags = static_cast<CameraClearFlags>(newVal);
                                }
                            },
                            [entity, oldVal, &ecs]() {
                                if (ecs.HasComponent<CameraComponent>(entity)) {
                                    ecs.GetComponent<CameraComponent>(entity).clearFlags = static_cast<CameraClearFlags>(oldVal);
                                }
                            },
                            "Change Camera Clear Flags"
                        );
                    }
                    isEditingClearFlags[entity] = false;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        else {
            isEditingClearFlags[entity] = false;
        }
        EditorComponents::PopComboColors();

        // Background Color
        ImGui::Text("Background");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float bgColor[3] = {camera.backgroundColor.r, camera.backgroundColor.g, camera.backgroundColor.b};

        if (!isEditingBgColor[entity]) startBgColor[entity] = camera.backgroundColor;
        if (ImGui::IsItemActivated()) { startBgColor[entity] = camera.backgroundColor; isEditingBgColor[entity] = true; }

        if (ImGui::ColorEdit3("##Background", bgColor))
        {
            camera.backgroundColor = glm::vec3(bgColor[0], bgColor[1], bgColor[2]);
            isEditingBgColor[entity] = true;
        }

        if (isEditingBgColor[entity] && !ImGui::IsItemActive()) {
            glm::vec3 oldVal = startBgColor[entity];
            glm::vec3 newVal = camera.backgroundColor;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<CameraComponent>(entity)) {
                            ecs.GetComponent<CameraComponent>(entity).backgroundColor = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<CameraComponent>(entity)) {
                            ecs.GetComponent<CameraComponent>(entity).backgroundColor = oldVal;
                        }
                    },
                    "Change Camera Background"
                );
            }
            isEditingBgColor[entity] = false;
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

                ENGINE_PRINT("[Inspector] Applying model - GUID: {", DraggedModelGuid.high, ", ", DraggedModelGuid.low, "}, Path: ", DraggedModelPath);

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
                        ENGINE_PRINT("[Inspector] Model loaded successfully!");
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
                ENGINE_PRINT("PathStr is ", pathStr);
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
    // Uses entity-aware lambda commands for proper undo/redo
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "color",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        Vector3D *color = static_cast<Vector3D *>(ptr);
        auto &sprite = ecs.GetComponent<SpriteRenderComponent>(entity);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking for entity-aware undo
        static std::unordered_map<Entity, Vector3D> startColor;
        static std::unordered_map<Entity, float> startAlpha;
        static std::unordered_map<Entity, bool> isEditingColor;

        if (isEditingColor.find(entity) == isEditingColor.end()) isEditingColor[entity] = false;

        // Convert to 0-255 range for display, combine with alpha
        float colorRGBA[4] = {
            color->x,
            color->y,
            color->z,
            sprite.alpha};

        ImGui::Text("Color:");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        if (!isEditingColor[entity]) {
            startColor[entity] = *color;
            startAlpha[entity] = sprite.alpha;
        }
        if (ImGui::IsItemActivated()) {
            startColor[entity] = *color;
            startAlpha[entity] = sprite.alpha;
            isEditingColor[entity] = true;
        }

        if (ImGui::ColorEdit4("##Color", colorRGBA, ImGuiColorEditFlags_Uint8))
        {
            color->x = colorRGBA[0];
            color->y = colorRGBA[1];
            color->z = colorRGBA[2];
            sprite.alpha = colorRGBA[3];
            isEditingColor[entity] = true;
        }

        if (isEditingColor[entity] && !ImGui::IsItemActive()) {
            Vector3D oldColor = startColor[entity];
            float oldAlpha = startAlpha[entity];
            Vector3D newColor = *color;
            float newAlpha = sprite.alpha;

            bool colorChanged = (oldColor.x != newColor.x || oldColor.y != newColor.y ||
                                oldColor.z != newColor.z || oldAlpha != newAlpha);
            if (colorChanged && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newColor, newAlpha]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpriteRenderComponent>(entity)) {
                            auto& s = ecs.GetComponent<SpriteRenderComponent>(entity);
                            s.color = newColor;
                            s.alpha = newAlpha;
                        }
                    },
                    [entity, oldColor, oldAlpha]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpriteRenderComponent>(entity)) {
                            auto& s = ecs.GetComponent<SpriteRenderComponent>(entity);
                            s.color = oldColor;
                            s.alpha = oldAlpha;
                        }
                    },
                    "Change Sprite Color"
                );
            }
            isEditingColor[entity] = false;
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

    // Skip ParticleComponent fields that are handled in the component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "emitterPosition",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Not editable, set by transform
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "emissionRate",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "maxParticles",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "particleLifetime",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "startSize",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "endSize",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "startColor",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "startColorAlpha",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "endColor",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "endColorAlpha",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "gravity",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "velocityRandomness",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "initialVelocity",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "isEmitting",
    [](const char*, void*, Entity, ECSManager&) { return false; }); // Handled in component renderer

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
    // Uses entity-aware lambda commands for proper undo/redo
    ReflectionRenderer::RegisterComponentRenderer("AudioComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        AudioComponent &audio = *static_cast<AudioComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, int> startMixerGroup;
        static std::unordered_map<Entity, bool> startMute;
        static std::unordered_map<Entity, bool> startBypassListenerEffects;
        static std::unordered_map<Entity, bool> startPlayOnAwake;
        static std::unordered_map<Entity, bool> startLoop;
        static std::unordered_map<Entity, bool> startSpatialize;
        static std::unordered_map<Entity, float> startMinDistance;
        static std::unordered_map<Entity, bool> isEditingMinDistance;
        static std::unordered_map<Entity, float> startMaxDistance;
        static std::unordered_map<Entity, bool> isEditingMaxDistance;

        // Initialize tracking
        if (isEditingMinDistance.find(entity) == isEditingMinDistance.end()) isEditingMinDistance[entity] = false;
        if (isEditingMaxDistance.find(entity) == isEditingMaxDistance.end()) isEditingMaxDistance[entity] = false;

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
                GUID_128 oldGUID = audio.audioGUID;
                GUID_128 newGUID = DraggedAudioGuid;
                audio.SetClip(newGUID);
                if (UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newGUID]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<AudioComponent>(entity)) {
                                ecs.GetComponent<AudioComponent>(entity).SetClip(newGUID);
                            }
                        },
                        [entity, oldGUID]() {
                            ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                            if (ecs.HasComponent<AudioComponent>(entity)) {
                                ecs.GetComponent<AudioComponent>(entity).SetClip(oldGUID);
                            }
                        },
                        "Assign Audio Clip"
                    );
                }
                ImGui::EndDragDropTarget();
                return true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();

        // Output section - Mixer Group dropdown
        ImGui::Text("Output");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        // Define available mixer groups
        const char* mixerGroups[] = { "Default", "BGM", "SFX" };
        int currentMixerIndex = 0;

        // Find current selection
        if (audio.OutputAudioMixerGroup == "BGM") {
            currentMixerIndex = 1;
        } else if (audio.OutputAudioMixerGroup == "SFX") {
            currentMixerIndex = 2;
        } else {
            currentMixerIndex = 0; // Default or empty
        }

        startMixerGroup[entity] = currentMixerIndex;
        EditorComponents::PushComboColors();
        if (ImGui::BeginCombo("##OutputMixerGroup", mixerGroups[currentMixerIndex])) {
            for (int i = 0; i < 3; i++) {
                bool isSelected = (currentMixerIndex == i);
                if (ImGui::Selectable(mixerGroups[i], isSelected)) {
                    int oldVal = startMixerGroup[entity];
                    int newVal = i;
                    if (newVal == 1) {
                        audio.SetOutputAudioMixerGroup("BGM");
                    } else if (newVal == 2) {
                        audio.SetOutputAudioMixerGroup("SFX");
                    } else {
                        audio.SetOutputAudioMixerGroup("");
                    }
                    if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal]() {
                                ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                                if (ecs.HasComponent<AudioComponent>(entity)) {
                                    auto& a = ecs.GetComponent<AudioComponent>(entity);
                                    if (newVal == 1) a.SetOutputAudioMixerGroup("BGM");
                                    else if (newVal == 2) a.SetOutputAudioMixerGroup("SFX");
                                    else a.SetOutputAudioMixerGroup("");
                                }
                            },
                            [entity, oldVal]() {
                                ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                                if (ecs.HasComponent<AudioComponent>(entity)) {
                                    auto& a = ecs.GetComponent<AudioComponent>(entity);
                                    if (oldVal == 1) a.SetOutputAudioMixerGroup("BGM");
                                    else if (oldVal == 2) a.SetOutputAudioMixerGroup("SFX");
                                    else a.SetOutputAudioMixerGroup("");
                                }
                            },
                            "Change Audio Mixer Group"
                        );
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        EditorComponents::PopComboColors();

        // Mute checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Mute");
        ImGui::SameLine(labelWidth);
        startMute[entity] = audio.Mute;
        bool muteVal = audio.Mute;
        if (ImGui::Checkbox("##Mute", &muteVal)) {
            bool oldVal = startMute[entity];
            bool newVal = muteVal;
            audio.Mute = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).Mute = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).Mute = oldVal;
                        }
                    },
                    "Toggle Audio Mute"
                );
            }
        }

        // Bypass Listener Effects checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Bypass Listener Effects");
        ImGui::SameLine(labelWidth);
        startBypassListenerEffects[entity] = audio.bypassListenerEffects;
        bool bypassVal = audio.bypassListenerEffects;
        if (ImGui::Checkbox("##BypassListenerEffects", &bypassVal)) {
            bool oldVal = startBypassListenerEffects[entity];
            bool newVal = bypassVal;
            audio.bypassListenerEffects = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).bypassListenerEffects = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).bypassListenerEffects = oldVal;
                        }
                    },
                    "Toggle Audio Bypass Listener"
                );
            }
        }

        // Play On Awake checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Play On Awake");
        ImGui::SameLine(labelWidth);
        startPlayOnAwake[entity] = audio.PlayOnAwake;
        bool playOnAwakeVal = audio.PlayOnAwake;
        if (ImGui::Checkbox("##PlayOnAwake", &playOnAwakeVal)) {
            bool oldVal = startPlayOnAwake[entity];
            bool newVal = playOnAwakeVal;
            audio.PlayOnAwake = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).PlayOnAwake = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).PlayOnAwake = oldVal;
                        }
                    },
                    "Toggle Audio Play On Awake"
                );
            }
        }

        // Loop checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Loop");
        ImGui::SameLine(labelWidth);
        startLoop[entity] = audio.Loop;
        bool loopVal = audio.Loop;
        if (ImGui::Checkbox("##Loop", &loopVal)) {
            bool oldVal = startLoop[entity];
            bool newVal = loopVal;
            audio.Loop = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).Loop = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<AudioComponent>(entity)) {
                            ecs.GetComponent<AudioComponent>(entity).Loop = oldVal;
                        }
                    },
                    "Toggle Audio Loop"
                );
            }
        }

        ImGui::Separator();

        // Priority, Volume, Pitch, etc. use EditorComponents::DrawSliderWithInput
        // which doesn't have entity-aware undo yet - these will use reflection renderer defaults
        EditorComponents::DrawSliderWithInput("Priority", &audio.Priority, 0, 256, true, labelWidth);
        EditorComponents::DrawSliderWithInput("Volume", &audio.Volume, 0.0f, 1.0f, false, labelWidth);
        EditorComponents::DrawSliderWithInput("Pitch", &audio.Pitch, 0.1f, 3.0f, false, labelWidth);
        EditorComponents::DrawSliderWithInput("Stereo Pan", &audio.StereoPan, -1.0f, 1.0f, false, labelWidth);
        EditorComponents::DrawSliderWithInput("Reverb Zone Mix", &audio.reverbZoneMix, 0.0f, 1.0f, false, labelWidth);

        // 3D Sound Settings (collapsible)
        if (ImGui::CollapsingHeader("3D Sound Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            // Spatialize checkbox
            ImGui::Text("Spatialize");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            startSpatialize[entity] = audio.Spatialize;
            bool spatializeVal = audio.Spatialize;
            if (ImGui::Checkbox("##Spatialize", &spatializeVal)) {
                bool oldVal = startSpatialize[entity];
                bool newVal = spatializeVal;
                audio.Spatialize = newVal;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<AudioComponent>(entity)) {
                                ecs.GetComponent<AudioComponent>(entity).Spatialize = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<AudioComponent>(entity)) {
                                ecs.GetComponent<AudioComponent>(entity).Spatialize = oldVal;
                            }
                        },
                        "Toggle Audio Spatialize"
                    );
                }
            }

            if (audio.Spatialize)
            {
                // Spatial Blend
                if (EditorComponents::DrawSliderWithInput("Spatial Blend", &audio.SpatialBlend, 0.0f, 1.0f, false, labelWidth))
                {
                    audio.SetSpatialBlend(audio.SpatialBlend);
                }

                // Doppler Level
                EditorComponents::DrawSliderWithInput("Doppler Level", &audio.DopplerLevel, 0.0f, 5.0f, false, labelWidth);

                // Min Distance
                ImGui::Text("Min Distance");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);
                if (!isEditingMinDistance[entity]) startMinDistance[entity] = audio.MinDistance;
                if (ImGui::IsItemActivated()) { startMinDistance[entity] = audio.MinDistance; isEditingMinDistance[entity] = true; }
                if (ImGui::DragFloat("##MinDistance", &audio.MinDistance, 0.1f, 0.0f, audio.MaxDistance, "%.2f")) {
                    isEditingMinDistance[entity] = true;
                }
                if (isEditingMinDistance[entity] && !ImGui::IsItemActive()) {
                    float oldVal = startMinDistance[entity];
                    float newVal = audio.MinDistance;
                    if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal, &ecs]() {
                                if (ecs.HasComponent<AudioComponent>(entity)) {
                                    ecs.GetComponent<AudioComponent>(entity).MinDistance = newVal;
                                }
                            },
                            [entity, oldVal, &ecs]() {
                                if (ecs.HasComponent<AudioComponent>(entity)) {
                                    ecs.GetComponent<AudioComponent>(entity).MinDistance = oldVal;
                                }
                            },
                            "Change Audio Min Distance"
                        );
                    }
                    isEditingMinDistance[entity] = false;
                }

                // Max Distance
                ImGui::Text("Max Distance");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);
                if (!isEditingMaxDistance[entity]) startMaxDistance[entity] = audio.MaxDistance;
                if (ImGui::IsItemActivated()) { startMaxDistance[entity] = audio.MaxDistance; isEditingMaxDistance[entity] = true; }
                if (ImGui::DragFloat("##MaxDistance", &audio.MaxDistance, 0.1f, audio.MinDistance, 10000.0f, "%.2f")) {
                    isEditingMaxDistance[entity] = true;
                }
                if (isEditingMaxDistance[entity] && !ImGui::IsItemActive()) {
                    float oldVal = startMaxDistance[entity];
                    float newVal = audio.MaxDistance;
                    if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal, &ecs]() {
                                if (ecs.HasComponent<AudioComponent>(entity)) {
                                    ecs.GetComponent<AudioComponent>(entity).MaxDistance = newVal;
                                }
                            },
                            [entity, oldVal, &ecs]() {
                                if (ecs.HasComponent<AudioComponent>(entity)) {
                                    ecs.GetComponent<AudioComponent>(entity).MaxDistance = oldVal;
                                }
                            },
                            "Change Audio Max Distance"
                        );
                    }
                    isEditingMaxDistance[entity] = false;
                }
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
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("ParticleComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        ParticleComponent &particle = *static_cast<ParticleComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, bool> startIsEmitting;
        static std::unordered_map<Entity, int> startMaxParticles;
        static std::unordered_map<Entity, bool> isEditingMaxParticles;
        static std::unordered_map<Entity, float> startEmissionRate;
        static std::unordered_map<Entity, bool> isEditingEmissionRate;
        static std::unordered_map<Entity, float> startParticleLifetime;
        static std::unordered_map<Entity, bool> isEditingParticleLifetime;
        static std::unordered_map<Entity, float> startStartSize;
        static std::unordered_map<Entity, bool> isEditingStartSize;
        static std::unordered_map<Entity, float> startEndSize;
        static std::unordered_map<Entity, bool> isEditingEndSize;
        static std::unordered_map<Entity, Vector3D> startStartColor;
        static std::unordered_map<Entity, bool> isEditingStartColor;
        static std::unordered_map<Entity, float> startStartColorAlpha;
        static std::unordered_map<Entity, bool> isEditingStartColorAlpha;
        static std::unordered_map<Entity, Vector3D> startEndColor;
        static std::unordered_map<Entity, bool> isEditingEndColor;
        static std::unordered_map<Entity, float> startEndColorAlpha;
        static std::unordered_map<Entity, bool> isEditingEndColorAlpha;
        static std::unordered_map<Entity, Vector3D> startGravity;
        static std::unordered_map<Entity, bool> isEditingGravity;
        static std::unordered_map<Entity, float> startVelocityRandomness;
        static std::unordered_map<Entity, bool> isEditingVelocityRandomness;
        static std::unordered_map<Entity, Vector3D> startInitialVelocity;
        static std::unordered_map<Entity, bool> isEditingInitialVelocity;

        // Initialize tracking state
        if (isEditingMaxParticles.find(entity) == isEditingMaxParticles.end()) isEditingMaxParticles[entity] = false;
        if (isEditingEmissionRate.find(entity) == isEditingEmissionRate.end()) isEditingEmissionRate[entity] = false;
        if (isEditingParticleLifetime.find(entity) == isEditingParticleLifetime.end()) isEditingParticleLifetime[entity] = false;
        if (isEditingStartSize.find(entity) == isEditingStartSize.end()) isEditingStartSize[entity] = false;
        if (isEditingEndSize.find(entity) == isEditingEndSize.end()) isEditingEndSize[entity] = false;
        if (isEditingStartColor.find(entity) == isEditingStartColor.end()) isEditingStartColor[entity] = false;
        if (isEditingStartColorAlpha.find(entity) == isEditingStartColorAlpha.end()) isEditingStartColorAlpha[entity] = false;
        if (isEditingEndColor.find(entity) == isEditingEndColor.end()) isEditingEndColor[entity] = false;
        if (isEditingEndColorAlpha.find(entity) == isEditingEndColorAlpha.end()) isEditingEndColorAlpha[entity] = false;
        if (isEditingGravity.find(entity) == isEditingGravity.end()) isEditingGravity[entity] = false;
        if (isEditingVelocityRandomness.find(entity) == isEditingVelocityRandomness.end()) isEditingVelocityRandomness[entity] = false;
        if (isEditingInitialVelocity.find(entity) == isEditingInitialVelocity.end()) isEditingInitialVelocity[entity] = false;

        // Play/Pause/Stop buttons for editor preview
        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (EditorComponents::DrawPlayButton(particle.isPlayingInEditor && !particle.isPausedInEditor, buttonWidth))
        {
            particle.isPlayingInEditor = true;
            particle.isPausedInEditor = false;
        }

        ImGui::SameLine();

        if (EditorComponents::DrawPauseButton(particle.isPausedInEditor, buttonWidth))
        {
            if (particle.isPlayingInEditor)
            {
                particle.isPausedInEditor = !particle.isPausedInEditor;
            }
        }

        if (EditorComponents::DrawStopButton())
        {
            particle.isPlayingInEditor = false;
            particle.isPausedInEditor = false;
            particle.particles.clear();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show active particle count
        ImGui::Text("Active Particles: %zu / %d", particle.particles.size(), particle.maxParticles);

        // Is Emitting checkbox with entity-aware undo
        startIsEmitting[entity] = particle.isEmitting;
        bool isEmittingVal = particle.isEmitting;
        if (ImGui::Checkbox("Is Emitting", &isEmittingVal)) {
            bool oldVal = startIsEmitting[entity];
            bool newVal = isEmittingVal;
            particle.isEmitting = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ParticleComponent>(entity)) {
                            ecs.GetComponent<ParticleComponent>(entity).isEmitting = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<ParticleComponent>(entity)) {
                            ecs.GetComponent<ParticleComponent>(entity).isEmitting = oldVal;
                        }
                    },
                    "Toggle Particle Emitting"
                );
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Whether the particle system is actively emitting new particles");
        }

        ImGui::Separator();
        ImGui::Text("Emitter Settings");

        // Max Particles
        ImGui::Text("Max Particles");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingMaxParticles[entity]) startMaxParticles[entity] = particle.maxParticles;
        if (ImGui::IsItemActivated()) { startMaxParticles[entity] = particle.maxParticles; isEditingMaxParticles[entity] = true; }
        if (ImGui::DragInt("##MaxParticles", &particle.maxParticles, 1.0f, 1, 10000)) {
            isEditingMaxParticles[entity] = true;
        }
        if (isEditingMaxParticles[entity] && !ImGui::IsItemActive()) {
            int oldVal = startMaxParticles[entity];
            int newVal = particle.maxParticles;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).maxParticles = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).maxParticles = oldVal; },
                    "Edit Max Particles"
                );
            }
            isEditingMaxParticles[entity] = false;
        }

        // Emission Rate
        ImGui::Text("Emission Rate");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingEmissionRate[entity]) startEmissionRate[entity] = particle.emissionRate;
        if (ImGui::IsItemActivated()) { startEmissionRate[entity] = particle.emissionRate; isEditingEmissionRate[entity] = true; }
        if (ImGui::DragFloat("##EmissionRate", &particle.emissionRate, 0.1f, 0.0f, 1000.0f)) {
            isEditingEmissionRate[entity] = true;
        }
        if (isEditingEmissionRate[entity] && !ImGui::IsItemActive()) {
            float oldVal = startEmissionRate[entity];
            float newVal = particle.emissionRate;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).emissionRate = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).emissionRate = oldVal; },
                    "Edit Emission Rate"
                );
            }
            isEditingEmissionRate[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Particle Properties");

        // Particle Lifetime
        ImGui::Text("Lifetime");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingParticleLifetime[entity]) startParticleLifetime[entity] = particle.particleLifetime;
        if (ImGui::IsItemActivated()) { startParticleLifetime[entity] = particle.particleLifetime; isEditingParticleLifetime[entity] = true; }
        if (ImGui::DragFloat("##ParticleLifetime", &particle.particleLifetime, 0.1f, 0.01f, 100.0f)) {
            isEditingParticleLifetime[entity] = true;
        }
        if (isEditingParticleLifetime[entity] && !ImGui::IsItemActive()) {
            float oldVal = startParticleLifetime[entity];
            float newVal = particle.particleLifetime;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).particleLifetime = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).particleLifetime = oldVal; },
                    "Edit Particle Lifetime"
                );
            }
            isEditingParticleLifetime[entity] = false;
        }

        // Start Size
        ImGui::Text("Start Size");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingStartSize[entity]) startStartSize[entity] = particle.startSize;
        if (ImGui::IsItemActivated()) { startStartSize[entity] = particle.startSize; isEditingStartSize[entity] = true; }
        if (ImGui::DragFloat("##StartSize", &particle.startSize, 0.01f, 0.0f, 10.0f)) {
            isEditingStartSize[entity] = true;
        }
        if (isEditingStartSize[entity] && !ImGui::IsItemActive()) {
            float oldVal = startStartSize[entity];
            float newVal = particle.startSize;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).startSize = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).startSize = oldVal; },
                    "Edit Start Size"
                );
            }
            isEditingStartSize[entity] = false;
        }

        // End Size
        ImGui::Text("End Size");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingEndSize[entity]) startEndSize[entity] = particle.endSize;
        if (ImGui::IsItemActivated()) { startEndSize[entity] = particle.endSize; isEditingEndSize[entity] = true; }
        if (ImGui::DragFloat("##EndSize", &particle.endSize, 0.01f, 0.0f, 10.0f)) {
            isEditingEndSize[entity] = true;
        }
        if (isEditingEndSize[entity] && !ImGui::IsItemActive()) {
            float oldVal = startEndSize[entity];
            float newVal = particle.endSize;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).endSize = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).endSize = oldVal; },
                    "Edit End Size"
                );
            }
            isEditingEndSize[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Color Settings");

        // Start Color
        ImGui::Text("Start Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float startColorArr[3] = { particle.startColor.x, particle.startColor.y, particle.startColor.z };
        if (!isEditingStartColor[entity]) startStartColor[entity] = particle.startColor;
        if (ImGui::IsItemActivated()) { startStartColor[entity] = particle.startColor; isEditingStartColor[entity] = true; }
        if (ImGui::ColorEdit3("##StartColor", startColorArr)) {
            particle.startColor = Vector3D(startColorArr[0], startColorArr[1], startColorArr[2]);
            isEditingStartColor[entity] = true;
        }
        if (isEditingStartColor[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startStartColor[entity];
            Vector3D newVal = particle.startColor;
            if ((oldVal.x != newVal.x || oldVal.y != newVal.y || oldVal.z != newVal.z) && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).startColor = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).startColor = oldVal; },
                    "Edit Start Color"
                );
            }
            isEditingStartColor[entity] = false;
        }

        // Start Color Alpha
        ImGui::Text("Start Alpha");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingStartColorAlpha[entity]) startStartColorAlpha[entity] = particle.startColorAlpha;
        if (ImGui::IsItemActivated()) { startStartColorAlpha[entity] = particle.startColorAlpha; isEditingStartColorAlpha[entity] = true; }
        if (ImGui::DragFloat("##StartColorAlpha", &particle.startColorAlpha, 0.01f, 0.0f, 1.0f)) {
            isEditingStartColorAlpha[entity] = true;
        }
        if (isEditingStartColorAlpha[entity] && !ImGui::IsItemActive()) {
            float oldVal = startStartColorAlpha[entity];
            float newVal = particle.startColorAlpha;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).startColorAlpha = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).startColorAlpha = oldVal; },
                    "Edit Start Alpha"
                );
            }
            isEditingStartColorAlpha[entity] = false;
        }

        // End Color
        ImGui::Text("End Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float endColorArr[3] = { particle.endColor.x, particle.endColor.y, particle.endColor.z };
        if (!isEditingEndColor[entity]) startEndColor[entity] = particle.endColor;
        if (ImGui::IsItemActivated()) { startEndColor[entity] = particle.endColor; isEditingEndColor[entity] = true; }
        if (ImGui::ColorEdit3("##EndColor", endColorArr)) {
            particle.endColor = Vector3D(endColorArr[0], endColorArr[1], endColorArr[2]);
            isEditingEndColor[entity] = true;
        }
        if (isEditingEndColor[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startEndColor[entity];
            Vector3D newVal = particle.endColor;
            if ((oldVal.x != newVal.x || oldVal.y != newVal.y || oldVal.z != newVal.z) && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).endColor = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).endColor = oldVal; },
                    "Edit End Color"
                );
            }
            isEditingEndColor[entity] = false;
        }

        // End Color Alpha
        ImGui::Text("End Alpha");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingEndColorAlpha[entity]) startEndColorAlpha[entity] = particle.endColorAlpha;
        if (ImGui::IsItemActivated()) { startEndColorAlpha[entity] = particle.endColorAlpha; isEditingEndColorAlpha[entity] = true; }
        if (ImGui::DragFloat("##EndColorAlpha", &particle.endColorAlpha, 0.01f, 0.0f, 1.0f)) {
            isEditingEndColorAlpha[entity] = true;
        }
        if (isEditingEndColorAlpha[entity] && !ImGui::IsItemActive()) {
            float oldVal = startEndColorAlpha[entity];
            float newVal = particle.endColorAlpha;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).endColorAlpha = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).endColorAlpha = oldVal; },
                    "Edit End Alpha"
                );
            }
            isEditingEndColorAlpha[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Physics");

        // Gravity
        ImGui::Text("Gravity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float gravityArr[3] = { particle.gravity.x, particle.gravity.y, particle.gravity.z };
        if (!isEditingGravity[entity]) startGravity[entity] = particle.gravity;
        if (ImGui::IsItemActivated()) { startGravity[entity] = particle.gravity; isEditingGravity[entity] = true; }
        if (ImGui::DragFloat3("##Gravity", gravityArr, 0.1f)) {
            particle.gravity = Vector3D(gravityArr[0], gravityArr[1], gravityArr[2]);
            isEditingGravity[entity] = true;
        }
        if (isEditingGravity[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startGravity[entity];
            Vector3D newVal = particle.gravity;
            if ((oldVal.x != newVal.x || oldVal.y != newVal.y || oldVal.z != newVal.z) && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).gravity = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).gravity = oldVal; },
                    "Edit Gravity"
                );
            }
            isEditingGravity[entity] = false;
        }

        // Velocity Randomness
        ImGui::Text("Velocity Randomness");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingVelocityRandomness[entity]) startVelocityRandomness[entity] = particle.velocityRandomness;
        if (ImGui::IsItemActivated()) { startVelocityRandomness[entity] = particle.velocityRandomness; isEditingVelocityRandomness[entity] = true; }
        if (ImGui::DragFloat("##VelocityRandomness", &particle.velocityRandomness, 0.1f, 0.0f, 10.0f)) {
            isEditingVelocityRandomness[entity] = true;
        }
        if (isEditingVelocityRandomness[entity] && !ImGui::IsItemActive()) {
            float oldVal = startVelocityRandomness[entity];
            float newVal = particle.velocityRandomness;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).velocityRandomness = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).velocityRandomness = oldVal; },
                    "Edit Velocity Randomness"
                );
            }
            isEditingVelocityRandomness[entity] = false;
        }

        // Initial Velocity
        ImGui::Text("Initial Velocity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float initialVelArr[3] = { particle.initialVelocity.x, particle.initialVelocity.y, particle.initialVelocity.z };
        if (!isEditingInitialVelocity[entity]) startInitialVelocity[entity] = particle.initialVelocity;
        if (ImGui::IsItemActivated()) { startInitialVelocity[entity] = particle.initialVelocity; isEditingInitialVelocity[entity] = true; }
        if (ImGui::DragFloat3("##InitialVelocity", initialVelArr, 0.1f)) {
            particle.initialVelocity = Vector3D(initialVelArr[0], initialVelArr[1], initialVelArr[2]);
            isEditingInitialVelocity[entity] = true;
        }
        if (isEditingInitialVelocity[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startInitialVelocity[entity];
            Vector3D newVal = particle.initialVelocity;
            if ((oldVal.x != newVal.x || oldVal.y != newVal.y || oldVal.z != newVal.z) && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).initialVelocity = newVal; },
                    [entity, oldVal]() { ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager(); if (ecs.HasComponent<ParticleComponent>(entity)) ecs.GetComponent<ParticleComponent>(entity).initialVelocity = oldVal; },
                    "Edit Initial Velocity"
                );
            }
            isEditingInitialVelocity[entity] = false;
        }

        // Note: textureGUID is handled by a separate field renderer
        return false; // Return false to let the textureGUID field renderer run
    });

    // ==================== DIRECTIONAL LIGHT COMPONENT ====================
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("DirectionalLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        DirectionalLightComponent &light = *static_cast<DirectionalLightComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, bool> startEnabled;
        static std::unordered_map<Entity, bool> isEditingEnabled;
        static std::unordered_map<Entity, Vector3D> startColor;
        static std::unordered_map<Entity, bool> isEditingColor;
        static std::unordered_map<Entity, float> startIntensity;
        static std::unordered_map<Entity, bool> isEditingIntensity;
        static std::unordered_map<Entity, Vector3D> startAmbient;
        static std::unordered_map<Entity, bool> isEditingAmbient;
        static std::unordered_map<Entity, Vector3D> startDiffuse;
        static std::unordered_map<Entity, bool> isEditingDiffuse;
        static std::unordered_map<Entity, Vector3D> startSpecular;
        static std::unordered_map<Entity, bool> isEditingSpecular;

        // Initialize tracking state
        if (isEditingEnabled.find(entity) == isEditingEnabled.end()) isEditingEnabled[entity] = false;
        if (isEditingColor.find(entity) == isEditingColor.end()) isEditingColor[entity] = false;
        if (isEditingIntensity.find(entity) == isEditingIntensity.end()) isEditingIntensity[entity] = false;
        if (isEditingAmbient.find(entity) == isEditingAmbient.end()) isEditingAmbient[entity] = false;
        if (isEditingDiffuse.find(entity) == isEditingDiffuse.end()) isEditingDiffuse[entity] = false;
        if (isEditingSpecular.find(entity) == isEditingSpecular.end()) isEditingSpecular[entity] = false;

        // Enabled checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);

        if (!isEditingEnabled[entity]) startEnabled[entity] = light.enabled;
        bool enabledVal = light.enabled;
        if (ImGui::Checkbox("##Enabled", &enabledVal)) {
            bool oldVal = startEnabled[entity];
            bool newVal = enabledVal;
            light.enabled = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).enabled = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).enabled = oldVal;
                        }
                    },
                    "Toggle Directional Light"
                );
            }
        }

        // Color
        ImGui::Text("Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float color[3] = {light.color.x, light.color.y, light.color.z};
        if (!isEditingColor[entity]) startColor[entity] = light.color;
        if (ImGui::IsItemActivated()) { startColor[entity] = light.color; isEditingColor[entity] = true; }
        if (ImGui::ColorEdit3("##Color", color)) {
            light.color = Vector3D(color[0], color[1], color[2]);
            isEditingColor[entity] = true;
        }
        if (isEditingColor[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startColor[entity];
            Vector3D newVal = light.color;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).color = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).color = oldVal;
                        }
                    },
                    "Change Light Color"
                );
            }
            isEditingColor[entity] = false;
        }

        // Intensity
        ImGui::Text("Intensity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingIntensity[entity]) startIntensity[entity] = light.intensity;
        if (ImGui::IsItemActivated()) { startIntensity[entity] = light.intensity; isEditingIntensity[entity] = true; }
        if (ImGui::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 10.0f)) {
            isEditingIntensity[entity] = true;
        }
        if (isEditingIntensity[entity] && !ImGui::IsItemActive()) {
            float oldVal = startIntensity[entity];
            float newVal = light.intensity;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).intensity = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).intensity = oldVal;
                        }
                    },
                    "Change Light Intensity"
                );
            }
            isEditingIntensity[entity] = false;
        }

        // Note: Direction is controlled via Transform rotation
        ImGui::Separator();
        ImGui::Text("Lighting Properties");

        // Ambient
        ImGui::Text("Ambient");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float ambient[3] = {light.ambient.x, light.ambient.y, light.ambient.z};
        if (!isEditingAmbient[entity]) startAmbient[entity] = light.ambient;
        if (ImGui::IsItemActivated()) { startAmbient[entity] = light.ambient; isEditingAmbient[entity] = true; }
        if (ImGui::ColorEdit3("##Ambient", ambient)) {
            light.ambient = Vector3D(ambient[0], ambient[1], ambient[2]);
            isEditingAmbient[entity] = true;
        }
        if (isEditingAmbient[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startAmbient[entity];
            Vector3D newVal = light.ambient;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).ambient = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).ambient = oldVal;
                        }
                    },
                    "Change Light Ambient"
                );
            }
            isEditingAmbient[entity] = false;
        }

        // Diffuse
        ImGui::Text("Diffuse");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float diffuse[3] = {light.diffuse.x, light.diffuse.y, light.diffuse.z};
        if (!isEditingDiffuse[entity]) startDiffuse[entity] = light.diffuse;
        if (ImGui::IsItemActivated()) { startDiffuse[entity] = light.diffuse; isEditingDiffuse[entity] = true; }
        if (ImGui::ColorEdit3("##Diffuse", diffuse)) {
            light.diffuse = Vector3D(diffuse[0], diffuse[1], diffuse[2]);
            isEditingDiffuse[entity] = true;
        }
        if (isEditingDiffuse[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startDiffuse[entity];
            Vector3D newVal = light.diffuse;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).diffuse = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).diffuse = oldVal;
                        }
                    },
                    "Change Light Diffuse"
                );
            }
            isEditingDiffuse[entity] = false;
        }

        // Specular
        ImGui::Text("Specular");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float specular[3] = {light.specular.x, light.specular.y, light.specular.z};
        if (!isEditingSpecular[entity]) startSpecular[entity] = light.specular;
        if (ImGui::IsItemActivated()) { startSpecular[entity] = light.specular; isEditingSpecular[entity] = true; }
        if (ImGui::ColorEdit3("##Specular", specular)) {
            light.specular = Vector3D(specular[0], specular[1], specular[2]);
            isEditingSpecular[entity] = true;
        }
        if (isEditingSpecular[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startSpecular[entity];
            Vector3D newVal = light.specular;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).specular = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<DirectionalLightComponent>(entity)) {
                            ecs.GetComponent<DirectionalLightComponent>(entity).specular = oldVal;
                        }
                    },
                    "Change Light Specular"
                );
            }
            isEditingSpecular[entity] = false;
        }

        return true; // Return true to skip default field rendering
    });

    // ==================== POINT LIGHT COMPONENT ====================
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("PointLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        PointLightComponent &light = *static_cast<PointLightComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, bool> startEnabled;
        static std::unordered_map<Entity, Vector3D> startColor;
        static std::unordered_map<Entity, bool> isEditingColor;
        static std::unordered_map<Entity, float> startIntensity;
        static std::unordered_map<Entity, bool> isEditingIntensity;
        static std::unordered_map<Entity, float> startConstant;
        static std::unordered_map<Entity, bool> isEditingConstant;
        static std::unordered_map<Entity, float> startLinear;
        static std::unordered_map<Entity, bool> isEditingLinear;
        static std::unordered_map<Entity, float> startQuadratic;
        static std::unordered_map<Entity, bool> isEditingQuadratic;
        static std::unordered_map<Entity, Vector3D> startAmbient;
        static std::unordered_map<Entity, bool> isEditingAmbient;
        static std::unordered_map<Entity, Vector3D> startDiffuse;
        static std::unordered_map<Entity, bool> isEditingDiffuse;
        static std::unordered_map<Entity, Vector3D> startSpecular;
        static std::unordered_map<Entity, bool> isEditingSpecular;
        static std::unordered_map<Entity, bool> startCastShadows;

        // Initialize tracking state
        if (isEditingColor.find(entity) == isEditingColor.end()) isEditingColor[entity] = false;
        if (isEditingIntensity.find(entity) == isEditingIntensity.end()) isEditingIntensity[entity] = false;
        if (isEditingConstant.find(entity) == isEditingConstant.end()) isEditingConstant[entity] = false;
        if (isEditingLinear.find(entity) == isEditingLinear.end()) isEditingLinear[entity] = false;
        if (isEditingQuadratic.find(entity) == isEditingQuadratic.end()) isEditingQuadratic[entity] = false;
        if (isEditingAmbient.find(entity) == isEditingAmbient.end()) isEditingAmbient[entity] = false;
        if (isEditingDiffuse.find(entity) == isEditingDiffuse.end()) isEditingDiffuse[entity] = false;
        if (isEditingSpecular.find(entity) == isEditingSpecular.end()) isEditingSpecular[entity] = false;

        // Enabled checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        startEnabled[entity] = light.enabled;
        bool enabledVal = light.enabled;
        if (ImGui::Checkbox("##Enabled", &enabledVal)) {
            bool oldVal = startEnabled[entity];
            bool newVal = enabledVal;
            light.enabled = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).enabled = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).enabled = oldVal;
                        }
                    },
                    "Toggle Point Light"
                );
            }
        }

        // Color
        ImGui::Text("Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float color[3] = {light.color.x, light.color.y, light.color.z};
        if (!isEditingColor[entity]) startColor[entity] = light.color;
        if (ImGui::IsItemActivated()) { startColor[entity] = light.color; isEditingColor[entity] = true; }
        if (ImGui::ColorEdit3("##Color", color)) {
            light.color = Vector3D(color[0], color[1], color[2]);
            isEditingColor[entity] = true;
        }
        if (isEditingColor[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startColor[entity];
            Vector3D newVal = light.color;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).color = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).color = oldVal;
                        }
                    },
                    "Change Point Light Color"
                );
            }
            isEditingColor[entity] = false;
        }

        // Intensity
        ImGui::Text("Intensity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingIntensity[entity]) startIntensity[entity] = light.intensity;
        if (ImGui::IsItemActivated()) { startIntensity[entity] = light.intensity; isEditingIntensity[entity] = true; }
        if (ImGui::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 10.0f)) {
            isEditingIntensity[entity] = true;
        }
        if (isEditingIntensity[entity] && !ImGui::IsItemActive()) {
            float oldVal = startIntensity[entity];
            float newVal = light.intensity;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).intensity = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).intensity = oldVal;
                        }
                    },
                    "Change Point Light Intensity"
                );
            }
            isEditingIntensity[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Attenuation");

        // Constant
        ImGui::Text("Constant");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingConstant[entity]) startConstant[entity] = light.constant;
        if (ImGui::IsItemActivated()) { startConstant[entity] = light.constant; isEditingConstant[entity] = true; }
        if (ImGui::DragFloat("##Constant", &light.constant, 0.01f, 0.0f, 2.0f)) {
            isEditingConstant[entity] = true;
        }
        if (isEditingConstant[entity] && !ImGui::IsItemActive()) {
            float oldVal = startConstant[entity];
            float newVal = light.constant;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).constant = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).constant = oldVal;
                        }
                    },
                    "Change Light Constant"
                );
            }
            isEditingConstant[entity] = false;
        }

        // Linear
        ImGui::Text("Linear");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingLinear[entity]) startLinear[entity] = light.linear;
        if (ImGui::IsItemActivated()) { startLinear[entity] = light.linear; isEditingLinear[entity] = true; }
        if (ImGui::DragFloat("##Linear", &light.linear, 0.01f, 0.0f, 1.0f)) {
            isEditingLinear[entity] = true;
        }
        if (isEditingLinear[entity] && !ImGui::IsItemActive()) {
            float oldVal = startLinear[entity];
            float newVal = light.linear;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).linear = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).linear = oldVal;
                        }
                    },
                    "Change Light Linear"
                );
            }
            isEditingLinear[entity] = false;
        }

        // Quadratic
        ImGui::Text("Quadratic");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingQuadratic[entity]) startQuadratic[entity] = light.quadratic;
        if (ImGui::IsItemActivated()) { startQuadratic[entity] = light.quadratic; isEditingQuadratic[entity] = true; }
        if (ImGui::DragFloat("##Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f)) {
            isEditingQuadratic[entity] = true;
        }
        if (isEditingQuadratic[entity] && !ImGui::IsItemActive()) {
            float oldVal = startQuadratic[entity];
            float newVal = light.quadratic;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).quadratic = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).quadratic = oldVal;
                        }
                    },
                    "Change Light Quadratic"
                );
            }
            isEditingQuadratic[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Lighting Properties");

        // Ambient
        ImGui::Text("Ambient");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float ambient[3] = {light.ambient.x, light.ambient.y, light.ambient.z};
        if (!isEditingAmbient[entity]) startAmbient[entity] = light.ambient;
        if (ImGui::IsItemActivated()) { startAmbient[entity] = light.ambient; isEditingAmbient[entity] = true; }
        if (ImGui::ColorEdit3("##Ambient", ambient)) {
            light.ambient = Vector3D(ambient[0], ambient[1], ambient[2]);
            isEditingAmbient[entity] = true;
        }
        if (isEditingAmbient[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startAmbient[entity];
            Vector3D newVal = light.ambient;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).ambient = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).ambient = oldVal;
                        }
                    },
                    "Change Point Light Ambient"
                );
            }
            isEditingAmbient[entity] = false;
        }

        // Diffuse
        ImGui::Text("Diffuse");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float diffuse[3] = {light.diffuse.x, light.diffuse.y, light.diffuse.z};
        if (!isEditingDiffuse[entity]) startDiffuse[entity] = light.diffuse;
        if (ImGui::IsItemActivated()) { startDiffuse[entity] = light.diffuse; isEditingDiffuse[entity] = true; }
        if (ImGui::ColorEdit3("##Diffuse", diffuse)) {
            light.diffuse = Vector3D(diffuse[0], diffuse[1], diffuse[2]);
            isEditingDiffuse[entity] = true;
        }
        if (isEditingDiffuse[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startDiffuse[entity];
            Vector3D newVal = light.diffuse;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).diffuse = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).diffuse = oldVal;
                        }
                    },
                    "Change Point Light Diffuse"
                );
            }
            isEditingDiffuse[entity] = false;
        }

        // Specular
        ImGui::Text("Specular");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float specular[3] = {light.specular.x, light.specular.y, light.specular.z};
        if (!isEditingSpecular[entity]) startSpecular[entity] = light.specular;
        if (ImGui::IsItemActivated()) { startSpecular[entity] = light.specular; isEditingSpecular[entity] = true; }
        if (ImGui::ColorEdit3("##Specular", specular)) {
            light.specular = Vector3D(specular[0], specular[1], specular[2]);
            isEditingSpecular[entity] = true;
        }
        if (isEditingSpecular[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startSpecular[entity];
            Vector3D newVal = light.specular;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).specular = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).specular = oldVal;
                        }
                    },
                    "Change Point Light Specular"
                );
            }
            isEditingSpecular[entity] = false;
        }

        // Cast Shadow checkbox
        ImGui::Text("Cast Shadow");
        ImGui::SameLine(labelWidth);
        startCastShadows[entity] = light.castShadows;
        bool castShadowsVal = light.castShadows;
        if (ImGui::Checkbox("##CastShadow", &castShadowsVal)) {
            bool oldVal = startCastShadows[entity];
            bool newVal = castShadowsVal;
            light.castShadows = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).castShadows = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<PointLightComponent>(entity)) {
                            ecs.GetComponent<PointLightComponent>(entity).castShadows = oldVal;
                        }
                    },
                    "Toggle Point Light Shadows"
                );
            }
        }

        return true; // Return true to skip default field rendering
    });

    // ==================== SPOT LIGHT COMPONENT ====================
    // Uses entity-aware lambda commands for proper undo/redo

    ReflectionRenderer::RegisterComponentRenderer("SpotLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        SpotLightComponent &light = *static_cast<SpotLightComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, bool> startEnabled;
        static std::unordered_map<Entity, Vector3D> startColor;
        static std::unordered_map<Entity, bool> isEditingColor;
        static std::unordered_map<Entity, float> startIntensity;
        static std::unordered_map<Entity, bool> isEditingIntensity;
        static std::unordered_map<Entity, float> startCutOff;
        static std::unordered_map<Entity, bool> isEditingCutOff;
        static std::unordered_map<Entity, float> startOuterCutOff;
        static std::unordered_map<Entity, bool> isEditingOuterCutOff;
        static std::unordered_map<Entity, float> startConstant;
        static std::unordered_map<Entity, bool> isEditingConstant;
        static std::unordered_map<Entity, float> startLinear;
        static std::unordered_map<Entity, bool> isEditingLinear;
        static std::unordered_map<Entity, float> startQuadratic;
        static std::unordered_map<Entity, bool> isEditingQuadratic;
        static std::unordered_map<Entity, Vector3D> startAmbient;
        static std::unordered_map<Entity, bool> isEditingAmbient;
        static std::unordered_map<Entity, Vector3D> startDiffuse;
        static std::unordered_map<Entity, bool> isEditingDiffuse;
        static std::unordered_map<Entity, Vector3D> startSpecular;
        static std::unordered_map<Entity, bool> isEditingSpecular;

        // Initialize tracking state
        if (isEditingColor.find(entity) == isEditingColor.end()) isEditingColor[entity] = false;
        if (isEditingIntensity.find(entity) == isEditingIntensity.end()) isEditingIntensity[entity] = false;
        if (isEditingCutOff.find(entity) == isEditingCutOff.end()) isEditingCutOff[entity] = false;
        if (isEditingOuterCutOff.find(entity) == isEditingOuterCutOff.end()) isEditingOuterCutOff[entity] = false;
        if (isEditingConstant.find(entity) == isEditingConstant.end()) isEditingConstant[entity] = false;
        if (isEditingLinear.find(entity) == isEditingLinear.end()) isEditingLinear[entity] = false;
        if (isEditingQuadratic.find(entity) == isEditingQuadratic.end()) isEditingQuadratic[entity] = false;
        if (isEditingAmbient.find(entity) == isEditingAmbient.end()) isEditingAmbient[entity] = false;
        if (isEditingDiffuse.find(entity) == isEditingDiffuse.end()) isEditingDiffuse[entity] = false;
        if (isEditingSpecular.find(entity) == isEditingSpecular.end()) isEditingSpecular[entity] = false;

        // Enabled checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        startEnabled[entity] = light.enabled;
        bool enabledVal = light.enabled;
        if (ImGui::Checkbox("##Enabled", &enabledVal)) {
            bool oldVal = startEnabled[entity];
            bool newVal = enabledVal;
            light.enabled = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).enabled = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).enabled = oldVal;
                        }
                    },
                    "Toggle Spot Light"
                );
            }
        }

        // Color
        ImGui::Text("Color");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float color[3] = {light.color.x, light.color.y, light.color.z};
        if (!isEditingColor[entity]) startColor[entity] = light.color;
        if (ImGui::IsItemActivated()) { startColor[entity] = light.color; isEditingColor[entity] = true; }
        if (ImGui::ColorEdit3("##Color", color)) {
            light.color = Vector3D(color[0], color[1], color[2]);
            isEditingColor[entity] = true;
        }
        if (isEditingColor[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startColor[entity];
            Vector3D newVal = light.color;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).color = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).color = oldVal;
                        }
                    },
                    "Change Spot Light Color"
                );
            }
            isEditingColor[entity] = false;
        }

        // Intensity
        ImGui::Text("Intensity");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingIntensity[entity]) startIntensity[entity] = light.intensity;
        if (ImGui::IsItemActivated()) { startIntensity[entity] = light.intensity; isEditingIntensity[entity] = true; }
        if (ImGui::DragFloat("##Intensity", &light.intensity, 0.1f, 0.0f, 10.0f)) {
            isEditingIntensity[entity] = true;
        }
        if (isEditingIntensity[entity] && !ImGui::IsItemActive()) {
            float oldVal = startIntensity[entity];
            float newVal = light.intensity;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).intensity = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).intensity = oldVal;
                        }
                    },
                    "Change Spot Light Intensity"
                );
            }
            isEditingIntensity[entity] = false;
        }

        // Note: Direction is controlled via Transform rotation
        ImGui::Separator();
        ImGui::Text("Cone Settings");

        // Convert from cosine to degrees for easier editing
        float cutOffDegrees = glm::degrees(glm::acos(light.cutOff));
        float outerCutOffDegrees = glm::degrees(glm::acos(light.outerCutOff));

        // Inner Cutoff
        ImGui::Text("Inner Cutoff (degrees)");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingCutOff[entity]) startCutOff[entity] = light.cutOff;
        if (ImGui::IsItemActivated()) { startCutOff[entity] = light.cutOff; isEditingCutOff[entity] = true; }
        if (ImGui::DragFloat("##InnerCutoff", &cutOffDegrees, 1.0f, 0.0f, 90.0f)) {
            light.cutOff = glm::cos(glm::radians(cutOffDegrees));
            isEditingCutOff[entity] = true;
        }
        if (isEditingCutOff[entity] && !ImGui::IsItemActive()) {
            float oldVal = startCutOff[entity];
            float newVal = light.cutOff;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).cutOff = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).cutOff = oldVal;
                        }
                    },
                    "Change Spot Light Inner Cutoff"
                );
            }
            isEditingCutOff[entity] = false;
        }

        // Outer Cutoff
        ImGui::Text("Outer Cutoff (degrees)");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingOuterCutOff[entity]) startOuterCutOff[entity] = light.outerCutOff;
        if (ImGui::IsItemActivated()) { startOuterCutOff[entity] = light.outerCutOff; isEditingOuterCutOff[entity] = true; }
        if (ImGui::DragFloat("##OuterCutoff", &outerCutOffDegrees, 1.0f, 0.0f, 90.0f)) {
            light.outerCutOff = glm::cos(glm::radians(outerCutOffDegrees));
            isEditingOuterCutOff[entity] = true;
        }
        if (isEditingOuterCutOff[entity] && !ImGui::IsItemActive()) {
            float oldVal = startOuterCutOff[entity];
            float newVal = light.outerCutOff;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).outerCutOff = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).outerCutOff = oldVal;
                        }
                    },
                    "Change Spot Light Outer Cutoff"
                );
            }
            isEditingOuterCutOff[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Attenuation");

        // Constant
        ImGui::Text("Constant");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingConstant[entity]) startConstant[entity] = light.constant;
        if (ImGui::IsItemActivated()) { startConstant[entity] = light.constant; isEditingConstant[entity] = true; }
        if (ImGui::DragFloat("##Constant", &light.constant, 0.01f, 0.0f, 2.0f)) {
            isEditingConstant[entity] = true;
        }
        if (isEditingConstant[entity] && !ImGui::IsItemActive()) {
            float oldVal = startConstant[entity];
            float newVal = light.constant;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).constant = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).constant = oldVal;
                        }
                    },
                    "Change Spot Light Constant"
                );
            }
            isEditingConstant[entity] = false;
        }

        // Linear
        ImGui::Text("Linear");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingLinear[entity]) startLinear[entity] = light.linear;
        if (ImGui::IsItemActivated()) { startLinear[entity] = light.linear; isEditingLinear[entity] = true; }
        if (ImGui::DragFloat("##Linear", &light.linear, 0.01f, 0.0f, 1.0f)) {
            isEditingLinear[entity] = true;
        }
        if (isEditingLinear[entity] && !ImGui::IsItemActive()) {
            float oldVal = startLinear[entity];
            float newVal = light.linear;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).linear = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).linear = oldVal;
                        }
                    },
                    "Change Spot Light Linear"
                );
            }
            isEditingLinear[entity] = false;
        }

        // Quadratic
        ImGui::Text("Quadratic");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingQuadratic[entity]) startQuadratic[entity] = light.quadratic;
        if (ImGui::IsItemActivated()) { startQuadratic[entity] = light.quadratic; isEditingQuadratic[entity] = true; }
        if (ImGui::DragFloat("##Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f)) {
            isEditingQuadratic[entity] = true;
        }
        if (isEditingQuadratic[entity] && !ImGui::IsItemActive()) {
            float oldVal = startQuadratic[entity];
            float newVal = light.quadratic;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).quadratic = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).quadratic = oldVal;
                        }
                    },
                    "Change Spot Light Quadratic"
                );
            }
            isEditingQuadratic[entity] = false;
        }

        ImGui::Separator();
        ImGui::Text("Lighting Properties");

        // Ambient
        ImGui::Text("Ambient");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float ambient[3] = {light.ambient.x, light.ambient.y, light.ambient.z};
        if (!isEditingAmbient[entity]) startAmbient[entity] = light.ambient;
        if (ImGui::IsItemActivated()) { startAmbient[entity] = light.ambient; isEditingAmbient[entity] = true; }
        if (ImGui::ColorEdit3("##Ambient", ambient)) {
            light.ambient = Vector3D(ambient[0], ambient[1], ambient[2]);
            isEditingAmbient[entity] = true;
        }
        if (isEditingAmbient[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startAmbient[entity];
            Vector3D newVal = light.ambient;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).ambient = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).ambient = oldVal;
                        }
                    },
                    "Change Spot Light Ambient"
                );
            }
            isEditingAmbient[entity] = false;
        }

        // Diffuse
        ImGui::Text("Diffuse");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float diffuse[3] = {light.diffuse.x, light.diffuse.y, light.diffuse.z};
        if (!isEditingDiffuse[entity]) startDiffuse[entity] = light.diffuse;
        if (ImGui::IsItemActivated()) { startDiffuse[entity] = light.diffuse; isEditingDiffuse[entity] = true; }
        if (ImGui::ColorEdit3("##Diffuse", diffuse)) {
            light.diffuse = Vector3D(diffuse[0], diffuse[1], diffuse[2]);
            isEditingDiffuse[entity] = true;
        }
        if (isEditingDiffuse[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startDiffuse[entity];
            Vector3D newVal = light.diffuse;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).diffuse = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).diffuse = oldVal;
                        }
                    },
                    "Change Spot Light Diffuse"
                );
            }
            isEditingDiffuse[entity] = false;
        }

        // Specular
        ImGui::Text("Specular");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        float specular[3] = {light.specular.x, light.specular.y, light.specular.z};
        if (!isEditingSpecular[entity]) startSpecular[entity] = light.specular;
        if (ImGui::IsItemActivated()) { startSpecular[entity] = light.specular; isEditingSpecular[entity] = true; }
        if (ImGui::ColorEdit3("##Specular", specular)) {
            light.specular = Vector3D(specular[0], specular[1], specular[2]);
            isEditingSpecular[entity] = true;
        }
        if (isEditingSpecular[entity] && !ImGui::IsItemActive()) {
            Vector3D oldVal = startSpecular[entity];
            Vector3D newVal = light.specular;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).specular = newVal;
                        }
                    },
                    [entity, oldVal]() {
                        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecs.HasComponent<SpotLightComponent>(entity)) {
                            ecs.GetComponent<SpotLightComponent>(entity).specular = oldVal;
                        }
                    },
                    "Change Spot Light Specular"
                );
            }
            isEditingSpecular[entity] = false;
        }

        return true;
    });

    // ==================== ANIMATION COMPONENT (Unity-style) ====================
    ReflectionRenderer::RegisterComponentRenderer("AnimationComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
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
                    // Store GUIDs for cross-machine compatibility
                    animComp.clipGUIDs.clear();
                    for (const auto& clipPath : ctrlClipPaths) {
                        GUID_128 guid = AssetManager::GetInstance().GetGUID128FromAssetMeta(clipPath);
                        animComp.clipGUIDs.push_back(guid);
                    }

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
                                    // Store GUIDs for cross-machine compatibility
                                    animComp.clipGUIDs.clear();
                                    for (const auto& clipPath : ctrlClipPaths) {
                                        GUID_128 guid = AssetManager::GetInstance().GetGUID128FromAssetMeta(clipPath);
                                        animComp.clipGUIDs.push_back(guid);
                                    }

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
                        // If clips aren't loaded or out of sync, reload them first
                        if (animComp.GetClips().size() != animComp.clipPaths.size()) {
                            if (ecs.HasComponent<ModelRenderComponent>(entity)) {
                                auto& modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                                if (modelComp.model) {
                                    animComp.LoadClipsFromPaths(
                                        modelComp.model->GetBoneInfoMap(),
                                        modelComp.model->GetBoneCount(),
                                        entity
                                    );
                                }
                            }
                        }

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

    // BrainComponent uses entity-aware lambda commands for proper undo/redo
    ReflectionRenderer::RegisterComponentRenderer("BrainComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        BrainComponent &brain = *static_cast<BrainComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking for entity-aware undo
        static std::unordered_map<Entity, int> startKind;

        // Combo for Kind with entity-aware undo
        ImGui::Text("Kind");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        static const char *kKinds[] = {"None", "Grunt", "Boss"};
        int kindIdx = static_cast<int>(brain.kind);
        startKind[entity] = kindIdx;
        EditorComponents::PushComboColors();
        if (ImGui::BeginCombo("##Kind", kKinds[kindIdx])) {
            for (int i = 0; i < IM_ARRAYSIZE(kKinds); i++) {
                bool isSelected = (kindIdx == i);
                if (ImGui::Selectable(kKinds[i], isSelected)) {
                    int oldVal = startKind[entity];
                    int newVal = i;
                    brain.kind = static_cast<BrainKind>(newVal);
                    brain.kindInt = newVal;

                    if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                        UndoSystem::GetInstance().RecordLambdaChange(
                            [entity, newVal, &ecs]() {
                                if (ecs.HasComponent<BrainComponent>(entity)) {
                                    auto& b = ecs.GetComponent<BrainComponent>(entity);
                                    b.kind = static_cast<BrainKind>(newVal);
                                    b.kindInt = newVal;
                                }
                            },
                            [entity, oldVal, &ecs]() {
                                if (ecs.HasComponent<BrainComponent>(entity)) {
                                    auto& b = ecs.GetComponent<BrainComponent>(entity);
                                    b.kind = static_cast<BrainKind>(oldVal);
                                    b.kindInt = oldVal;
                                }
                            },
                            "Change Brain Kind"
                        );
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
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
                            /*if (restored)
                            {
                                ENGINE_PRINT("Restored pendingInstanceState for ", scriptData.scriptPath.c_str());
                            }
                            else
                            {
                                ENGINE_PRINT("Failed to restore pendingInstanceState for ", scriptData.scriptPath.c_str());
                            }*/
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
                                // Lua table - convert to JSON
                                syntheticField.type = Scripting::FieldType::Table;
                                syntheticField.defaultValueSerialized = ConvertLuaTableToJson(luaDefault);
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
                                // Generic string array - render with +/- buttons like Unity
                                renderLabelWithTooltip();
                                bool arrayModified = false;
                                rapidjson::Document newDoc;
                                newDoc.SetArray();
                                auto& alloc = newDoc.GetAllocator();

                                // Static buffers for string editing (keyed by field name + index)
                                static std::unordered_map<std::string, std::vector<char>> stringArrayBuffers;

                                for (size_t i = 0; i < doc.Size(); ++i)
                                {
                                    ImGui::PushID(static_cast<int>(i));

                                    std::string currentValue;
                                    if (doc[i].IsString())
                                    {
                                        currentValue = doc[i].GetString();
                                    }
                                    else if (doc[i].IsNumber())
                                    {
                                        currentValue = std::to_string(doc[i].GetDouble());
                                    }
                                    else
                                    {
                                        currentValue = "";
                                    }

                                    // Create a unique key for this field+index buffer
                                    std::string bufferKey = field.name + "_" + std::to_string(i);
                                    auto& buffer = stringArrayBuffers[bufferKey];
                                    if (buffer.size() < 256) buffer.resize(256);

                                    // Initialize buffer with current value if it's empty or different
                                    if (buffer[0] == '\0' || std::string(buffer.data()) != currentValue) {
                                        size_t copyLen = std::min(currentValue.size(), size_t(255));
                                        std::memcpy(buffer.data(), currentValue.c_str(), copyLen);
                                        buffer[copyLen] = '\0';
                                    }

                                    ImGui::Text("[%zu]", i + 1);
                                    ImGui::SameLine();
                                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);

                                    if (ImGui::InputText(("##str" + std::to_string(i)).c_str(), buffer.data(), 256))
                                    {
                                        arrayModified = true;
                                    }

                                    ImGui::SameLine();
                                    if (ImGui::SmallButton((std::string(ICON_FA_MINUS) + "##remove" + std::to_string(i)).c_str()))
                                    {
                                        // Skip this element (remove it)
                                        arrayModified = true;
                                        // Clear the buffer for this removed element
                                        stringArrayBuffers.erase(bufferKey);
                                    }
                                    else
                                    {
                                        // Add to new array (use buffer content)
                                        newDoc.PushBack(rapidjson::Value(buffer.data(), alloc), alloc);
                                    }

                                    ImGui::PopID();
                                }

                                // Add new element button
                                if (ImGui::Button((std::string(ICON_FA_PLUS) + "##add_" + field.name).c_str()))
                                {
                                    newDoc.PushBack(rapidjson::Value("", alloc), alloc);
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
                                    // Generic string array-like table - render with +/- buttons
                                    renderLabelWithTooltip();
                                    bool arrayModified = false;
                                    rapidjson::Document newDoc;
                                    newDoc.SetArray();
                                    auto& alloc = newDoc.GetAllocator();

                                    // Static buffers for string editing
                                    static std::unordered_map<std::string, std::vector<char>> stringArrayTableBuffers;

                                    size_t arraySize = doc.GetObject().MemberCount();
                                    for (size_t i = 0; i < arraySize; ++i)
                                    {
                                        std::string key = std::to_string(i + 1);
                                        auto it = doc.FindMember(key.c_str());
                                        if (it == doc.MemberEnd()) continue;

                                        ImGui::PushID(static_cast<int>(i));

                                        std::string currentValue;
                                        if (it->value.IsString())
                                        {
                                            currentValue = it->value.GetString();
                                        }
                                        else if (it->value.IsNumber())
                                        {
                                            currentValue = std::to_string(it->value.GetDouble());
                                        }
                                        else
                                        {
                                            currentValue = "";
                                        }

                                        // Create a unique key for this field+index buffer
                                        std::string bufferKey = field.name + "_tbl_" + std::to_string(i);
                                        auto& buffer = stringArrayTableBuffers[bufferKey];
                                        if (buffer.size() < 256) buffer.resize(256);

                                        // Initialize buffer
                                        if (buffer[0] == '\0' || std::string(buffer.data()) != currentValue) {
                                            size_t copyLen = std::min(currentValue.size(), size_t(255));
                                            std::memcpy(buffer.data(), currentValue.c_str(), copyLen);
                                            buffer[copyLen] = '\0';
                                        }

                                        ImGui::Text("[%zu]", i + 1);
                                        ImGui::SameLine();
                                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);

                                        if (ImGui::InputText(("##str_tbl" + std::to_string(i)).c_str(), buffer.data(), 256))
                                        {
                                            arrayModified = true;
                                        }

                                        ImGui::SameLine();
                                        if (ImGui::SmallButton((std::string(ICON_FA_MINUS) + "##remove_tbl" + std::to_string(i)).c_str()))
                                        {
                                            arrayModified = true;
                                            stringArrayTableBuffers.erase(bufferKey);
                                        }
                                        else
                                        {
                                            newDoc.PushBack(rapidjson::Value(buffer.data(), alloc), alloc);
                                        }

                                        ImGui::PopID();
                                    }

                                    // Add new element button
                                    if (ImGui::Button((std::string(ICON_FA_PLUS) + "##add_tbl_" + field.name).c_str()))
                                    {
                                        newDoc.PushBack(rapidjson::Value("", alloc), alloc);
                                        arrayModified = true;
                                    }

                                    if (arrayModified)
                                    {
                                        rapidjson::StringBuffer buffer;
                                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                                        newDoc.Accept(writer);
                                        newValue = buffer.GetString();
                                        fieldModified = true;
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
    // Uses entity-aware lambda commands for proper undo/redo
    ReflectionRenderer::RegisterComponentRenderer("ButtonComponent",
    [](void* componentPtr, TypeDescriptor_Struct*, Entity entity, ECSManager& ecs) -> bool
    {
        ButtonComponent& buttonComp = *static_cast<ButtonComponent*>(componentPtr);

        // Static tracking for entity-aware undo
        static std::unordered_map<Entity, bool> startInteractable;
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

        // Interactable toggle with entity-aware undo
        ImGui::Text("Interactable");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        startInteractable[entity] = buttonComp.interactable;
        bool interactableVal = buttonComp.interactable;
        if (ImGui::Checkbox("##Interactable", &interactableVal)) {
            bool oldVal = startInteractable[entity];
            bool newVal = interactableVal;
            buttonComp.interactable = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<ButtonComponent>(entity)) {
                            ecs.GetComponent<ButtonComponent>(entity).interactable = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<ButtonComponent>(entity)) {
                            ecs.GetComponent<ButtonComponent>(entity).interactable = oldVal;
                        }
                    },
                    "Toggle Button Interactable"
                );
            }
        }

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
    // Uses entity-aware lambda commands for proper undo/redo
    ReflectionRenderer::RegisterComponentRenderer("SliderComponent",
    [](void* componentPtr, TypeDescriptor_Struct*, Entity entity, ECSManager& ecs) -> bool
    {
        SliderComponent& sliderComp = *static_cast<SliderComponent*>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, float> startMinValue;
        static std::unordered_map<Entity, bool> isEditingMinValue;
        static std::unordered_map<Entity, float> startMaxValue;
        static std::unordered_map<Entity, bool> isEditingMaxValue;
        static std::unordered_map<Entity, float> startValue;
        static std::unordered_map<Entity, bool> isEditingValue;
        static std::unordered_map<Entity, bool> startWholeNumbers;
        static std::unordered_map<Entity, bool> startInteractable;
        static std::unordered_map<Entity, bool> startHorizontal;

        // Initialize tracking state
        if (isEditingMinValue.find(entity) == isEditingMinValue.end()) isEditingMinValue[entity] = false;
        if (isEditingMaxValue.find(entity) == isEditingMaxValue.end()) isEditingMaxValue[entity] = false;
        if (isEditingValue.find(entity) == isEditingValue.end()) isEditingValue[entity] = false;

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

        // Core slider fields with entity-aware undo
        // Min Value
        ImGui::Text("Min Value");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingMinValue[entity]) startMinValue[entity] = sliderComp.minValue;
        if (ImGui::IsItemActivated()) { startMinValue[entity] = sliderComp.minValue; isEditingMinValue[entity] = true; }
        if (ImGui::DragFloat("##SliderMin", &sliderComp.minValue, 0.1f)) {
            isEditingMinValue[entity] = true;
        }
        if (isEditingMinValue[entity] && !ImGui::IsItemActive()) {
            float oldVal = startMinValue[entity];
            float newVal = sliderComp.minValue;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).minValue = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).minValue = oldVal;
                        }
                    },
                    "Change Slider Min Value"
                );
            }
            isEditingMinValue[entity] = false;
        }

        // Max Value
        ImGui::Text("Max Value");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingMaxValue[entity]) startMaxValue[entity] = sliderComp.maxValue;
        if (ImGui::IsItemActivated()) { startMaxValue[entity] = sliderComp.maxValue; isEditingMaxValue[entity] = true; }
        if (ImGui::DragFloat("##SliderMax", &sliderComp.maxValue, 0.1f)) {
            isEditingMaxValue[entity] = true;
        }
        if (isEditingMaxValue[entity] && !ImGui::IsItemActive()) {
            float oldVal = startMaxValue[entity];
            float newVal = sliderComp.maxValue;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).maxValue = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).maxValue = oldVal;
                        }
                    },
                    "Change Slider Max Value"
                );
            }
            isEditingMaxValue[entity] = false;
        }

        if (sliderComp.maxValue < sliderComp.minValue) {
            std::swap(sliderComp.maxValue, sliderComp.minValue);
        }

        // Value
        ImGui::Text("Value");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingValue[entity]) startValue[entity] = sliderComp.value;
        if (ImGui::IsItemActivated()) { startValue[entity] = sliderComp.value; isEditingValue[entity] = true; }
        if (ImGui::DragFloat("##SliderValue", &sliderComp.value, 0.1f, sliderComp.minValue, sliderComp.maxValue)) {
            isEditingValue[entity] = true;
        }
        if (isEditingValue[entity] && !ImGui::IsItemActive()) {
            float oldVal = startValue[entity];
            float newVal = sliderComp.value;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).value = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).value = oldVal;
                        }
                    },
                    "Change Slider Value"
                );
            }
            isEditingValue[entity] = false;
        }

        sliderComp.value = std::max(sliderComp.minValue, std::min(sliderComp.maxValue, sliderComp.value));
        if (sliderComp.wholeNumbers) {
            sliderComp.value = std::round(sliderComp.value);
        }

        // Whole Numbers checkbox
        ImGui::Text("Whole Numbers");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        startWholeNumbers[entity] = sliderComp.wholeNumbers;
        bool wholeNumVal = sliderComp.wholeNumbers;
        if (ImGui::Checkbox("##SliderWhole", &wholeNumVal)) {
            bool oldVal = startWholeNumbers[entity];
            bool newVal = wholeNumVal;
            sliderComp.wholeNumbers = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).wholeNumbers = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).wholeNumbers = oldVal;
                        }
                    },
                    "Toggle Slider Whole Numbers"
                );
            }
        }

        // Interactable checkbox
        ImGui::Text("Interactable");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        startInteractable[entity] = sliderComp.interactable;
        bool interactableVal = sliderComp.interactable;
        if (ImGui::Checkbox("##SliderInteractable", &interactableVal)) {
            bool oldVal = startInteractable[entity];
            bool newVal = interactableVal;
            sliderComp.interactable = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).interactable = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).interactable = oldVal;
                        }
                    },
                    "Toggle Slider Interactable"
                );
            }
        }

        // Horizontal checkbox
        ImGui::Text("Horizontal");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        startHorizontal[entity] = sliderComp.horizontal;
        bool horizontalVal = sliderComp.horizontal;
        if (ImGui::Checkbox("##SliderHorizontal", &horizontalVal)) {
            bool oldVal = startHorizontal[entity];
            bool newVal = horizontalVal;
            sliderComp.horizontal = newVal;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).horizontal = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<SliderComponent>(entity)) {
                            ecs.GetComponent<SliderComponent>(entity).horizontal = oldVal;
                        }
                    },
                    "Toggle Slider Horizontal"
                );
            }
        }

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
    // Uses entity-aware lambda commands for proper undo/redo
    ReflectionRenderer::RegisterComponentRenderer("UIAnchorComponent",
    [](void* componentPtr, TypeDescriptor_Struct*, Entity entity, ECSManager& ecs) -> bool
    {
        UIAnchorComponent& anchor = *static_cast<UIAnchorComponent*>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Static tracking maps for entity-aware undo
        static std::unordered_map<Entity, int> startPreset;
        static std::unordered_map<Entity, float> startAnchorX;
        static std::unordered_map<Entity, bool> isEditingAnchorX;
        static std::unordered_map<Entity, float> startAnchorY;
        static std::unordered_map<Entity, bool> isEditingAnchorY;
        static std::unordered_map<Entity, float> startOffsetX;
        static std::unordered_map<Entity, bool> isEditingOffsetX;
        static std::unordered_map<Entity, float> startOffsetY;
        static std::unordered_map<Entity, bool> isEditingOffsetY;
        static std::unordered_map<Entity, int> startSizeMode;
        static std::unordered_map<Entity, float> startMarginLeft;
        static std::unordered_map<Entity, bool> isEditingMarginLeft;
        static std::unordered_map<Entity, float> startMarginRight;
        static std::unordered_map<Entity, bool> isEditingMarginRight;
        static std::unordered_map<Entity, float> startMarginTop;
        static std::unordered_map<Entity, bool> isEditingMarginTop;
        static std::unordered_map<Entity, float> startMarginBottom;
        static std::unordered_map<Entity, bool> isEditingMarginBottom;
        static std::unordered_map<Entity, float> startRefWidth;
        static std::unordered_map<Entity, bool> isEditingRefWidth;
        static std::unordered_map<Entity, float> startRefHeight;
        static std::unordered_map<Entity, bool> isEditingRefHeight;

        // Initialize tracking state
        if (isEditingAnchorX.find(entity) == isEditingAnchorX.end()) isEditingAnchorX[entity] = false;
        if (isEditingAnchorY.find(entity) == isEditingAnchorY.end()) isEditingAnchorY[entity] = false;
        if (isEditingOffsetX.find(entity) == isEditingOffsetX.end()) isEditingOffsetX[entity] = false;
        if (isEditingOffsetY.find(entity) == isEditingOffsetY.end()) isEditingOffsetY[entity] = false;
        if (isEditingMarginLeft.find(entity) == isEditingMarginLeft.end()) isEditingMarginLeft[entity] = false;
        if (isEditingMarginRight.find(entity) == isEditingMarginRight.end()) isEditingMarginRight[entity] = false;
        if (isEditingMarginTop.find(entity) == isEditingMarginTop.end()) isEditingMarginTop[entity] = false;
        if (isEditingMarginBottom.find(entity) == isEditingMarginBottom.end()) isEditingMarginBottom[entity] = false;
        if (isEditingRefWidth.find(entity) == isEditingRefWidth.end()) isEditingRefWidth[entity] = false;
        if (isEditingRefHeight.find(entity) == isEditingRefHeight.end()) isEditingRefHeight[entity] = false;

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
        startPreset[entity] = currentPreset;
        if (ImGui::Combo("##AnchorPreset", &currentPreset, presetNames, IM_ARRAYSIZE(presetNames))) {
            int oldVal = startPreset[entity];
            int newVal = currentPreset;
            anchor.SetPreset(static_cast<UIAnchorPreset>(newVal));
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).SetPreset(static_cast<UIAnchorPreset>(newVal));
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).SetPreset(static_cast<UIAnchorPreset>(oldVal));
                        }
                    },
                    "Change Anchor Preset"
                );
            }
        }

        ImGui::Separator();

        // Anchor X
        ImGui::Text("Anchor X");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingAnchorX[entity]) startAnchorX[entity] = anchor.anchorX;
        if (ImGui::IsItemActivated()) { startAnchorX[entity] = anchor.anchorX; isEditingAnchorX[entity] = true; }
        if (ImGui::SliderFloat("##AnchorX", &anchor.anchorX, 0.0f, 1.0f, "%.2f")) {
            isEditingAnchorX[entity] = true;
        }
        if (isEditingAnchorX[entity] && !ImGui::IsItemActive()) {
            float oldVal = startAnchorX[entity];
            float newVal = anchor.anchorX;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).anchorX = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).anchorX = oldVal;
                        }
                    },
                    "Change Anchor X"
                );
            }
            isEditingAnchorX[entity] = false;
        }

        // Anchor Y
        ImGui::Text("Anchor Y");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingAnchorY[entity]) startAnchorY[entity] = anchor.anchorY;
        if (ImGui::IsItemActivated()) { startAnchorY[entity] = anchor.anchorY; isEditingAnchorY[entity] = true; }
        if (ImGui::SliderFloat("##AnchorY", &anchor.anchorY, 0.0f, 1.0f, "%.2f")) {
            isEditingAnchorY[entity] = true;
        }
        if (isEditingAnchorY[entity] && !ImGui::IsItemActive()) {
            float oldVal = startAnchorY[entity];
            float newVal = anchor.anchorY;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).anchorY = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).anchorY = oldVal;
                        }
                    },
                    "Change Anchor Y"
                );
            }
            isEditingAnchorY[entity] = false;
        }

        ImGui::Separator();

        // Offset X
        ImGui::Text("Offset X");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingOffsetX[entity]) startOffsetX[entity] = anchor.offsetX;
        if (ImGui::IsItemActivated()) { startOffsetX[entity] = anchor.offsetX; isEditingOffsetX[entity] = true; }
        if (ImGui::DragFloat("##OffsetX", &anchor.offsetX, 1.0f, -10000.0f, 10000.0f, "%.1f")) {
            isEditingOffsetX[entity] = true;
        }
        if (isEditingOffsetX[entity] && !ImGui::IsItemActive()) {
            float oldVal = startOffsetX[entity];
            float newVal = anchor.offsetX;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).offsetX = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).offsetX = oldVal;
                        }
                    },
                    "Change Offset X"
                );
            }
            isEditingOffsetX[entity] = false;
        }

        // Offset Y
        ImGui::Text("Offset Y");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (!isEditingOffsetY[entity]) startOffsetY[entity] = anchor.offsetY;
        if (ImGui::IsItemActivated()) { startOffsetY[entity] = anchor.offsetY; isEditingOffsetY[entity] = true; }
        if (ImGui::DragFloat("##OffsetY", &anchor.offsetY, 1.0f, -10000.0f, 10000.0f, "%.1f")) {
            isEditingOffsetY[entity] = true;
        }
        if (isEditingOffsetY[entity] && !ImGui::IsItemActive()) {
            float oldVal = startOffsetY[entity];
            float newVal = anchor.offsetY;
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).offsetY = newVal;
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).offsetY = oldVal;
                        }
                    },
                    "Change Offset Y"
                );
            }
            isEditingOffsetY[entity] = false;
        }

        ImGui::Separator();

        // Size Mode dropdown
        ImGui::Text("Size Mode");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char* sizeModeNames[] = {
            "Fixed", "Stretch X", "Stretch Y", "Stretch Both", "Scale Uniform"
        };
        int currentSizeMode = static_cast<int>(anchor.sizeMode);
        startSizeMode[entity] = currentSizeMode;
        if (ImGui::Combo("##SizeMode", &currentSizeMode, sizeModeNames, IM_ARRAYSIZE(sizeModeNames))) {
            int oldVal = startSizeMode[entity];
            int newVal = currentSizeMode;
            anchor.sizeMode = static_cast<UISizeMode>(newVal);
            if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordLambdaChange(
                    [entity, newVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).sizeMode = static_cast<UISizeMode>(newVal);
                        }
                    },
                    [entity, oldVal, &ecs]() {
                        if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                            ecs.GetComponent<UIAnchorComponent>(entity).sizeMode = static_cast<UISizeMode>(oldVal);
                        }
                    },
                    "Change Size Mode"
                );
            }
        }

        // Show margins for stretch modes
        if (anchor.sizeMode == UISizeMode::StretchX ||
            anchor.sizeMode == UISizeMode::StretchY ||
            anchor.sizeMode == UISizeMode::StretchBoth) {

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Margins");

            // Margin Left
            ImGui::Text("Left");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            if (!isEditingMarginLeft[entity]) startMarginLeft[entity] = anchor.marginLeft;
            if (ImGui::IsItemActivated()) { startMarginLeft[entity] = anchor.marginLeft; isEditingMarginLeft[entity] = true; }
            if (ImGui::DragFloat("##MarginLeft", &anchor.marginLeft, 1.0f, 0.0f, 10000.0f, "%.0f")) {
                isEditingMarginLeft[entity] = true;
            }
            if (isEditingMarginLeft[entity] && !ImGui::IsItemActive()) {
                float oldVal = startMarginLeft[entity];
                float newVal = anchor.marginLeft;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginLeft = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginLeft = oldVal;
                            }
                        },
                        "Change Margin Left"
                    );
                }
                isEditingMarginLeft[entity] = false;
            }

            // Margin Right
            ImGui::Text("Right");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            if (!isEditingMarginRight[entity]) startMarginRight[entity] = anchor.marginRight;
            if (ImGui::IsItemActivated()) { startMarginRight[entity] = anchor.marginRight; isEditingMarginRight[entity] = true; }
            if (ImGui::DragFloat("##MarginRight", &anchor.marginRight, 1.0f, 0.0f, 10000.0f, "%.0f")) {
                isEditingMarginRight[entity] = true;
            }
            if (isEditingMarginRight[entity] && !ImGui::IsItemActive()) {
                float oldVal = startMarginRight[entity];
                float newVal = anchor.marginRight;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginRight = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginRight = oldVal;
                            }
                        },
                        "Change Margin Right"
                    );
                }
                isEditingMarginRight[entity] = false;
            }

            // Margin Top
            ImGui::Text("Top");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            if (!isEditingMarginTop[entity]) startMarginTop[entity] = anchor.marginTop;
            if (ImGui::IsItemActivated()) { startMarginTop[entity] = anchor.marginTop; isEditingMarginTop[entity] = true; }
            if (ImGui::DragFloat("##MarginTop", &anchor.marginTop, 1.0f, 0.0f, 10000.0f, "%.0f")) {
                isEditingMarginTop[entity] = true;
            }
            if (isEditingMarginTop[entity] && !ImGui::IsItemActive()) {
                float oldVal = startMarginTop[entity];
                float newVal = anchor.marginTop;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginTop = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginTop = oldVal;
                            }
                        },
                        "Change Margin Top"
                    );
                }
                isEditingMarginTop[entity] = false;
            }

            // Margin Bottom
            ImGui::Text("Bottom");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            if (!isEditingMarginBottom[entity]) startMarginBottom[entity] = anchor.marginBottom;
            if (ImGui::IsItemActivated()) { startMarginBottom[entity] = anchor.marginBottom; isEditingMarginBottom[entity] = true; }
            if (ImGui::DragFloat("##MarginBottom", &anchor.marginBottom, 1.0f, 0.0f, 10000.0f, "%.0f")) {
                isEditingMarginBottom[entity] = true;
            }
            if (isEditingMarginBottom[entity] && !ImGui::IsItemActive()) {
                float oldVal = startMarginBottom[entity];
                float newVal = anchor.marginBottom;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginBottom = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).marginBottom = oldVal;
                            }
                        },
                        "Change Margin Bottom"
                    );
                }
                isEditingMarginBottom[entity] = false;
            }
        }

        // Show reference resolution for ScaleUniform mode
        if (anchor.sizeMode == UISizeMode::ScaleUniform) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Reference Resolution");

            // Ref Width
            ImGui::Text("Width");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            if (!isEditingRefWidth[entity]) startRefWidth[entity] = anchor.referenceWidth;
            if (ImGui::IsItemActivated()) { startRefWidth[entity] = anchor.referenceWidth; isEditingRefWidth[entity] = true; }
            if (ImGui::DragFloat("##RefWidth", &anchor.referenceWidth, 1.0f, 1.0f, 10000.0f, "%.0f")) {
                isEditingRefWidth[entity] = true;
            }
            if (isEditingRefWidth[entity] && !ImGui::IsItemActive()) {
                float oldVal = startRefWidth[entity];
                float newVal = anchor.referenceWidth;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).referenceWidth = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).referenceWidth = oldVal;
                            }
                        },
                        "Change Reference Width"
                    );
                }
                isEditingRefWidth[entity] = false;
            }

            // Ref Height
            ImGui::Text("Height");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            if (!isEditingRefHeight[entity]) startRefHeight[entity] = anchor.referenceHeight;
            if (ImGui::IsItemActivated()) { startRefHeight[entity] = anchor.referenceHeight; isEditingRefHeight[entity] = true; }
            if (ImGui::DragFloat("##RefHeight", &anchor.referenceHeight, 1.0f, 1.0f, 10000.0f, "%.0f")) {
                isEditingRefHeight[entity] = true;
            }
            if (isEditingRefHeight[entity] && !ImGui::IsItemActive()) {
                float oldVal = startRefHeight[entity];
                float newVal = anchor.referenceHeight;
                if (oldVal != newVal && UndoSystem::GetInstance().IsEnabled()) {
                    UndoSystem::GetInstance().RecordLambdaChange(
                        [entity, newVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).referenceHeight = newVal;
                            }
                        },
                        [entity, oldVal, &ecs]() {
                            if (ecs.HasComponent<UIAnchorComponent>(entity)) {
                                ecs.GetComponent<UIAnchorComponent>(entity).referenceHeight = oldVal;
                            }
                        },
                        "Change Reference Height"
                    );
                }
                isEditingRefHeight[entity] = false;
            }
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
