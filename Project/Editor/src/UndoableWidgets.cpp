/* Start Header ************************************************************************/
/*!
\file       UndoableWidgets.cpp
\author     Lucas Yee
\date       2025
\brief      Implementation of ImGui widget wrappers with automatic undo/redo support.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "UndoableWidgets.hpp"
#include "SnapshotManager.hpp"
#include <unordered_map>
#include <string>

namespace UndoableWidgets {

    // ==================== DRAG FLOAT ====================
    bool DragFloat(const char* label, float* v, float v_speed,
                   float v_min, float v_max,
                   const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v] = *v;
        }

        bool modified = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal = savedValues[v];
            float newVal = *v;
            *v = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *v = newVal;
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== DRAG FLOAT3 ====================
    bool DragFloat3(const char* label, float v[3], float v_speed,
                    float v_min, float v_max,
                    const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float[3]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v][0] = v[0];
            savedValues[v][1] = v[1];
            savedValues[v][2] = v[2];
        }

        bool modified = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[3] = { savedValues[v][0], savedValues[v][1], savedValues[v][2] };
            float newVal[3] = { v[0], v[1], v[2] };
            v[0] = oldVal[0]; v[1] = oldVal[1]; v[2] = oldVal[2];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            v[0] = newVal[0]; v[1] = newVal[1]; v[2] = newVal[2];
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== DRAG INT ====================
    bool DragInt(const char* label, int* v, float v_speed,
                 int v_min, int v_max,
                 const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, int> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v] = *v;
        }

        bool modified = ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            int oldVal = savedValues[v];
            int newVal = *v;
            *v = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *v = newVal;
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== COLOR EDIT 3 ====================
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
        static std::unordered_map<const void*, float[3]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(col) == wasEditing.end() || !wasEditing[col]) {
            savedValues[col][0] = col[0];
            savedValues[col][1] = col[1];
            savedValues[col][2] = col[2];
        }

        bool modified = ImGui::ColorEdit3(label, col, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[3] = { savedValues[col][0], savedValues[col][1], savedValues[col][2] };
            float newVal[3] = { col[0], col[1], col[2] };
            col[0] = oldVal[0]; col[1] = oldVal[1]; col[2] = oldVal[2];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            col[0] = newVal[0]; col[1] = newVal[1]; col[2] = newVal[2];
            wasEditing[col] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[col]) {
            wasEditing[col] = false;
        }

        return modified;
    }

    // ==================== COLOR EDIT 4 ====================
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
        static std::unordered_map<const void*, float[4]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(col) == wasEditing.end() || !wasEditing[col]) {
            savedValues[col][0] = col[0];
            savedValues[col][1] = col[1];
            savedValues[col][2] = col[2];
            savedValues[col][3] = col[3];
        }

        bool modified = ImGui::ColorEdit4(label, col, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[4] = { savedValues[col][0], savedValues[col][1], savedValues[col][2], savedValues[col][3] };
            float newVal[4] = { col[0], col[1], col[2], col[3] };
            col[0] = oldVal[0]; col[1] = oldVal[1]; col[2] = oldVal[2]; col[3] = oldVal[3];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            col[0] = newVal[0]; col[1] = newVal[1]; col[2] = newVal[2]; col[3] = newVal[3];
            wasEditing[col] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[col]) {
            wasEditing[col] = false;
        }

        return modified;
    }

    // ==================== CHECKBOX ====================
    bool Checkbox(const char* label, bool* v) {
        static std::unordered_map<const void*, bool> previousFrameValue;
        static std::unordered_map<const void*, bool> snapshotTakenForThisChange;

        // Initialize tracking on first encounter
        if (previousFrameValue.find(v) == previousFrameValue.end()) {
            previousFrameValue[v] = *v;
            snapshotTakenForThisChange[v] = false;
        }

        // Store the value BEFORE ImGui::Checkbox modifies it
        bool valueBeforeCheckbox = *v;

        // Render the checkbox (this may toggle the value)
        bool changed = ImGui::Checkbox(label, v);

        // If the checkbox changed AND we haven't taken a snapshot for this change yet
        if (changed && !snapshotTakenForThisChange[v] && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            // Use the value from BEFORE the checkbox toggled it
            bool oldValue = previousFrameValue[v];
            bool newValue = *v;

            // Temporarily set to old value for snapshot
            *v = oldValue;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            // Restore the new value
            *v = newValue;

            snapshotTakenForThisChange[v] = true;
        }

        // Update previous frame value for next frame
        if (!changed || !snapshotTakenForThisChange[v]) {
            previousFrameValue[v] = *v;
        }

        // Reset snapshot flag when not clicking
        if (!ImGui::IsItemActive()) {
            snapshotTakenForThisChange[v] = false;
        }

        return changed;
    }

    // ==================== COMBO ====================
    bool Combo(const char* label, int* current_item, const char* const items[],
               int items_count, int popup_max_height_in_items) {
        static std::unordered_map<const void*, int> savedValues;
        static std::unordered_map<const void*, bool> wasActive;

        // Save value before widget modifies it
        if (wasActive.find(current_item) == wasActive.end() || !wasActive[current_item]) {
            savedValues[current_item] = *current_item;
        }

        bool changed = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);

        // Snapshot when combo popup is opened (before user selects new value)
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            int oldVal = savedValues[current_item];
            int newVal = *current_item;
            *current_item = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *current_item = newVal;
            wasActive[current_item] = true;
        }

        // Reset flag when combo is closed
        if (!ImGui::IsItemActive() && wasActive[current_item]) {
            wasActive[current_item] = false;
        }

        return changed;
    }

    // Combo overload for callback-based items
    bool Combo(const char* label, int* current_item,
               bool(*items_getter)(void* data, int idx, const char** out_text),
               void* data, int items_count, int popup_max_height_in_items) {
        static std::unordered_map<const void*, int> savedValues;
        static std::unordered_map<const void*, bool> wasActive;

        // Save value before widget modifies it
        if (wasActive.find(current_item) == wasActive.end() || !wasActive[current_item]) {
            savedValues[current_item] = *current_item;
        }

        bool changed = ImGui::Combo(label, current_item, items_getter, data, items_count, popup_max_height_in_items);

        // Snapshot when combo popup is opened (before user selects new value)
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            int oldVal = savedValues[current_item];
            int newVal = *current_item;
            *current_item = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *current_item = newVal;
            wasActive[current_item] = true;
        }

        // Reset flag when combo is closed
        if (!ImGui::IsItemActive() && wasActive[current_item]) {
            wasActive[current_item] = false;
        }

        return changed;
    }

    // Combo overload for single string with separators
    bool Combo(const char* label, int* current_item,
               const char* items_separated_by_zeros, int popup_max_height_in_items) {
        static std::unordered_map<const void*, int> savedValues;
        static std::unordered_map<const void*, bool> wasActive;

        // Save value before widget modifies it
        if (wasActive.find(current_item) == wasActive.end() || !wasActive[current_item]) {
            savedValues[current_item] = *current_item;
        }

        bool changed = ImGui::Combo(label, current_item, items_separated_by_zeros, popup_max_height_in_items);

        // Snapshot when combo popup is opened (before user selects new value)
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            int oldVal = savedValues[current_item];
            int newVal = *current_item;
            *current_item = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *current_item = newVal;
            wasActive[current_item] = true;
        }

        // Reset flag when combo is closed
        if (!ImGui::IsItemActive() && wasActive[current_item]) {
            wasActive[current_item] = false;
        }

        return changed;
    }

    // ==================== INPUT TEXT ====================
    bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
        static std::unordered_map<const void*, std::string> savedValues;
        static std::unordered_map<const void*, bool> wasActive;

        // Save value before widget modifies it
        if (wasActive.find(buf) == wasActive.end() || !wasActive[buf]) {
            savedValues[buf] = std::string(buf);
        }

        bool changed = ImGui::InputText(label, buf, buf_size, flags);

        // Snapshot when text input is activated (clicked/focused)
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            std::string oldVal = savedValues[buf];
            std::string newVal = std::string(buf);
            #ifdef _MSC_VER
                strncpy_s(buf, buf_size, oldVal.c_str(), _TRUNCATE);
            #else
                std::strncpy(buf, oldVal.c_str(), buf_size - 1);
                buf[buf_size - 1] = '\0';
            #endif
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            #ifdef _MSC_VER
                strncpy_s(buf, buf_size, newVal.c_str(), _TRUNCATE);
            #else
                std::strncpy(buf, newVal.c_str(), buf_size - 1);
                buf[buf_size - 1] = '\0';
            #endif
            wasActive[buf] = true;
        }

        // Reset flag when text input loses focus
        if (!ImGui::IsItemActive() && wasActive[buf]) {
            wasActive[buf] = false;
        }

        return changed;
    }

    // ==================== SLIDER FLOAT ====================
    bool SliderFloat(const char* label, float* v, float v_min, float v_max,
                     const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v] = *v;
        }

        bool modified = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal = savedValues[v];
            float newVal = *v;
            *v = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *v = newVal;
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== SLIDER INT ====================
    bool SliderInt(const char* label, int* v, int v_min, int v_max,
                   const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, int> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v] = *v;
        }

        bool modified = ImGui::SliderInt(label, v, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            int oldVal = savedValues[v];
            int newVal = *v;
            *v = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *v = newVal;
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== INPUT FLOAT ====================
    bool InputFloat(const char* label, float* v, float step, float step_fast,
                    const char* format, ImGuiInputTextFlags flags) {
        static std::unordered_map<const void*, float> savedValues;
        static std::unordered_map<const void*, bool> wasActive;

        // Save value before widget modifies it
        if (wasActive.find(v) == wasActive.end() || !wasActive[v]) {
            savedValues[v] = *v;
        }

        bool changed = ImGui::InputFloat(label, v, step, step_fast, format, flags);

        // Snapshot when input is activated
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal = savedValues[v];
            float newVal = *v;
            *v = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *v = newVal;
            wasActive[v] = true;
        }

        // Reset flag when input loses focus
        if (!ImGui::IsItemActive() && wasActive[v]) {
            wasActive[v] = false;
        }

        return changed;
    }

    // ==================== INPUT INT ====================
    bool InputInt(const char* label, int* v, int step, int step_fast,
                  ImGuiInputTextFlags flags) {
        static std::unordered_map<const void*, int> savedValues;
        static std::unordered_map<const void*, bool> wasActive;

        // Save value before widget modifies it
        if (wasActive.find(v) == wasActive.end() || !wasActive[v]) {
            savedValues[v] = *v;
        }

        bool changed = ImGui::InputInt(label, v, step, step_fast, flags);

        // Snapshot when input is activated
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            int oldVal = savedValues[v];
            int newVal = *v;
            *v = oldVal;
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            *v = newVal;
            wasActive[v] = true;
        }

        // Reset flag when input loses focus
        if (!ImGui::IsItemActive() && wasActive[v]) {
            wasActive[v] = false;
        }

        return changed;
    }

    // ==================== DRAG FLOAT2 ====================
    bool DragFloat2(const char* label, float v[2], float v_speed,
                    float v_min, float v_max,
                    const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float[2]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v][0] = v[0];
            savedValues[v][1] = v[1];
        }

        bool modified = ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[2] = { savedValues[v][0], savedValues[v][1] };
            float newVal[2] = { v[0], v[1] };
            v[0] = oldVal[0]; v[1] = oldVal[1];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            v[0] = newVal[0]; v[1] = newVal[1];
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== DRAG FLOAT4 ====================
    bool DragFloat4(const char* label, float v[4], float v_speed,
                    float v_min, float v_max,
                    const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float[4]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(v) == wasEditing.end() || !wasEditing[v]) {
            savedValues[v][0] = v[0];
            savedValues[v][1] = v[1];
            savedValues[v][2] = v[2];
            savedValues[v][3] = v[3];
        }

        bool modified = ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[4] = { savedValues[v][0], savedValues[v][1], savedValues[v][2], savedValues[v][3] };
            float newVal[4] = { v[0], v[1], v[2], v[3] };
            v[0] = oldVal[0]; v[1] = oldVal[1]; v[2] = oldVal[2]; v[3] = oldVal[3];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            v[0] = newVal[0]; v[1] = newVal[1]; v[2] = newVal[2]; v[3] = newVal[3];
            wasEditing[v] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[v]) {
            wasEditing[v] = false;
        }

        return modified;
    }

    // ==================== COLOR PICKER 3 ====================
    bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags) {
        static std::unordered_map<const void*, float[3]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(col) == wasEditing.end() || !wasEditing[col]) {
            savedValues[col][0] = col[0];
            savedValues[col][1] = col[1];
            savedValues[col][2] = col[2];
        }

        bool modified = ImGui::ColorPicker3(label, col, flags);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[3] = { savedValues[col][0], savedValues[col][1], savedValues[col][2] };
            float newVal[3] = { col[0], col[1], col[2] };
            col[0] = oldVal[0]; col[1] = oldVal[1]; col[2] = oldVal[2];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            col[0] = newVal[0]; col[1] = newVal[1]; col[2] = newVal[2];
            wasEditing[col] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[col]) {
            wasEditing[col] = false;
        }

        return modified;
    }

    // ==================== COLOR PICKER 4 ====================
    bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags,
                      const float* ref_col) {
        static std::unordered_map<const void*, float[4]> savedValues;
        static std::unordered_map<const void*, bool> wasEditing;

        // Save value before widget modifies it
        if (wasEditing.find(col) == wasEditing.end() || !wasEditing[col]) {
            savedValues[col][0] = col[0];
            savedValues[col][1] = col[1];
            savedValues[col][2] = col[2];
            savedValues[col][3] = col[3];
        }

        bool modified = ImGui::ColorPicker4(label, col, flags, ref_col);

        // Snapshot when editing starts
        if (ImGui::IsItemActivated() && SnapshotManager::GetInstance().IsSnapshotEnabled()) {
            float oldVal[4] = { savedValues[col][0], savedValues[col][1], savedValues[col][2], savedValues[col][3] };
            float newVal[4] = { col[0], col[1], col[2], col[3] };
            col[0] = oldVal[0]; col[1] = oldVal[1]; col[2] = oldVal[2]; col[3] = oldVal[3];
            SnapshotManager::GetInstance().TakeSnapshot(std::string("Edit ") + label);
            col[0] = newVal[0]; col[1] = newVal[1]; col[2] = newVal[2]; col[3] = newVal[3];
            wasEditing[col] = true;
        }

        // Reset flag when editing ends
        if (!ImGui::IsItemActive() && wasEditing[col]) {
            wasEditing[col] = false;
        }

        return modified;
    }

} // namespace UndoableWidgets
