#pragma once

#include "EditorPanel.hpp"
#include "EditorCamera.hpp"
#include "EditorState.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include "Utilities/GUID.hpp"
#include <memory>
#include <string>

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

    /**
     * @brief Reposition camera for 2D/3D mode switching
     * @param target The target position to look at
     */
    void SetCameraTarget(const glm::vec3& target);

    /**
     * @brief Reset camera zoom to default (1.0)
     */
    void ResetCameraZoom() { editorCamera.OrthoZoomLevel = 1.0f; }

    /**
     * @brief Set camera zoom level
     * @param zoom Zoom level (1.0 = normal, >1.0 = zoomed out, <1.0 = zoomed in)
     */
    void SetCameraZoom(float zoom) { editorCamera.OrthoZoomLevel = zoom; }

private:
    void AcceptPrefabDropInScene(const ImVec2& sceneTopLeft, const ImVec2& sceneSize);

    // ImGuizmo state (now managed by PlayControlPanel)
    ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;
    
    // Editor camera for this panel
    EditorCamera editorCamera;

    // Input tracking for camera (reverted from EditorInputManager for working orbit)
    glm::vec2 lastMousePos;
    bool firstMouse = true;

    // Matrix storage for ImGuizmo
    float identityMatrix[16];

    // Model drag-and-drop preview state
    bool isDraggingModel = false;
    GUID_128 previewModelGUID = {0, 0};
    std::string previewModelPath;
    glm::vec3 previewPosition = glm::vec3(0.0f);
    bool previewValidPlacement = true;
    Entity previewEntity = static_cast<Entity>(-1);

    // Marquee selection state
    bool isMarqueeSelecting = false;
    ImVec2 marqueeStart;
    ImVec2 marqueeEnd;
    
    // Gizmo drag state to prevent accidental selection after dragging
    bool justFinishedGizmoDrag = false;
    
    // Cached matrices for performance
    glm::mat4 cachedViewMatrix;
    glm::mat4 cachedProjectionMatrix;
    ImVec2 cachedWindowSize;
    
    // Gizmo manipulation state
    bool gizmoWasUsing = false;
    bool gizmoSnapshotTaken = false;
    std::vector<std::array<float, 16>> originalMatrices;
    std::array<float, 16> originalPivot;
    
    void InitializeMatrices();
    void HandleKeyboardInput();
    void HandleCameraInput();
    void HandleEntitySelection();
    void HandleModelDragDrop(float sceneWidth, float sceneHeight);
    void RenderModelPreview(float sceneWidth, float sceneHeight);
    Entity SpawnModelEntity(const glm::vec3& position);
    void RenderSceneWithEditorCamera(int width, int height);
    void HandleImGuizmoInChildWindow(float sceneWidth, float sceneHeight);
    void RenderViewGizmo(float sceneWidth, float sceneHeight);
    void DrawGameViewportIndicator();
    void DrawColliderGizmos();
    void DrawCameraGizmos();
    void DrawAudioGizmos();
    void DrawSelectionOutline(Entity entity, int sceneWidth, int sceneHeight);

    // Helper functions
    void Mat4ToFloatArray(const glm::mat4& mat, float* arr);
    ImVec2 ProjectToScreen(const glm::vec3& worldPoint, bool& isVisible, const glm::mat4& vp, const ImVec2& windowPos, const ImVec2& windowSize) const;
    void UpdateMouseState(ImVec2& relativeMousePos, bool& isHovering, float& sceneWidth, float& sceneHeight);
};