/* Start Header ************************************************************************/
/*!
\file       UndoableWidgets.hpp
\author     Claude (Rewrite)
\date       2026
\brief      ImGui widget wrappers with automatic undo/redo support (Unity-style).

            These widgets use the new command-based UndoSystem instead of taking
            full scene snapshots. This means:
            - NO lag when clicking on fields
            - NO scene serialization
            - Instant undo/redo

            Usage: Use these instead of raw ImGui calls to get undo/redo for free.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "imgui.h"
#include <functional>

namespace UndoableWidgets {

    // ==================== DRAG FLOAT ====================
    bool DragFloat(const char* label, float* v, float v_speed = 1.0f,
                   float v_min = 0.0f, float v_max = 0.0f,
                   const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    // ==================== DRAG FLOAT2 ====================
    bool DragFloat2(const char* label, float v[2], float v_speed = 1.0f,
                    float v_min = 0.0f, float v_max = 0.0f,
                    const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    // ==================== DRAG FLOAT3 ====================
    bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f,
                    float v_min = 0.0f, float v_max = 0.0f,
                    const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    // ==================== DRAG FLOAT4 ====================
    bool DragFloat4(const char* label, float v[4], float v_speed = 1.0f,
                    float v_min = 0.0f, float v_max = 0.0f,
                    const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    // ==================== DRAG INT ====================
    bool DragInt(const char* label, int* v, float v_speed = 1.0f,
                 int v_min = 0, int v_max = 0,
                 const char* format = "%d", ImGuiSliderFlags flags = 0);

    // ==================== COLOR EDIT 3 ====================
    bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);

    // ==================== COLOR EDIT 4 ====================
    bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);

    // ==================== COLOR PICKER 3 ====================
    bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);

    // ==================== COLOR PICKER 4 ====================
    bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0,
                      const float* ref_col = NULL);

    // ==================== CHECKBOX ====================
    bool Checkbox(const char* label, bool* v);

    // ==================== COMBO ====================
    bool Combo(const char* label, int* current_item, const char* const items[],
               int items_count, int popup_max_height_in_items = -1);

    bool Combo(const char* label, int* current_item,
               bool(*items_getter)(void* data, int idx, const char** out_text),
               void* data, int items_count, int popup_max_height_in_items = -1);

    bool Combo(const char* label, int* current_item,
               const char* items_separated_by_zeros, int popup_max_height_in_items = -1);

    // ==================== INPUT TEXT ====================
    bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);

    // ==================== SLIDER FLOAT ====================
    bool SliderFloat(const char* label, float* v, float v_min, float v_max,
                     const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    // ==================== SLIDER INT ====================
    bool SliderInt(const char* label, int* v, int v_min, int v_max,
                   const char* format = "%d", ImGuiSliderFlags flags = 0);

    // ==================== INPUT FLOAT ====================
    bool InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f,
                    const char* format = "%.3f", ImGuiInputTextFlags flags = 0);

    // ==================== INPUT INT ====================
    bool InputInt(const char* label, int* v, int step = 1, int step_fast = 100,
                  ImGuiInputTextFlags flags = 0);

    // ==================== DRAG DROP HANDLERS ====================

    /**
     * @brief Handle drag-drop with automatic undo support
     *
     * Use this for drag-drop operations that modify data.
     */
    template<typename T>
    bool AcceptDragDropPayload(const char* type, const char* description,
                                std::function<void(const ImGuiPayload*)> handler) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type)) {
            // Execute the handler (it should record its own undo command)
            handler(payload);
            return true;
        }
        return false;
    }

    /**
     * @brief Handle drag-drop for simple value assignments
     */
    template<typename T>
    bool AcceptDragDropValue(const char* type, const char* description, T* targetValue, const T& newValue);

} // namespace UndoableWidgets
