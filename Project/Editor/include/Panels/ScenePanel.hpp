#pragma once

#include "EditorPanel.hpp"
#include "EditorCamera.hpp"
#include "imgui.h"
#include "ImGuizmo.h"

/**
 * @brief Scene editing panel with ImGuizmo integration.
 *
 * This panel provides scene editing capabilities with gizmos for
 * transforming objects, grid visualization, and other scene editing tools.
 * Updated to use camelCase member variables without m_ prefix.
 */
class ScenePanel : public EditorPanel {
public:
    ScenePanel();
    virtual ~ScenePanel() = default;

    /**
     * @brief Render the scene panel's ImGui content with ImGuizmo tools.
     */
    void OnImGuiRender() override;

private:
    // ImGuizmo state (now managed by PlayControlPanel)
    ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;
    
    // Editor camera for this panel
    EditorCamera editorCamera;

    // Input tracking for camera (reverted from EditorInputManager for working orbit)
    glm::vec2 lastMousePos;
    bool firstMouse = true;

    // Matrix storage for ImGuizmo
    float identityMatrix[16];
    
    void InitializeMatrices();
    void HandleKeyboardInput();
    void HandleCameraInput();
    void HandleEntitySelection();
    void RenderSceneWithEditorCamera(int width, int height);
    void HandleImGuizmoInChildWindow(float sceneWidth, float sceneHeight);
    void RenderViewGizmo(float sceneWidth, float sceneHeight);

    // Helper functions
    void Mat4ToFloatArray(const glm::mat4& mat, float* arr);
};