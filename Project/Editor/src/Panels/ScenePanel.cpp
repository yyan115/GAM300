#include "Panels/ScenePanel.hpp"
#include "EditorInputManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/SceneRenderer.hpp"
#include "RaycastUtil.hpp"
#include "imgui.h"
#include "ImGuizmo.h"
#include "EditorState.hpp"
#include <cstring>
#include <cmath>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

// Don't include Graphics headers here due to OpenGL conflicts
// We'll use RaycastUtil to get entity transforms instead

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ScenePanel::ScenePanel()
    : EditorPanel("Scene", true), m_EditorCamera(glm::vec3(0.0f, 0.0f, 0.0f), 5.0f) {
    InitializeMatrices();
}

void ScenePanel::InitializeMatrices() {
    // Initialize identity matrix for ImGuizmo
    memset(m_IdentityMatrix, 0, sizeof(m_IdentityMatrix));
    m_IdentityMatrix[0] = m_IdentityMatrix[5] = m_IdentityMatrix[10] = m_IdentityMatrix[15] = 1.0f;
}

void ScenePanel::Mat4ToFloatArray(const glm::mat4& mat, float* arr) {
    const float* source = glm::value_ptr(mat);
    for (int i = 0; i < 16; ++i) {
        arr[i] = source[i];
    }
}


void ScenePanel::HandleKeyboardInput() {
    // Check keyboard input regardless of camera input conditions
    if (EditorInputManager::IsGizmoShortcutPressed(0)) {
        // Q key - Normal pan mode (no gizmos, left click panning)
        m_IsNormalPanMode = true;
        std::cout << "[ScenePanel] Q pressed - Normal/Pan mode (gizmos hidden, LMB panning)" << std::endl;
    }
    if (EditorInputManager::IsGizmoShortcutPressed(1)) {
        m_IsNormalPanMode = false;
        m_GizmoOperation = ImGuizmo::TRANSLATE;
        std::cout << "[ScenePanel] W pressed - Translate mode" << std::endl;
    }
    if (EditorInputManager::IsGizmoShortcutPressed(2)) {
        m_IsNormalPanMode = false;
        m_GizmoOperation = ImGuizmo::ROTATE;
        std::cout << "[ScenePanel] E pressed - Rotate mode" << std::endl;
    }
    if (EditorInputManager::IsGizmoShortcutPressed(3)) {
        m_IsNormalPanMode = false;
        m_GizmoOperation = ImGuizmo::SCALE;
        std::cout << "[ScenePanel] R pressed - Scale mode" << std::endl;
    }
}

void ScenePanel::HandleCameraInput() {
    // Hover check is now handled by the caller

    // Get current mouse position (revert to original working method)
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 currentMousePos = glm::vec2(io.MousePos.x, io.MousePos.y);

    // Calculate mouse delta (original working logic)
    glm::vec2 mouseDelta(0.0f);
    if (!m_FirstMouse) {
        mouseDelta = currentMousePos - m_LastMousePos;
    } else {
        m_FirstMouse = false;
    }
    m_LastMousePos = currentMousePos;

    // Get input states
    bool isAltPressed = io.KeyAlt;
    bool isLeftMousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMiddleMousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    float scrollDelta = io.MouseWheel;

    if (m_IsNormalPanMode) {
        isMiddleMousePressed = isLeftMousePressed;
        isLeftMousePressed = false; 
        isAltPressed = false; 
    }

    m_EditorCamera.ProcessInput(
        io.DeltaTime,
        true,
        isAltPressed,
        isLeftMousePressed,
        isMiddleMousePressed,
        mouseDelta.x,
        -mouseDelta.y,  // Invert Y for standard camera behavior
        scrollDelta
    );
}

