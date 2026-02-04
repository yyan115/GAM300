/* Start Header ************************************************************************/
/*!
\file       ReflectionRenderer.cpp
\author     Lucas Yee
\date       2025
\brief      Implementation of automatic ImGui rendering for reflected components.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "ReflectionRenderer.hpp"
#include "ECS/ECSManager.hpp"
#include "Math/Vector3D.hpp"
#include "Transform/Quaternion.hpp"
#include "Utilities/GUID.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "UndoableWidgets.hpp"
#include "EditorComponents.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <Panels/PrefabEditorPanel.hpp>

// Initialize static registries
std::unordered_map<std::string, ReflectionRenderer::CustomFieldRenderer>&
ReflectionRenderer::GetCustomRenderers() {
    static std::unordered_map<std::string, CustomFieldRenderer> customRenderers;
    return customRenderers;
}

std::unordered_map<std::string, ReflectionRenderer::CustomFieldRenderer>&
ReflectionRenderer::GetFieldRenderers() {
    static std::unordered_map<std::string, CustomFieldRenderer> fieldRenderers;
    return fieldRenderers;
}

std::unordered_map<std::string, ReflectionRenderer::CustomComponentRenderer>&
ReflectionRenderer::GetComponentRenderers() {
    static std::unordered_map<std::string, CustomComponentRenderer> componentRenderers;
    return componentRenderers;
}

void ReflectionRenderer::RegisterCustomRenderer(const std::string& typeName,
                                                CustomFieldRenderer renderer) {
    GetCustomRenderers()[typeName] = renderer;
}

void ReflectionRenderer::RegisterFieldRenderer(const std::string& componentType,
                                               const std::string& fieldName,
                                               CustomFieldRenderer renderer) {
    GetFieldRenderers()[componentType + "::" + fieldName] = renderer;
}

void ReflectionRenderer::RegisterComponentRenderer(const std::string& componentType,
                                                    CustomComponentRenderer renderer) {
    GetComponentRenderers()[componentType] = renderer;
}

std::string ReflectionRenderer::MakeFieldID(const char* fieldName, const void* fieldPtr) {
    std::stringstream ss;
    ss << "##" << fieldName << "_" << fieldPtr;
    return ss.str();
}

bool ReflectionRenderer::RenderComponent(void* componentPtr, TypeDescriptor_Struct* typeDesc,
                                         Entity entity, ECSManager& ecsManager) {
    // Delegate to the field-tracking version and just return the bool
    FieldModificationResult result = RenderComponentWithFieldTracking(componentPtr, typeDesc, entity, ecsManager);
    return result.wasModified;
}

FieldModificationResult ReflectionRenderer::RenderComponentWithFieldTracking(void* componentPtr, TypeDescriptor_Struct* typeDesc,
                                                                              Entity entity, ECSManager& ecsManager) {
    FieldModificationResult result;
    if (!componentPtr || !typeDesc) return result;

    // Check for custom component renderer
    std::string componentType = typeDesc->GetName();
    auto& componentRenderers = GetComponentRenderers();
    if (componentRenderers.find(componentType) != componentRenderers.end()) {
        bool skipDefaultRendering = componentRenderers[componentType](componentPtr, typeDesc, entity, ecsManager);
        if (skipDefaultRendering) {
            return result;  // Custom renderer handled everything, skip default field rendering
        }
        // If it returns false, continue with default field rendering below
    }

    std::vector<TypeDescriptor_Struct::Member> members = typeDesc->GetMembers();

    for (const auto& member : members) {
        // Skip special/internal fields
        std::string memberName = member.name;
        if (memberName == "overrideFromPrefab") continue;  // Handled separately
        if (memberName == "version") continue;              // Internal version counter
        if (memberName == "isDirty") continue;              // Internal dirty flag
        if (memberName == "worldMatrix") continue;          // Computed matrix, not editable
        if (memberName == "transform") continue;            // ModelRenderComponent internal transform reference
        if (memberName == "layer") continue;                // Jolt internal (use layerID)
        if (memberName == "shapeType") continue;            // Use shapeTypeID instead
        if (memberName == "shape") continue;                // Jolt internal shape pointer
        if (memberName == "sphereRadius") continue;         // Handled by shapeTypeID custom renderer
        if (memberName == "capsuleRadius") continue;        // Handled by shapeTypeID custom renderer
        if (memberName == "capsuleHalfHeight") continue;    // Handled by shapeTypeID custom renderer
        if (memberName == "cylinderRadius") continue;       // Handled by shapeTypeID custom renderer
        if (memberName == "cylinderHalfHeight") continue;   // Handled by shapeTypeID custom renderer
        if (memberName == "isShaking") continue;            // Camera internal shake state
        if (memberName == "shakeTimer") continue;           // Camera internal shake timer
        if (memberName == "shakeOffset") continue;          // Camera internal shake offset

        // Get field pointer
        void* fieldPtr = member.get_ptr(componentPtr);
        if (!fieldPtr) continue;

        // Check for custom field renderer (component::field specific)
        componentType = typeDesc->GetName();
        std::string fieldKey = componentType + "::" + memberName;
        auto& fieldRenderers = GetFieldRenderers();
        if (fieldRenderers.find(fieldKey) != fieldRenderers.end()) {
            // Custom field renderer handles its own undo/redo via UndoableWidgets
            bool fieldModified = fieldRenderers[fieldKey](member.name, fieldPtr, entity, ecsManager);
            if (fieldModified) {
                result.wasModified = true;
                result.modifiedFieldName = memberName;
                // Continue rendering other fields but we already know which one was modified
            }
            continue;
        }

        // Render using type-based renderer (has its own snapshot handling in RenderField)
        bool fieldModified = RenderField(member.name, fieldPtr, member.type, entity, ecsManager);
        if (fieldModified) {
            result.wasModified = true;
            result.modifiedFieldName = memberName;
            // Continue rendering other fields but we already know which one was modified
        }
    }

    if (result.wasModified && PrefabEditor::IsInPrefabEditorMode()) {
		ENGINE_LOG_DEBUG("[ReflectionRenderer] Modified prefab component, saving prefab changes...");
        PrefabEditor::SaveEditedPrefab();
    }

    return result;
}

bool ReflectionRenderer::RenderField(const char* fieldName, void* fieldPtr,
                                     TypeDescriptor* fieldType, Entity entity,
                                     ECSManager& ecsManager) {
    if (!fieldPtr || !fieldType) return false;

    std::string typeName = fieldType->GetName();
    bool modified = false;

    // Check for custom type renderer
    auto& customRenderers = GetCustomRenderers();
    if (customRenderers.find(typeName) != customRenderers.end()) {
        modified = customRenderers[typeName](fieldName, fieldPtr, entity, ecsManager);
    }
    // Handle common types
    else if (typeName == "bool" || typeName == "int" || typeName == "unsigned" ||
        typeName == "float" || typeName == "double" ||
        typeName == "int64_t" || typeName == "uint64_t") {
        modified = RenderPrimitive(fieldName, fieldPtr, typeName);
    }
    else if (typeName == "std::string") {
        modified = RenderString(fieldName, fieldPtr);
    }
    else if (typeName == "Vector3D") {
        modified = RenderVector3D(fieldName, fieldPtr);
    }
    else if (typeName == "Quaternion") {
        modified = RenderQuaternion(fieldName, fieldPtr);
    }
    else if (typeName == "GUID_128") {
        modified = RenderGUID(fieldName, fieldPtr, entity, ecsManager);
    }
    else {
        // Try to render as struct (nested reflection)
        TypeDescriptor_Struct* structType = dynamic_cast<TypeDescriptor_Struct*>(fieldType);
        if (structType) {
            modified = RenderStruct(fieldName, fieldPtr, fieldType, entity, ecsManager);
        }
        else {
            // Unknown type - just show type name
            ImGui::Text("%s", fieldName);
            ImGui::SameLine();
            ImGui::TextDisabled("(%s - not rendered)", typeName.c_str());
        }
    }

    // Snapshots are now handled inside each widget renderer (RenderPrimitive, etc.)
    // This ensures we capture the OLD value before modification, not after

    return modified;
}

// Helper function to convert camelCase to "Proper Case"
static std::string CamelCaseToProperCase(const std::string& fieldName) {
    if (fieldName.empty()) return fieldName;

    std::string result;
    result += static_cast<char>(std::toupper(fieldName[0])); // Capitalize first letter

    for (size_t i = 1; i < fieldName.size(); ++i) {
        char c = fieldName[i];

        // Insert space before uppercase letters (but not for consecutive uppercase like "GUID")
        if (std::isupper(c) && i > 0 && std::islower(fieldName[i-1])) {
            result += ' ';
        }

        result += c;
    }

    return result;
}

bool ReflectionRenderer::RenderPrimitive(const char* fieldName, void* fieldPtr,
                                         const std::string& typeName) {
    std::string displayName = CamelCaseToProperCase(fieldName);
    std::string id = MakeFieldID(fieldName, fieldPtr);
    const float labelWidth = EditorComponents::GetLabelWidth();

    if (typeName == "bool") {
        bool* value = static_cast<bool*>(fieldPtr);

        // Align text to frame padding for consistent alignment
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", displayName.c_str());
        ImGui::SameLine(labelWidth);

        // Use UndoableWidgets wrapper for automatic undo/redo
        return UndoableWidgets::Checkbox(id.c_str(), value);
    }

    // All other primitives: Label on left, widget on right
    ImGui::Text("%s", displayName.c_str());
    ImGui::SameLine(labelWidth);
    ImGui::SetNextItemWidth(-1);

    // Use UndoableWidgets wrappers for automatic undo/redo
    if (typeName == "int") {
        int* value = static_cast<int*>(fieldPtr);
        return UndoableWidgets::DragInt(id.c_str(), value, 1.0f);
    }
    else if (typeName == "unsigned") {
        unsigned* value = static_cast<unsigned*>(fieldPtr);
        int temp = static_cast<int>(*value);
        if (UndoableWidgets::DragInt(id.c_str(), &temp, 1.0f, 0, INT_MAX)) {
            *value = static_cast<unsigned>(temp);
            return true;
        }
        return false;
    }
    else if (typeName == "float") {
        float* value = static_cast<float*>(fieldPtr);
        return UndoableWidgets::DragFloat(id.c_str(), value, 0.01f);
    }
    else if (typeName == "double") {
        double* value = static_cast<double*>(fieldPtr);
        float temp = static_cast<float>(*value);
        if (UndoableWidgets::DragFloat(id.c_str(), &temp, 0.01f)) {
            *value = static_cast<double>(temp);
            return true;
        }
        return false;
    }
    else if (typeName == "int64_t") {
        int64_t* value = static_cast<int64_t*>(fieldPtr);
        int temp = static_cast<int>(*value);
        if (UndoableWidgets::DragInt(id.c_str(), &temp)) {
            *value = static_cast<int64_t>(temp);
            return true;
        }
        return false;
    }
    else if (typeName == "uint64_t") {
        uint64_t* value = static_cast<uint64_t*>(fieldPtr);
        int temp = static_cast<int>(*value);
        if (UndoableWidgets::DragInt(id.c_str(), &temp, 1.0f, 0, INT_MAX)) {
            *value = static_cast<uint64_t>(temp);
            return true;
        }
        return false;
    }

    return false;
}

bool ReflectionRenderer::RenderVector3D(const char* fieldName, void* fieldPtr) {
    Vector3D* vec = static_cast<Vector3D*>(fieldPtr);

    std::string displayName = CamelCaseToProperCase(fieldName);
    const float labelWidth = EditorComponents::GetLabelWidth();
    ImGui::Text("%s", displayName.c_str());
    ImGui::SameLine(labelWidth);
    ImGui::SetNextItemWidth(-1);

    float values[3] = { vec->x, vec->y, vec->z };
    std::string id = MakeFieldID(fieldName, fieldPtr);

    if (UndoableWidgets::DragFloat3(id.c_str(), values, 0.1f)) {
        vec->x = values[0];
        vec->y = values[1];
        vec->z = values[2];
        return true;
    }

    return false;
}

bool ReflectionRenderer::RenderQuaternion(const char* fieldName, void* fieldPtr) {
    Quaternion* quat = static_cast<Quaternion*>(fieldPtr);

    // Convert to Euler angles for editing
    Vector3D euler = quat->ToEulerDegrees();

    std::string displayName = CamelCaseToProperCase(fieldName);
    const float labelWidth = EditorComponents::GetLabelWidth();
    ImGui::Text("%s (Euler)", displayName.c_str());
    ImGui::SameLine(labelWidth);
    ImGui::SetNextItemWidth(-1);

    float values[3] = { euler.x, euler.y, euler.z };
    std::string id = MakeFieldID(fieldName, fieldPtr);

    if (UndoableWidgets::DragFloat3(id.c_str(), values, 1.0f, -180.0f, 180.0f, "%.1f")) {
        *quat = Quaternion::FromEulerDegrees(Vector3D(values[0], values[1], values[2]));
        return true;
    }

    return false;
}

bool ReflectionRenderer::RenderGUID(const char* fieldName, void* fieldPtr,
                                    Entity, ECSManager&) {
    GUID_128* guid = static_cast<GUID_128*>(fieldPtr);

    std::string displayName = CamelCaseToProperCase(fieldName);
    const float labelWidth = EditorComponents::GetLabelWidth();
    ImGui::Text("%s", displayName.c_str());
    ImGui::SameLine(labelWidth);

    // Display GUID as hex string (read-only for now)
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(16) << guid->high
       << std::setw(16) << guid->low;

    std::string guidStr = ss.str();
    if (guidStr.length() > 16) {
        guidStr = guidStr.substr(0, 8) + "..." + guidStr.substr(guidStr.length() - 8);
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::TextDisabled("%s", guidStr.c_str());

    // TODO: Add drag-drop support for asset GUIDs
    return false;
}

bool ReflectionRenderer::RenderString(const char* fieldName, void* fieldPtr) {
    std::string* str = static_cast<std::string*>(fieldPtr);

    std::string displayName = CamelCaseToProperCase(fieldName);
    const float labelWidth = EditorComponents::GetLabelWidth();
    ImGui::Text("%s", displayName.c_str());
    ImGui::SameLine(labelWidth);
    ImGui::SetNextItemWidth(-1);

    // Create buffer for ImGui (strings need char buffer)
    static std::unordered_map<const void*, std::vector<char>> stringBuffers;

    auto& buffer = stringBuffers[fieldPtr];
    if (buffer.empty() || std::string(buffer.data()) != *str) {
        buffer.clear();
        buffer.resize(256, '\0');
        if (!str->empty() && str->length() < 255) {
            std::copy(str->begin(), str->end(), buffer.begin());
        }
    }

    std::string id = MakeFieldID(fieldName, fieldPtr);
    if (UndoableWidgets::InputText(id.c_str(), buffer.data(), buffer.size())) {
        *str = std::string(buffer.data());
        return true;
    }

    return false;
}

bool ReflectionRenderer::RenderStruct(const char* fieldName, void* fieldPtr,
                                     TypeDescriptor* fieldType, Entity entity,
                                     ECSManager& ecsManager) {
    TypeDescriptor_Struct* structType = dynamic_cast<TypeDescriptor_Struct*>(fieldType);
    if (!structType) return false;

    // Render nested struct as collapsing header
    std::string displayName = CamelCaseToProperCase(fieldName);
    std::string id = MakeFieldID(fieldName, fieldPtr);
    if (ImGui::CollapsingHeader((displayName + id).c_str())) {
        ImGui::Indent();
        bool modified = RenderComponent(fieldPtr, structType, entity, ecsManager);
        ImGui::Unindent();
        return modified;
    }

    return false;
}
