/* Start Header ************************************************************************/
/*!
\file       UndoableWidgets.cpp
\author     Claude (Rewrite)
\date       2026
\brief      Implementation of ImGui widget wrappers with automatic undo/redo support.

            Key changes from the old implementation:
            - NO SCENE SNAPSHOTS - no lag when clicking fields
            - Uses command-based UndoSystem for instant undo/redo
            - Commands are recorded when editing ENDS, not when it starts
            - Continuous edits (dragging) are merged into single commands

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "UndoableWidgets.hpp"
#include "UndoSystem.hpp"
#include <unordered_map>
#include <string>
#include <cstring>

namespace UndoableWidgets {

    // ==================== TRACKING STATE ====================
    // We track when editing starts to capture the "before" value,
    // then record the command when editing ends with the "after" value.

    // ==================== DRAG FLOAT ====================
    bool DragFloat(const char* label, float* v, float v_speed,
                   float v_min, float v_max,
                   const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        // Capture start value when editing begins
        if (!isEditing[v]) {
            startValues[v] = *v;
        }

        bool modified = ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);

        // Track editing state
        if (ImGui::IsItemActivated()) {
            startValues[v] = *v;  // Refresh start value
            isEditing[v] = true;
        }

        // Record command when editing ends (if value changed)
        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            float startVal = startValues[v];
            float endVal = *v;

            // Only record if actually changed
            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(v, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== DRAG FLOAT2 ====================
    bool DragFloat2(const char* label, float v[2], float v_speed,
                    float v_min, float v_max,
                    const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float[2]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v][0] = v[0];
            startValues[v][1] = v[1];
        }

        bool modified = ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v][0] = v[0];
            startValues[v][1] = v[1];
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            bool changed = (startValues[v][0] != v[0]) || (startValues[v][1] != v[1]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[2] = { startValues[v][0], startValues[v][1] };
                float newVal[2] = { v[0], v[1] };
                UndoSystem::GetInstance().RecordArrayChange<float, 2>(v, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== DRAG FLOAT3 ====================
    bool DragFloat3(const char* label, float v[3], float v_speed,
                    float v_min, float v_max,
                    const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float[3]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v][0] = v[0];
            startValues[v][1] = v[1];
            startValues[v][2] = v[2];
        }

        bool modified = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v][0] = v[0];
            startValues[v][1] = v[1];
            startValues[v][2] = v[2];
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            bool changed = (startValues[v][0] != v[0]) ||
                          (startValues[v][1] != v[1]) ||
                          (startValues[v][2] != v[2]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[3] = { startValues[v][0], startValues[v][1], startValues[v][2] };
                float newVal[3] = { v[0], v[1], v[2] };
                UndoSystem::GetInstance().RecordArrayChange<float, 3>(v, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== DRAG FLOAT4 ====================
    bool DragFloat4(const char* label, float v[4], float v_speed,
                    float v_min, float v_max,
                    const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float[4]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v][0] = v[0];
            startValues[v][1] = v[1];
            startValues[v][2] = v[2];
            startValues[v][3] = v[3];
        }

        bool modified = ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v][0] = v[0];
            startValues[v][1] = v[1];
            startValues[v][2] = v[2];
            startValues[v][3] = v[3];
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            bool changed = (startValues[v][0] != v[0]) ||
                          (startValues[v][1] != v[1]) ||
                          (startValues[v][2] != v[2]) ||
                          (startValues[v][3] != v[3]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[4] = { startValues[v][0], startValues[v][1], startValues[v][2], startValues[v][3] };
                float newVal[4] = { v[0], v[1], v[2], v[3] };
                UndoSystem::GetInstance().RecordArrayChange<float, 4>(v, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== DRAG INT ====================
    bool DragInt(const char* label, int* v, float v_speed,
                 int v_min, int v_max,
                 const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, int> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v] = *v;
        }

        bool modified = ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v] = *v;
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            int startVal = startValues[v];
            int endVal = *v;

            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(v, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== COLOR EDIT 3 ====================
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
        static std::unordered_map<const void*, float[3]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[col]) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
        }

        bool modified = ImGui::ColorEdit3(label, col, flags);

        if (ImGui::IsItemActivated()) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
            isEditing[col] = true;
        }

        if (isEditing[col] && !ImGui::IsItemActive()) {
            isEditing[col] = false;
            bool changed = (startValues[col][0] != col[0]) ||
                          (startValues[col][1] != col[1]) ||
                          (startValues[col][2] != col[2]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[3] = { startValues[col][0], startValues[col][1], startValues[col][2] };
                float newVal[3] = { col[0], col[1], col[2] };
                UndoSystem::GetInstance().RecordArrayChange<float, 3>(col, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== COLOR EDIT 4 ====================
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
        static std::unordered_map<const void*, float[4]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[col]) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
            startValues[col][3] = col[3];
        }

        bool modified = ImGui::ColorEdit4(label, col, flags);

        if (ImGui::IsItemActivated()) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
            startValues[col][3] = col[3];
            isEditing[col] = true;
        }

        if (isEditing[col] && !ImGui::IsItemActive()) {
            isEditing[col] = false;
            bool changed = (startValues[col][0] != col[0]) ||
                          (startValues[col][1] != col[1]) ||
                          (startValues[col][2] != col[2]) ||
                          (startValues[col][3] != col[3]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[4] = { startValues[col][0], startValues[col][1], startValues[col][2], startValues[col][3] };
                float newVal[4] = { col[0], col[1], col[2], col[3] };
                UndoSystem::GetInstance().RecordArrayChange<float, 4>(col, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== COLOR PICKER 3 ====================
    bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags) {
        static std::unordered_map<const void*, float[3]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[col]) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
        }

        bool modified = ImGui::ColorPicker3(label, col, flags);

        if (ImGui::IsItemActivated()) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
            isEditing[col] = true;
        }

        if (isEditing[col] && !ImGui::IsItemActive()) {
            isEditing[col] = false;
            bool changed = (startValues[col][0] != col[0]) ||
                          (startValues[col][1] != col[1]) ||
                          (startValues[col][2] != col[2]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[3] = { startValues[col][0], startValues[col][1], startValues[col][2] };
                float newVal[3] = { col[0], col[1], col[2] };
                UndoSystem::GetInstance().RecordArrayChange<float, 3>(col, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== COLOR PICKER 4 ====================
    bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags,
                      const float* ref_col) {
        static std::unordered_map<const void*, float[4]> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[col]) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
            startValues[col][3] = col[3];
        }

        bool modified = ImGui::ColorPicker4(label, col, flags, ref_col);

        if (ImGui::IsItemActivated()) {
            startValues[col][0] = col[0];
            startValues[col][1] = col[1];
            startValues[col][2] = col[2];
            startValues[col][3] = col[3];
            isEditing[col] = true;
        }

        if (isEditing[col] && !ImGui::IsItemActive()) {
            isEditing[col] = false;
            bool changed = (startValues[col][0] != col[0]) ||
                          (startValues[col][1] != col[1]) ||
                          (startValues[col][2] != col[2]) ||
                          (startValues[col][3] != col[3]);

            if (changed && UndoSystem::GetInstance().IsEnabled()) {
                float oldVal[4] = { startValues[col][0], startValues[col][1], startValues[col][2], startValues[col][3] };
                float newVal[4] = { col[0], col[1], col[2], col[3] };
                UndoSystem::GetInstance().RecordArrayChange<float, 4>(col, oldVal, newVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== CHECKBOX ====================
    bool Checkbox(const char* label, bool* v) {
        bool oldValue = *v;
        bool changed = ImGui::Checkbox(label, v);

        if (changed && UndoSystem::GetInstance().IsEnabled()) {
            // Checkbox changes are instant - record immediately
            UndoSystem::GetInstance().RecordPropertyChange(v, oldValue, *v,
                std::string("Toggle ") + label);
        }

        return changed;
    }

    // ==================== COMBO ====================
    bool Combo(const char* label, int* current_item, const char* const items[],
               int items_count, int popup_max_height_in_items) {
        int oldValue = *current_item;
        bool changed = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);

        if (changed && UndoSystem::GetInstance().IsEnabled()) {
            UndoSystem::GetInstance().RecordPropertyChange(current_item, oldValue, *current_item,
                std::string("Change ") + label);
        }

        return changed;
    }

    bool Combo(const char* label, int* current_item,
               bool(*items_getter)(void* data, int idx, const char** out_text),
               void* data, int items_count, int popup_max_height_in_items) {
        int oldValue = *current_item;
        bool changed = ImGui::Combo(label, current_item, items_getter, data, items_count, popup_max_height_in_items);

        if (changed && UndoSystem::GetInstance().IsEnabled()) {
            UndoSystem::GetInstance().RecordPropertyChange(current_item, oldValue, *current_item,
                std::string("Change ") + label);
        }

        return changed;
    }

    bool Combo(const char* label, int* current_item,
               const char* items_separated_by_zeros, int popup_max_height_in_items) {
        int oldValue = *current_item;
        bool changed = ImGui::Combo(label, current_item, items_separated_by_zeros, popup_max_height_in_items);

        if (changed && UndoSystem::GetInstance().IsEnabled()) {
            UndoSystem::GetInstance().RecordPropertyChange(current_item, oldValue, *current_item,
                std::string("Change ") + label);
        }

        return changed;
    }

    // ==================== INPUT TEXT ====================
    bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
        static std::unordered_map<const void*, std::string> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[buf]) {
            startValues[buf] = std::string(buf);
        }

        bool changed = ImGui::InputText(label, buf, buf_size, flags);

        if (ImGui::IsItemActivated()) {
            startValues[buf] = std::string(buf);
            isEditing[buf] = true;
        }

        if (isEditing[buf] && !ImGui::IsItemActive()) {
            isEditing[buf] = false;
            std::string startVal = startValues[buf];
            std::string endVal = std::string(buf);

            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordCharBufferChange(buf, buf_size, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return changed;
    }

    // ==================== SLIDER FLOAT ====================
    bool SliderFloat(const char* label, float* v, float v_min, float v_max,
                     const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, float> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v] = *v;
        }

        bool modified = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v] = *v;
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            float startVal = startValues[v];
            float endVal = *v;

            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(v, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== SLIDER INT ====================
    bool SliderInt(const char* label, int* v, int v_min, int v_max,
                   const char* format, ImGuiSliderFlags flags) {
        static std::unordered_map<const void*, int> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v] = *v;
        }

        bool modified = ImGui::SliderInt(label, v, v_min, v_max, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v] = *v;
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            int startVal = startValues[v];
            int endVal = *v;

            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(v, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return modified;
    }

    // ==================== INPUT FLOAT ====================
    bool InputFloat(const char* label, float* v, float step, float step_fast,
                    const char* format, ImGuiInputTextFlags flags) {
        static std::unordered_map<const void*, float> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v] = *v;
        }

        bool changed = ImGui::InputFloat(label, v, step, step_fast, format, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v] = *v;
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            float startVal = startValues[v];
            float endVal = *v;

            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(v, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return changed;
    }

    // ==================== INPUT INT ====================
    bool InputInt(const char* label, int* v, int step, int step_fast,
                  ImGuiInputTextFlags flags) {
        static std::unordered_map<const void*, int> startValues;
        static std::unordered_map<const void*, bool> isEditing;

        if (!isEditing[v]) {
            startValues[v] = *v;
        }

        bool changed = ImGui::InputInt(label, v, step, step_fast, flags);

        if (ImGui::IsItemActivated()) {
            startValues[v] = *v;
            isEditing[v] = true;
        }

        if (isEditing[v] && !ImGui::IsItemActive()) {
            isEditing[v] = false;
            int startVal = startValues[v];
            int endVal = *v;

            if (startVal != endVal && UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(v, startVal, endVal,
                    std::string("Edit ") + label);
            }
        }

        return changed;
    }

    // ==================== DRAG DROP VALUE ====================
    template<typename T>
    bool AcceptDragDropValue(const char* type, const char* description, T* targetValue, const T& newValue) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type)) {
            T oldValue = *targetValue;
            *targetValue = newValue;

            if (UndoSystem::GetInstance().IsEnabled()) {
                UndoSystem::GetInstance().RecordPropertyChange(targetValue, oldValue, newValue, description);
            }
            return true;
        }
        return false;
    }

    // Explicit template instantiations for common types
    template bool AcceptDragDropValue<int>(const char*, const char*, int*, const int&);
    template bool AcceptDragDropValue<float>(const char*, const char*, float*, const float&);
    template bool AcceptDragDropValue<bool>(const char*, const char*, bool*, const bool&);

} // namespace UndoableWidgets