void ScenePanel::HandleEntitySelection() {
    // Hover check is now handled by the caller

    // Skip entity selection in normal pan mode
    if (m_IsNormalPanMode) {
        return;  // No entity selection in pan mode
    }

    // Only handle selection on left click (not during camera operations)
    ImGuiIO& io = ImGui::GetIO();
    bool isLeftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool isAltPressed = io.KeyAlt;

    // Only select entities when left clicking without Alt (Alt is for camera orbit)
    if (isLeftClicked && !isAltPressed) {
        // Get mouse position relative to the scene window
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

        // Calculate relative mouse position within the scene view
        float relativeX = mousePos.x - (windowPos.x + contentMin.x);
        float relativeY = mousePos.y - (windowPos.y + contentMin.y);

        // Get scene view dimensions
        float sceneWidth = contentMax.x - contentMin.x;
        float sceneHeight = contentMax.y - contentMin.y;

        // Check if click is within scene bounds
        if (relativeX >= 0 && relativeX <= sceneWidth &&
            relativeY >= 0 && relativeY <= sceneHeight) {

            // Perform proper raycasting for entity selection
            EditorState& editorState = EditorState::GetInstance();

            // Get camera matrices
            float aspectRatio = sceneWidth / sceneHeight;
            glm::mat4 viewMatrix = m_EditorCamera.GetViewMatrix();
            glm::mat4 projMatrix = m_EditorCamera.GetProjectionMatrix(aspectRatio);

            // Cast ray from camera through mouse position
            RaycastUtil::Ray ray = RaycastUtil::ScreenToWorldRay(
                relativeX, relativeY,
                sceneWidth, sceneHeight,
                viewMatrix, projMatrix
            );

            // Perform raycast against scene entities
            RaycastUtil::RaycastHit hit = RaycastUtil::RaycastScene(ray);

            if (hit.hit) {
                // Entity found, select it
                editorState.SetSelectedEntity(hit.entity);
                std::cout << "[ScenePanel] Raycast hit entity " << hit.entity
                          << " at distance " << hit.distance << std::endl;
            } else {
                // No entity hit, clear selection
                editorState.ClearSelection();
                std::cout << "[ScenePanel] Raycast missed - cleared selection" << std::endl;
            }

            std::cout << "[ScenePanel] Mouse clicked at (" << relativeX << ", " << relativeY
                      << ") in scene bounds (" << sceneWidth << "x" << sceneHeight << ")" << std::endl;
        }
    }
}

void ScenePanel::RenderGizmoControls() {
    // Toolbar for gizmo operations with visual feedback for active mode
    if (ImGui::Button(m_IsNormalPanMode ? "[Normal/Pan (Q)]" : "Normal/Pan (Q)")) {
        m_IsNormalPanMode = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(!m_IsNormalPanMode && m_GizmoOperation == ImGuizmo::TRANSLATE ? "[Translate (W)]" : "Translate (W)")) {
        m_IsNormalPanMode = false;
        m_GizmoOperation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::Button(!m_IsNormalPanMode && m_GizmoOperation == ImGuizmo::ROTATE ? "[Rotate (E)]" : "Rotate (E)")) {
        m_IsNormalPanMode = false;
        m_GizmoOperation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::Button(!m_IsNormalPanMode && m_GizmoOperation == ImGuizmo::SCALE ? "[Scale (R)]" : "Scale (R)")) {
        m_IsNormalPanMode = false;
        m_GizmoOperation = ImGuizmo::SCALE;
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
}

void ScenePanel::OnImGuiRender() {
    // Update input manager state
    EditorInputManager::Update();

    if (ImGui::Begin(m_Name.c_str(), &m_IsOpen)) {

        // Render gizmo controls at the top
        RenderGizmoControls();
        ImGui::Separator();

        // Handle input (but not if ImGuizmo is active)
        HandleKeyboardInput();

        // Store scene area hover state for camera input
        bool isSceneHovered = false;

        // Get the content region size for the scene view
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        int sceneViewWidth = (int)viewportPanelSize.x;
        int sceneViewHeight = (int)viewportPanelSize.y;

        // Ensure minimum size
        if (sceneViewWidth < 100) sceneViewWidth = 100;
        if (sceneViewHeight < 100) sceneViewHeight = 100;

        // Render the scene with our editor camera
        RenderSceneWithEditorCamera(sceneViewWidth, sceneViewHeight);

        // Get window position for ImGuizmo (where the scene image starts)
        ImVec2 imagePos = ImGui::GetCursorScreenPos();

        // Get the texture from SceneRenderer and display it
        unsigned int sceneTexture = SceneRenderer::GetSceneTexture();
        if (sceneTexture != 0) {
            // Use a child window to contain both the image and ImGuizmo
            ImGui::BeginChild("SceneView", ImVec2((float)sceneViewWidth, (float)sceneViewHeight), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Get child window position and size
            ImVec2 childPos = ImGui::GetCursorScreenPos();
            ImVec2 childSize = ImGui::GetContentRegionAvail();

            // Draw the scene texture as background
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddImage(
                (void*)(intptr_t)sceneTexture,
                childPos, ImVec2(childPos.x + childSize.x, childPos.y + childSize.y),
                ImVec2(0, 1), ImVec2(1, 0)  // Flip Y coordinate for OpenGL
            );

            // Check if the child window is hovered (without invisible button interfering)
            isSceneHovered = ImGui::IsWindowHovered();

            // Now handle ImGuizmo within the child window context
            HandleImGuizmoInChildWindow((float)sceneViewWidth, (float)sceneViewHeight);

            // End the child window here
            ImGui::EndChild();
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Scene View - Framebuffer not ready");
            ImGui::Text("Size: %dx%d", sceneViewWidth, sceneViewHeight);
        }

        // Handle input based on ImGuizmo state (now calculated)
        if (isSceneHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
            HandleCameraInput();
            HandleEntitySelection();
        }
    }
    ImGui::End();
}

void ScenePanel::RenderSceneWithEditorCamera(int width, int height) {
    try {
        // Pass our editor camera data to the rendering system
        SceneRenderer::BeginSceneRender(width, height);
        SceneRenderer::RenderSceneForEditor(
            m_EditorCamera.Position,
            m_EditorCamera.Front,
            m_EditorCamera.Up,
            m_EditorCamera.Zoom
        );
        SceneRenderer::EndSceneRender();

        // Now both the visual representation AND ImGuizmo overlay use our editor camera
        // This gives us proper Unity-style editor controls

    } catch (const std::exception& e) {
        std::cerr << "Exception in RenderSceneWithEditorCamera: " << e.what() << std::endl;
    }
}

void ScenePanel::HandleImGuizmoInChildWindow(float sceneWidth, float sceneHeight) {
    // Ensure ImGuizmo is set up properly for this frame
    ImGuizmo::BeginFrame();

    // Push unique ID for this ImGuizmo instance
    ImGui::PushID("SceneGizmo");

    // Make gizmos bigger and more interactive
    ImGuizmo::SetGizmoSizeClipSpace(0.25f);  // Make gizmos bigger (default is 0.1f)

    // Set ImGuizmo to use the current window's draw list
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

    // Get the current child window dimensions directly
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Use the full child window area for ImGuizmo
    ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

    // Enable ImGuizmo to receive input
    ImGuizmo::Enable(true);
    ImGuizmo::AllowAxisFlip(false);

    // Print debug info about the setup
    static bool setupPrinted = false;
    if (!setupPrinted) {
        std::cout << "[ImGuizmo] Setup - WindowPos: (" << windowPos.x << ", " << windowPos.y
                  << ") Size: (" << windowSize.x << ", " << windowSize.y << ")" << std::endl;
        setupPrinted = true;
    }

    // Get matrices from editor camera
    float aspectRatio = sceneWidth / sceneHeight;
    glm::mat4 view = m_EditorCamera.GetViewMatrix();
    glm::mat4 projection = m_EditorCamera.GetProjectionMatrix(aspectRatio);

    float viewMatrix[16], projMatrix[16];
    Mat4ToFloatArray(view, viewMatrix);
    Mat4ToFloatArray(projection, projMatrix);

    // Draw grid
    ImGuizmo::DrawGrid(viewMatrix, projMatrix, m_IdentityMatrix, 10.0f);

    // Only show gizmo when an entity is selected AND not in normal pan mode
    EditorState& editorState = EditorState::GetInstance();
    if (editorState.HasSelectedEntity() && !m_IsNormalPanMode) {
        // Get the actual transform matrix from the selected entity
        Entity selectedEntity = editorState.GetSelectedEntity();
        static float selectedObjectMatrix[16];

        // Get transform using RaycastUtil helper to avoid OpenGL header conflicts
        if (!RaycastUtil::GetEntityTransform(selectedEntity, selectedObjectMatrix)) {
            // Fallback to identity if entity doesn't have transform
            memcpy(selectedObjectMatrix, m_IdentityMatrix, sizeof(selectedObjectMatrix));
        }

        bool isUsing = ImGuizmo::Manipulate(
            viewMatrix, projMatrix,
            m_GizmoOperation, m_GizmoMode,
            selectedObjectMatrix,
            nullptr, nullptr
        );

        // Debug: Check if ImGuizmo is responding to mouse
        static bool wasOver = false;
        static bool wasUsing = false;

        bool isOver = ImGuizmo::IsOver();
        bool isCurrentlyUsing = ImGuizmo::IsUsing();

        if (isOver != wasOver) {
            std::cout << "[ImGuizmo] IsOver changed: " << (isOver ? "true" : "false") << std::endl;
            wasOver = isOver;
        }

        if (isCurrentlyUsing != wasUsing) {
            std::cout << "[ImGuizmo] IsUsing changed: " << (isCurrentlyUsing ? "true" : "false") << std::endl;
            wasUsing = isCurrentlyUsing;
        }

        // Debug mouse click detection
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            std::cout << "[ImGuizmo] Mouse clicked at (" << io.MousePos.x << ", " << io.MousePos.y << ")" << std::endl;
            std::cout << "[ImGuizmo] Gizmo area: (" << windowPos.x << ", " << windowPos.y << ") to ("
                      << (windowPos.x + windowSize.x) << ", " << (windowPos.y + windowSize.y) << ")" << std::endl;

            // Calculate relative mouse position within the child window
            float relativeX = io.MousePos.x - windowPos.x;
            float relativeY = io.MousePos.y - windowPos.y;
            std::cout << "[ImGuizmo] Relative mouse pos: (" << relativeX << ", " << relativeY << ")" << std::endl;

            // Check if click is within ImGuizmo area
            bool withinGizmoArea = (io.MousePos.x >= windowPos.x && io.MousePos.x <= windowPos.x + windowSize.x &&
                                   io.MousePos.y >= windowPos.y && io.MousePos.y <= windowPos.y + windowSize.y);
            std::cout << "[ImGuizmo] Click within gizmo area: " << (withinGizmoArea ? "true" : "false") << std::endl;
            std::cout << "[ImGuizmo] ImGuizmo::IsOver(): " << (ImGuizmo::IsOver() ? "true" : "false") << std::endl;
            std::cout << "[ImGuizmo] ImGuizmo::IsUsing(): " << (ImGuizmo::IsUsing() ? "true" : "false") << std::endl;
        }

        // Apply transform changes to the actual entity
        if (isUsing) {
            // Update the entity's transform in the ECS system
            RaycastUtil::SetEntityTransform(selectedEntity, selectedObjectMatrix);

            // Display transformation information for debugging (render outside child window)
            // This will be shown in the main panel
        }
    }

    // Pop the ID scope
    ImGui::PopID();
}