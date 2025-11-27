#include "pch.h"
#include "Panels/ScenePanel.hpp"
#include "Panels/PlayControlPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/GamePanel.hpp"
#include "EditorInputManager.hpp"
#include "EditorComponents.hpp"
#include "Graphics/SceneRenderer.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/NameComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "Graphics/Material.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Graphics/DebugDraw/DebugDrawSystem.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "RaycastUtil.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include "EditorState.hpp"
#include "PrefabIO.hpp"
#include "GUIManager.hpp"
#include <cstring>
#include <cmath>
#include <iostream>
#include <limits>
#include <glm/gtc/type_ptr.hpp>
#include "SnapshotManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "Logging.hpp"
#include "RunTimeVar.hpp"

// External globals for model drag-drop from AssetBrowserPanel
extern GUID_128 DraggedModelGuid;
extern std::string DraggedModelPath;

// Don't include Graphics headers here due to OpenGL conflicts
// We'll use RaycastUtil to get entity transforms instead

ScenePanel::ScenePanel()
    : EditorPanel("Scene", true), editorCamera(glm::vec3(0.0f, 0.0f, 0.0f), 5.0f) {
    // Initialize camera at origin
    // This works for 3D mode (models at origin are visible)
    // For 2D mode, user needs to pan to find 2D sprites (they use pixel coordinates like 25, 700)
    InitializeMatrices();
}

void ScenePanel::SetCameraTarget(const glm::vec3& target) {
    editorCamera.Target = target;

    // Check if we're in 2D mode
    EditorState& editorState = EditorState::GetInstance();
    bool is2DMode = editorState.Is2DMode();

    if (is2DMode) {
        // In 2D mode, only update target - don't recalculate camera vectors
        // (to preserve 2D panning orientation)
        editorCamera.Position = glm::vec3(target.x, target.y, target.z + 5.0f);
    } else {
        // In 3D mode, frame the target properly
        if (editorCamera.Distance > 10.0f) {
            editorCamera.Distance = 5.0f;
        }

        // Let UpdateCameraVectors() calculate the correct position using spherical coordinates
        editorCamera.UpdateCameraVectors();
    }
}

void ScenePanel::DrawGameViewportIndicator() {
    // Get the game resolution from GamePanel (this updates when user changes resolution)
    int gameWidth = RunTimeVar::window.width;
    int gameHeight = RunTimeVar::window.height;

    // Try to get the GamePanel to read its target resolution
    auto gamePanelPtr = GUIManager::GetPanelManager().GetPanel("Game");
    auto gamePanel = std::dynamic_pointer_cast<GamePanel>(gamePanelPtr);
    if (gamePanel) {
        gamePanel->GetTargetGameResolution(gameWidth, gameHeight);
    }

    // For 2D games, the game viewport uses pixel coordinates from (0, 0) to (width, height)
    // Origin (0,0) is at bottom-left, typical OpenGL convention
    // World space corners of the game viewport
    float gameWidthF = static_cast<float>(gameWidth);
    float gameHeightF = static_cast<float>(gameHeight);
    glm::vec3 worldTopLeft(0.0f, gameHeightF, 0.0f);
    glm::vec3 worldTopRight(gameWidthF, gameHeightF, 0.0f);
    glm::vec3 worldBottomRight(gameWidthF, 0.0f, 0.0f);
    glm::vec3 worldBottomLeft(0.0f, 0.0f, 0.0f);

    // Convert world space to screen space using editor camera
    auto worldToScreen = [this](const glm::vec3& worldPos) -> ImVec2 {
        // For 2D orthographic: screen_x = (world_x - camera_target_x) / zoom + viewport_center
        // EditorCamera orthographic projection centers around Target, not Position
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        float screenX = ((worldPos.x - editorCamera.Target.x) / editorCamera.OrthoZoomLevel) + viewportSize.x * 0.5f;
        float screenY = ((editorCamera.Target.y - worldPos.y) / editorCamera.OrthoZoomLevel) + viewportSize.y * 0.5f;

        ImVec2 windowPos = ImGui::GetCursorScreenPos();
        return ImVec2(windowPos.x + screenX, windowPos.y + screenY);
    };

    // Convert corners to screen space
    ImVec2 screenTopLeft = worldToScreen(worldTopLeft);
    ImVec2 screenTopRight = worldToScreen(worldTopRight);
    ImVec2 screenBottomRight = worldToScreen(worldBottomRight);
    ImVec2 screenBottomLeft = worldToScreen(worldBottomLeft);

    // Draw the rectangle using ImGui
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 color = IM_COL32(180, 180, 180, 255); // Light gray
    float thickness = 2.0f;

    drawList->AddLine(screenTopLeft, screenTopRight, color, thickness);
    drawList->AddLine(screenTopRight, screenBottomRight, color, thickness);
    drawList->AddLine(screenBottomRight, screenBottomLeft, color, thickness);
    drawList->AddLine(screenBottomLeft, screenTopLeft, color, thickness);
}

void ScenePanel::InitializeMatrices() {
    // Initialize identity matrix for ImGuizmo
    memset(identityMatrix, 0, sizeof(identityMatrix));
    identityMatrix[0] = identityMatrix[5] = identityMatrix[10] = identityMatrix[15] = 1.0f;
}

void ScenePanel::Mat4ToFloatArray(const glm::mat4& mat, float* arr) {
    const float* source = glm::value_ptr(mat);
    for (int i = 0; i < 16; ++i) {
        arr[i] = source[i];
    }
}

ImVec2 ScenePanel::ProjectToScreen(const glm::vec3& worldPoint, bool& isVisible, const glm::mat4& vp, const ImVec2& windowPos, const ImVec2& windowSize) const {
    glm::vec4 clipSpace = vp * glm::vec4(worldPoint, 1.0f);
    if (clipSpace.w <= 0.0001f) {
        isVisible = false;
        return ImVec2(-10000, -10000);
    }
    glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
    float screenX = (ndc.x + 1.0f) * 0.5f * windowSize.x + windowPos.x;
    float screenY = (1.0f - ndc.y) * 0.5f * windowSize.y + windowPos.y;
    isVisible = true;
    return ImVec2(screenX, screenY);
}

void ScenePanel::UpdateMouseState(ImVec2& relativeMousePos, bool& isHovering, float& sceneWidth, float& sceneHeight) {
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    relativeMousePos = ImVec2(mousePos.x - (windowPos.x + contentMin.x), mousePos.y - (windowPos.y + contentMin.y));
    sceneWidth = contentMax.x - contentMin.x;
    sceneHeight = contentMax.y - contentMin.y;
    isHovering = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
}

void ScenePanel::HandleKeyboardInput() {
    // Get the PlayControlPanel to modify its state
    auto playControlPanelPtr = GUIManager::GetPanelManager().GetPanel("Play Controls");
    auto playControlPanel = std::dynamic_pointer_cast<PlayControlPanel>(playControlPanelPtr);
    if (!playControlPanel) return;
    
    // Check keyboard input regardless of camera input conditions
    // But don't process gizmo shortcuts when right-click is held (rotating)
    bool isRightClickHeld = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    
    // Only process gizmo shortcuts when the ScenePanel window is focused
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !isRightClickHeld) {
        if (EditorInputManager::IsGizmoShortcutPressed(0)) {
            // Q key - Toggle pan mode
            if (playControlPanel->HasToolSelected() && playControlPanel->IsNormalPanMode()) {
                // Already in pan mode, deselect all tools
                playControlPanel->SetNormalPanMode(false);
                ENGINE_PRINT("[ScenePanel] Q pressed - Deselected all tools\n");
            } else {
                // Not in pan mode, switch to pan
                playControlPanel->SetNormalPanMode(true);
                ENGINE_PRINT("[ScenePanel] Q pressed - Switched to Pan mode\n");
            }
        }
        if (EditorInputManager::IsGizmoShortcutPressed(1)) {
            // W key - Toggle translate mode
            if (playControlPanel->HasToolSelected() && !playControlPanel->IsNormalPanMode() && playControlPanel->GetGizmoOperation() == ImGuizmo::TRANSLATE) {
                // Already in translate mode, deselect all tools
                playControlPanel->SetNormalPanMode(false);
                ENGINE_PRINT("[ScenePanel] W pressed - Deselected all tools\n");
            } else {
                // Not in translate mode, switch to translate
                playControlPanel->SetNormalPanMode(false);
                playControlPanel->SetGizmoOperation(ImGuizmo::TRANSLATE);
                ENGINE_PRINT("[ScenePanel] W pressed - Switched to Translate mode\n");
            }
        }
        if (EditorInputManager::IsGizmoShortcutPressed(2)) {
            // E key - Toggle rotate mode
            if (playControlPanel->HasToolSelected() && !playControlPanel->IsNormalPanMode() && playControlPanel->GetGizmoOperation() == ImGuizmo::ROTATE) {
                // Already in rotate mode, deselect all tools
                playControlPanel->SetNormalPanMode(false);
                ENGINE_PRINT("[ScenePanel] E pressed - Deselected all tools\n");
            } else {
                // Not in rotate mode, switch to rotate
                playControlPanel->SetNormalPanMode(false);
                playControlPanel->SetGizmoOperation(ImGuizmo::ROTATE);
                ENGINE_PRINT("[ScenePanel] E pressed - Switched to Rotate mode\n");
            }
        }
        if (EditorInputManager::IsGizmoShortcutPressed(3)) {
            // R key - Toggle scale mode
            if (playControlPanel->HasToolSelected() && !playControlPanel->IsNormalPanMode() && playControlPanel->GetGizmoOperation() == ImGuizmo::SCALE) {
                // Already in scale mode, deselect all tools
                playControlPanel->SetNormalPanMode(false);
                ENGINE_PRINT("[ScenePanel] R pressed - Deselected all tools\n");
            } else {
                // Not in scale mode, switch to scale
                playControlPanel->SetNormalPanMode(false);
                playControlPanel->SetGizmoOperation(ImGuizmo::SCALE);
                ENGINE_PRINT("[ScenePanel] R pressed - Switched to Scale mode\n");
            }
        }
    }

    // Handle Delete key for deleting selected entity (when scene is focused)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        Entity selectedEntity = GUIManager::GetSelectedEntity();
        if (selectedEntity != static_cast<Entity>(-1)) {
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                std::string entityName = ecsManager.GetComponent<NameComponent>(selectedEntity).name;

                ENGINE_PRINT("[ScenePanel] Deleting entity: ", entityName, " (ID: ", selectedEntity, ")\n");

                // Clear selection before deleting
                GUIManager::SetSelectedEntity(static_cast<Entity>(-1));

                // Delete the entity
                ecsManager.DestroyEntity(selectedEntity);

                ENGINE_PRINT("[ScenePanel] Entity deleted successfully\n");
            } catch (const std::exception& e) {
                ENGINE_PRINT("[ScenePanel] Failed to delete entity: ", e.what(), "\n");
            }
        }
    }
}

void ScenePanel::HandleCameraInput() {
    // Hover check is now handled by the caller

    // Get current mouse position (revert to original working method)
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 currentMousePos = glm::vec2(io.MousePos.x, io.MousePos.y);

    // Calculate mouse delta (original working logic)
    glm::vec2 mouseDelta(0.0f);
    if (!firstMouse) {
        mouseDelta = currentMousePos - lastMousePos;
    } else {
        firstMouse = false;
    }
    lastMousePos = currentMousePos;

    // Get input states
    bool isAltPressed = io.KeyAlt;
    bool isLeftMousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMiddleMousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    bool isRightMousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    float scrollDelta = io.MouseWheel;

    // Get the PlayControlPanel to check state
    auto playControlPanelPtr = GUIManager::GetPanelManager().GetPanel("Play Controls");
    auto playControlPanel = std::dynamic_pointer_cast<PlayControlPanel>(playControlPanelPtr);
    bool isNormalPanMode = playControlPanel ? playControlPanel->IsNormalPanMode() : false;

    if (isNormalPanMode) {
        isMiddleMousePressed = isLeftMousePressed;
        isLeftMousePressed = false;
        isAltPressed = false;
    }

    // Set base pan sensitivity based on view mode
    EditorState& editorState = EditorState::GetInstance();
    bool is2DMode = editorState.Is2DMode();

    if (is2DMode) {
        editorCamera.PanSensitivity = 2.5f; // Increased sensitivity for 2D (will be scaled by zoom in ProcessInput)
    } else {
        editorCamera.PanSensitivity = 0.005f; // Slower panning in 3D mode
    }

    // Set orbit sensitivity based on zoom level (distance) - more zoomed in = slower rotation
    editorCamera.OrbitSensitivity = 0.35f * (5.0f / (editorCamera.Distance + 5.0f));

    editorCamera.ProcessInput(
        io.DeltaTime,
        true,
        isAltPressed,
        isLeftMousePressed,
        isMiddleMousePressed,
        isRightMousePressed,
        mouseDelta.x,
        -mouseDelta.y,  // Invert Y for standard camera behavior
        scrollDelta,
        is2DMode
    );

    // WASD movement when right-click rotating
    if (isRightMousePressed && !is2DMode) {
        float moveSpeed = editorCamera.Distance * 0.01f; // Scale movement speed by distance
        
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            editorCamera.Position += editorCamera.Front * moveSpeed;
            editorCamera.Target += editorCamera.Front * moveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            editorCamera.Position -= editorCamera.Front * moveSpeed;
            editorCamera.Target -= editorCamera.Front * moveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            editorCamera.Position -= editorCamera.Right * moveSpeed;
            editorCamera.Target -= editorCamera.Right * moveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            editorCamera.Position += editorCamera.Right * moveSpeed;
            editorCamera.Target += editorCamera.Right * moveSpeed;
        }
        
        // Update camera vectors after movement
        editorCamera.UpdateCameraVectors();
    }
}

void ScenePanel::HandleEntitySelection() {
    // Hover check is now handled by the caller

    // Prevent selection if we just finished dragging with gizmo
    if (justFinishedGizmoDrag) {
        justFinishedGizmoDrag = false;
        return;
    }

    // Get the PlayControlPanel to check state
    auto playControlPanelPtr = GUIManager::GetPanelManager().GetPanel("Play Controls");
    auto playControlPanel = std::dynamic_pointer_cast<PlayControlPanel>(playControlPanelPtr);
    bool isNormalPanMode = playControlPanel ? playControlPanel->IsNormalPanMode() : false;
    
    // Skip entity selection in normal pan mode
    if (isNormalPanMode) {
        return;  // No entity selection in pan mode
    }

    // Only handle selection on left click (not during camera operations)
    ImGuiIO& io = ImGui::GetIO();
    bool isLeftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool isDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    bool isLeftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isLeftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool isAltPressed = io.KeyAlt;

    // Only select entities when left clicking without Alt (Alt is for camera orbit)
    if ((isLeftClicked || isDoubleClicked || isLeftDown || isLeftReleased) && !isAltPressed) {
        // Get mouse position relative to the scene window
        ImVec2 relativeMousePos;
        bool isHovering;
        float sceneWidth, sceneHeight;
        UpdateMouseState(relativeMousePos, isHovering, sceneWidth, sceneHeight);

        float relativeX = relativeMousePos.x;
        float relativeY = relativeMousePos.y;

        // Check if click is within scene bounds
        if (relativeX >= 0 && relativeX <= sceneWidth &&
            relativeY >= 0 && relativeY <= sceneHeight) {

            // Handle marquee selection
            if (isLeftClicked && !isDoubleClicked) {
                isMarqueeSelecting = false;
                marqueeStart = ImVec2(relativeX, relativeY);
                marqueeEnd = marqueeStart;
            }

            if (isLeftDown) {
                ImVec2 currentPos(relativeX, relativeY);
                if (!isMarqueeSelecting) {
                    float dx = currentPos.x - marqueeStart.x;
                    float dy = currentPos.y - marqueeStart.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist > 5.0f) {
                        isMarqueeSelecting = true;
                    }
                }
                if (isMarqueeSelecting) {
                    marqueeEnd = currentPos;
                }
            }

            if (isLeftReleased) {
                if (isMarqueeSelecting) {
                    isMarqueeSelecting = false;

                    // Calculate marquee bounds
                    float minX = std::min(marqueeStart.x, marqueeEnd.x);
                    float maxX = std::max(marqueeStart.x, marqueeEnd.x);
                    float minY = std::min(marqueeStart.y, marqueeEnd.y);
                    float maxY = std::max(marqueeStart.y, marqueeEnd.y);

                    // Marquee selection
                    std::vector<Entity> selectedEntities;
                    try {
                        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                        EditorState& editorState = EditorState::GetInstance();
                        bool is2DMode = editorState.Is2DMode();

                        float aspectRatio = sceneWidth / sceneHeight;
                        glm::mat4 glmViewMatrix = is2DMode ? editorCamera.Get2DViewMatrix() : editorCamera.GetViewMatrix();
                        glm::mat4 glmProjMatrix = is2DMode ? editorCamera.GetOrthographicProjectionMatrix(aspectRatio, sceneWidth, sceneHeight) : editorCamera.GetProjectionMatrix(aspectRatio);
                        glm::mat4 vp = glmProjMatrix * glmViewMatrix;

                        for (auto entity : ecsManager.GetActiveEntities()) {
                            if (ecsManager.HasComponent<Transform>(entity)) {
                                Transform& transform = ecsManager.GetComponent<Transform>(entity);
                                glm::vec3 worldPos(transform.worldMatrix.m.m03, transform.worldMatrix.m.m13, transform.worldMatrix.m.m23);

                                glm::vec4 clipSpace = vp * glm::vec4(worldPos, 1.0f);
                                if (clipSpace.w > 0.0001f) {
                                    glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
                                    float screenX = (ndc.x + 1.0f) * 0.5f * sceneWidth;
                                    float screenY = (1.0f - ndc.y) * 0.5f * sceneHeight;

                                    if (screenX >= minX && screenX <= maxX && screenY >= minY && screenY <= maxY) {
                                        selectedEntities.push_back(entity);
                                    }
                                }
                            }
                        }

                        if (!selectedEntities.empty()) {
                            GUIManager::SetSelectedEntities(selectedEntities);
                            ENGINE_PRINT("[ScenePanel] Marquee selected ", selectedEntities.size(), " entities\n");
                        } else {
                            GUIManager::ClearSelectedEntities();
                            ENGINE_PRINT("[ScenePanel] Marquee selection cleared\n");
                        }
                    } catch (const std::exception& e) {
                        ENGINE_PRINT("[ScenePanel] Error during marquee selection: ", e.what(), "\n");
                    }
                    return; // Skip raycast for marquee
                }
            }

            // Perform proper raycasting for entity selection
            EditorState& editorState = EditorState::GetInstance();
            bool is2DMode = editorState.Is2DMode();

            // Get camera matrices based on mode
            float aspectRatio = sceneWidth / sceneHeight;
            glm::mat4 glmViewMatrix;
            glm::mat4 glmProjMatrix;

            if (is2DMode) {
                // Use 2D orthographic matrices for 2D mode
                glmViewMatrix = editorCamera.Get2DViewMatrix();
                glmProjMatrix = editorCamera.GetOrthographicProjectionMatrix(aspectRatio, sceneWidth, sceneHeight);
            } else {
                // Use 3D perspective matrices for 3D mode
                glmViewMatrix = editorCamera.GetViewMatrix();
                glmProjMatrix = editorCamera.GetProjectionMatrix(aspectRatio);
            }

            // Convert GLM matrices to Matrix4x4 for raycast
            Matrix4x4 viewMatrix(
                glmViewMatrix[0][0], glmViewMatrix[1][0], glmViewMatrix[2][0], glmViewMatrix[3][0],
                glmViewMatrix[0][1], glmViewMatrix[1][1], glmViewMatrix[2][1], glmViewMatrix[3][1],
                glmViewMatrix[0][2], glmViewMatrix[1][2], glmViewMatrix[2][2], glmViewMatrix[3][2],
                glmViewMatrix[0][3], glmViewMatrix[1][3], glmViewMatrix[2][3], glmViewMatrix[3][3]
            );
            Matrix4x4 projMatrix(
                glmProjMatrix[0][0], glmProjMatrix[1][0], glmProjMatrix[2][0], glmProjMatrix[3][0],
                glmProjMatrix[0][1], glmProjMatrix[1][1], glmProjMatrix[2][1], glmProjMatrix[3][1],
                glmProjMatrix[0][2], glmProjMatrix[1][2], glmProjMatrix[2][2], glmProjMatrix[3][2],
                glmProjMatrix[0][3], glmProjMatrix[1][3], glmProjMatrix[2][3], glmProjMatrix[3][3]
            );

            // Cast ray from camera through mouse position
            RaycastUtil::Ray ray = RaycastUtil::ScreenToWorldRay(
                relativeX, relativeY,
                sceneWidth, sceneHeight,
                viewMatrix, projMatrix
            );

            // Perform raycast (filter for single-click, don't filter for double-click)
            bool shouldFilter = !isDoubleClicked;
            RaycastUtil::RaycastHit hit = RaycastUtil::RaycastScene(ray, INVALID_ENTITY, shouldFilter, is2DMode);

            if (hit.hit) {
                // Check if entity matches current mode
                bool entityIs3D = RaycastUtil::IsEntity3D(hit.entity);
                bool entityMatchesMode = (is2DMode && !entityIs3D) || (!is2DMode && entityIs3D);

                ENGINE_PRINT("[ScenePanel] Hit entity ", hit.entity, " - entityIs3D: ", entityIs3D,
                           ", currentMode is2D: ", is2DMode, ", matchesMode: ", entityMatchesMode,
                           ", isDoubleClick: ", isDoubleClicked, "\n");

                // Handle double-click: switch mode and focus
                if (isDoubleClicked) {
                    ENGINE_PRINT("[ScenePanel] Double-click detected!\n");

                    if (!entityMatchesMode) {
                        ENGINE_PRINT("[ScenePanel] Entity doesn't match mode - switching modes\n");
                        ENGINE_PRINT("[ScenePanel] Before switch - EditorState is2D: ", editorState.Is2DMode(), "\n");

                        // Entity is in opposite mode - switch mode
                        EditorState::ViewMode newViewMode = entityIs3D ? EditorState::ViewMode::VIEW_3D : EditorState::ViewMode::VIEW_2D;
                        editorState.SetViewMode(newViewMode);

                        ENGINE_PRINT("[ScenePanel] After EditorState.SetViewMode - is2D: ", editorState.Is2DMode(), "\n");

                        // Sync with GraphicsManager (important!)
                        GraphicsManager::ViewMode gfxMode = entityIs3D ? GraphicsManager::ViewMode::VIEW_3D : GraphicsManager::ViewMode::VIEW_2D;
                        GraphicsManager::GetInstance().SetViewMode(gfxMode);

                        ENGINE_PRINT("[ScenePanel] Double-click: Switched to ", (entityIs3D ? "3D" : "2D"), " mode\n");

                        // Update is2DMode for focus calculation
                        is2DMode = editorState.Is2DMode();
                        ENGINE_PRINT("[ScenePanel] Updated is2DMode to: ", is2DMode, "\n");
                    } else {
                        ENGINE_PRINT("[ScenePanel] Entity matches mode - no mode switch needed\n");
                    }

                    // Focus on the entity
                    try {
                        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                        if (ecsManager.HasComponent<Transform>(hit.entity)) {
                            Transform& transform = ecsManager.GetComponent<Transform>(hit.entity);
                            Vector3D targetPos(transform.worldMatrix.m.m03,
                                             transform.worldMatrix.m.m13,
                                             transform.worldMatrix.m.m23);

                            glm::vec3 entityPos(targetPos.x, targetPos.y, targetPos.z);

                            if (is2DMode) {
                                // Focus in 2D
                                editorCamera.Target = glm::vec3(targetPos.x, targetPos.y, 0.0f);
                            } else {
                                // Focus in 3D
                                editorCamera.FrameTarget(entityPos, 5.0f);
                            }
                            ENGINE_PRINT("[ScenePanel] Focused camera on entity ", hit.entity, "\n");
                        }
                    } catch (const std::exception& e) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScenePanel] Error focusing on entity: ", e.what(), "\n");
                    }
                }

                // Select the entity (for both single and double click)
                GUIManager::SetSelectedEntity(hit.entity);
                ENGINE_PRINT("[ScenePanel] Raycast hit entity ", hit.entity
                    , " at distance ", hit.distance, "\n");
            } else {
                // No entity hit, clear selection (only on single click)
                if (!isDoubleClicked) {
                    GUIManager::SetSelectedEntity(static_cast<Entity>(-1));
                    ENGINE_PRINT("[ScenePanel] Raycast missed - cleared selection\n");
                }
            }
            ENGINE_PRINT("[ScenePanel] Mouse clicked at (" , relativeX , ", " , relativeY
                , ") in scene bounds (", sceneWidth, "x", sceneHeight, ")\n"); 

        }
    }
}

void ScenePanel::OnImGuiRender()
{
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_VIEWPORT);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_VIEWPORT);

    // Update input manager state
    EditorInputManager::Update();

    if (ImGui::Begin(name.c_str(), &isOpen))
    {
        // Make every widget in this panel have a unique ID namespace
        ImGui::PushID(this); // <--- NEW

        // Handle input (but not if ImGuizmo is active)
        HandleKeyboardInput();

        bool isSceneHovered = false;

        // Content size for the scene view
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        int sceneViewWidth = (int)viewportPanelSize.x;
        int sceneViewHeight = (int)viewportPanelSize.y;
        if (sceneViewWidth < 100) sceneViewWidth = 100;
        if (sceneViewHeight < 100) sceneViewHeight = 100;

        // Optimize: Reduce render frequency when window is not focused
        bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        static int unfocusedFrameCounter = 0;
        bool shouldRender = true;

        if (!isFocused) {
            unfocusedFrameCounter++;
            // Render unfocused panel every 3rd frame instead of every frame
            if (unfocusedFrameCounter % 3 != 0) {
                shouldRender = false;
            }
        } else {
            unfocusedFrameCounter = 0;
        }

        // Render the scene with our editor camera to the framebuffer
        if (shouldRender) {
            RenderSceneWithEditorCamera(sceneViewWidth, sceneViewHeight);
        }

        // Scene texture from renderer
        unsigned int sceneTexture = SceneRenderer::GetSceneTexture();
        if (sceneTexture != 0)
        {
            // Child window that contains the scene image and gizmos
            ImGui::BeginChild(
                "SceneView##ScenePanel", // <--- UNIQUE NAME
                ImVec2((float)sceneViewWidth, (float)sceneViewHeight),
                false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
            );

            // Where to draw
            ImVec2 childPos = ImGui::GetCursorScreenPos();
            ImVec2 childSize = ImGui::GetContentRegionAvail();

            // Draw the scene texture as background (flip V for OpenGL)
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddImage(
                (void*)(intptr_t)sceneTexture,
                childPos,
                ImVec2(childPos.x + childSize.x, childPos.y + childSize.y),
                ImVec2(0, 1), ImVec2(1, 0)
            );

            // In 2D mode, draw game viewport bounds indicator (like Unity)
            EditorState& editorState = EditorState::GetInstance();
            if (editorState.Is2DMode()) {
                DrawGameViewportIndicator();
            }

            // Draw marquee selection box if active
            if (isMarqueeSelecting) {
                ImVec2 startPos = ImVec2(childPos.x + marqueeStart.x, childPos.y + marqueeStart.y);
                ImVec2 endPos = ImVec2(childPos.x + marqueeEnd.x, childPos.y + marqueeEnd.y);
                ImU32 marqueeColor = IM_COL32(0, 120, 255, 100);  // Semi-transparent blue
                dl->AddRect(startPos, endPos, marqueeColor, 0.0f, 0, 2.0f);
            }

            // Hover state for input routing
            // Use flags to ensure hover works correctly when docked with other panels
            isSceneHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

            // Auto-focus on interaction (any mouse click)
            if (isSceneHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                   ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                                   ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
                ImGui::SetWindowFocus();
            }

            // Cache matrices for performance
            cachedViewMatrix = editorCamera.GetViewMatrix();
            cachedProjectionMatrix = editorCamera.GetProjectionMatrix(static_cast<float>(sceneViewWidth) / sceneViewHeight);
            cachedWindowSize = ImVec2((float)sceneViewWidth, (float)sceneViewHeight);

            // ImGuizmo manipulation inside the child
            HandleImGuizmoInChildWindow((float)sceneViewWidth, (float)sceneViewHeight);

            // Draw collider gizmos for selected entity
            DrawColliderGizmos();
            DrawCameraGizmos();
            DrawAudioGizmos();
            DrawLightGizmos();

            // Draw selection outline for selected entities
            auto selectedEntities = GUIManager::GetSelectedEntities();
            for (auto entity : selectedEntities) {
                DrawSelectionOutline(entity, sceneViewWidth, sceneViewHeight);
            }

            // View gizmo in the corner
            RenderViewGizmo((float)sceneViewWidth, (float)sceneViewHeight);

            // Handle model drag-and-drop (must be inside child window)
            HandleModelDragDrop((float)sceneViewWidth, (float)sceneViewHeight);

            ImGui::EndChild();
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Scene View - Framebuffer not ready");
            ImGui::Text("Size: %d x %d", sceneViewWidth, sceneViewHeight);
        }

        // Render model preview overlay if dragging
        if (isDraggingModel) {
            RenderModelPreview((float)sceneViewWidth, (float)sceneViewHeight);
        }

        // Route input to camera/selection when not interacting with gizmos or dragging
        const bool hasActiveDragPayload = ImGui::GetDragDropPayload() != nullptr;
        const bool canHandleInput = isSceneHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !isDraggingModel && !hasActiveDragPayload;
        if (canHandleInput)
        {
            HandleCameraInput();
            HandleEntitySelection();
        }

        ImGui::PopID(); // <--- NEW
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
}

void ScenePanel::AcceptPrefabDropInScene(const ImVec2& sceneTopLeft, const ImVec2& sceneSize)
{
    // Make the whole scene image a drop target
    ImGui::SetCursorScreenPos(sceneTopLeft);
    ImGui::InvisibleButton("##ScenePrefabDropTarget", sceneSize, ImGuiButtonFlags_MouseButtonLeft);

    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_PATH"))
    {
        // Payload is a null-terminated C string we set in the Asset Browser
        const char* pathCStr = static_cast<const char*>(payload->Data);
        std::filesystem::path prefabPath(pathCStr);

        // Create an entity immediately so the user gets feedback
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        Entity e = ecs.CreateEntity();

        // Give it a friendly name based on the file name
        std::string displayName = prefabPath.stem().string();
        if (ecs.HasComponent<NameComponent>(e))
        {
            ecs.GetComponent<NameComponent>(e).name = displayName;
        }
        else
        {
            ecs.AddComponent<NameComponent>(e, NameComponent{ displayName });
        }

        // TODO (optional): actually instantiate the prefab contents here
        // AssetManager& assets = AssetManager::GetInstance();
        // Prefab prefab = assets.LoadPrefab(prefabPath.string()); // your loader
        // prefab.instantiatePrefab(ecs, static_cast<EntityID>(e));

        // Simple console feedback
        ENGINE_PRINT("[ScenePanel] Spawned entity from prefab: ", prefabPath, " -> entity ", (uint64_t)e, "\n");
    }

    ImGui::EndDragDropTarget();
}

void ScenePanel::RenderSceneWithEditorCamera(int width, int height) {
    try {
        auto& gfx = GraphicsManager::GetInstance();

        // Set viewport size for correct aspect ratio
        gfx.SetViewportSize(width, height);

        // Pass our editor camera data to the rendering system
        SceneRenderer::BeginSceneRender(width, height);

        // Update frustum with Scene Panel's viewport BEFORE rendering
        gfx.UpdateFrustum();

        SceneRenderer::RenderSceneForEditor(
            editorCamera.Position,
            editorCamera.Front,
            editorCamera.Up,
            editorCamera.Zoom,
            editorCamera.OrthoZoomLevel
        );
        SceneRenderer::EndSceneRender();

        // Now both the visual representation AND ImGuizmo overlay use our editor camera
        // This gives us proper editor controls

    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Exception in RenderSceneWithEditorCamera: ", e.what(), "\n");
        //std::cerr << "Exception in RenderSceneWithEditorCamera: " << e.what() << std::endl;
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


    // Get matrices from editor camera
    float aspectRatio = sceneWidth / sceneHeight;
    glm::mat4 view = editorCamera.GetViewMatrix();
    glm::mat4 projection = editorCamera.GetProjectionMatrix(aspectRatio);

    float viewMatrix[16], projMatrix[16];
    Mat4ToFloatArray(view, viewMatrix);
    Mat4ToFloatArray(projection, projMatrix);

    // Draw grid (only in 3D mode)
    EditorState& editorState = EditorState::GetInstance();
    if (!editorState.Is2DMode()) {
        ImGuizmo::DrawGrid(viewMatrix, projMatrix, identityMatrix, 10.0f);
    }

    // Get the PlayControlPanel to check state and get gizmo operation
    auto playControlPanelPtr = GUIManager::GetPanelManager().GetPanel("Play Controls");
    auto playControlPanel = std::dynamic_pointer_cast<PlayControlPanel>(playControlPanelPtr);
    bool isNormalPanMode = playControlPanel ? playControlPanel->IsNormalPanMode() : false;
    ImGuizmo::OPERATION gizmoOperation = playControlPanel ? playControlPanel->GetGizmoOperation() : ImGuizmo::TRANSLATE;

    // Only show gizmo when entities are selected AND not in normal pan mode
    auto selectedEntities = GUIManager::GetSelectedEntities();
    if (!selectedEntities.empty() && !isNormalPanMode) {
        // Check if entities should show gizmo based on 2D/3D mode
        // Commented out to fix warning C4456 - duplicate declaration (already declared on line 841)
        // EditorState& editorState = EditorState::GetInstance();
        bool is2DMode = editorState.Is2DMode();

        // Check if all selected entities are compatible (all 2D or all 3D)
        bool all2D = true;
        bool all3D = true;
        for (auto entity : selectedEntities) {
            bool is3D = RaycastUtil::IsEntity3D(entity);
            if (is3D) all2D = false;
            else all3D = false;
        }

        // In 2D mode, only show gizmo for 2D entities
        // In 3D mode, only show gizmo for 3D entities
        bool shouldShowGizmo = (is2DMode && all2D) || (!is2DMode && all3D);

        if (!shouldShowGizmo) {
            ImGui::PopID();
            return; // Skip gizmo rendering
        }

        // Compute pivot matrix for gizmo
        static float selectedObjectMatrix[16];
        glm::vec3 avgPos(0.0f);
        bool hasValidEntities = false;

        for (auto entity : selectedEntities) {
            float matrix[16];
            if (RaycastUtil::GetEntityTransform(entity, matrix)) {
                avgPos += glm::vec3(matrix[12], matrix[13], matrix[14]);
                hasValidEntities = true;
            }
        }

        if (!hasValidEntities) {
            ImGui::PopID();
            return;
        }

        avgPos /= static_cast<float>(selectedEntities.size());

        // Use rotation and scale from first entity
        float firstMatrix[16];
        RaycastUtil::GetEntityTransform(selectedEntities[0], firstMatrix);

        // Set position to average position, keep rotation and scale
        memcpy(selectedObjectMatrix, firstMatrix, sizeof(selectedObjectMatrix));
        selectedObjectMatrix[12] = avgPos.x;
        selectedObjectMatrix[13] = avgPos.y;
        selectedObjectMatrix[14] = avgPos.z;

        ImGuizmo::Manipulate(
            viewMatrix, projMatrix,
            gizmoOperation, gizmoMode,
            selectedObjectMatrix,
            nullptr, nullptr
        );

        // Use ImGuizmo::IsUsing() for reliable state detection (doesn't flicker during drag)
        bool isUsing = ImGuizmo::IsUsing();

        // Track gizmo manipulation for undo/redo

        // When user STARTS dragging: take ONE snapshot and disable auto-snapshots from Inspector
        if (isUsing && !gizmoWasUsing && !gizmoSnapshotTaken) {
            SnapshotManager::GetInstance().TakeSnapshot("Transform Entities");
            SnapshotManager::GetInstance().SetSnapshotEnabled(false);  // Disable Inspector snapshots
            gizmoSnapshotTaken = true;
            // Commented out to fix warning C4189 - unused variable
            // Entity lastManipulatedEntity = selectedEntities[0];  // Use first for tracking

            // Store original matrices for all selected entities
            originalMatrices.clear();
            for (auto entity : selectedEntities) {
                std::array<float, 16> mat;
                RaycastUtil::GetEntityTransform(entity, mat.data());
                originalMatrices.push_back(mat);
            }
            memcpy(originalPivot.data(), selectedObjectMatrix, sizeof(selectedObjectMatrix));
        }

        // When user STOPS dragging: re-enable auto-snapshots and reset flag
        if (!isUsing && gizmoWasUsing) {
            SnapshotManager::GetInstance().SetSnapshotEnabled(true);  // Re-enable Inspector snapshots
            gizmoSnapshotTaken = false;  // Reset for next drag operation
            justFinishedGizmoDrag = true;  // Prevent accidental selection on release
            originalMatrices.clear();  // Free memory
        }

        gizmoWasUsing = isUsing;

        // Apply transform changes to the actual entities
        if (isUsing) {
            // Compute delta transform from original pivot to new pivot
            glm::mat4 origPivotMat = glm::make_mat4(originalPivot.data());
            glm::mat4 newPivotMat = glm::make_mat4(selectedObjectMatrix);
            glm::mat4 delta = newPivotMat * glm::inverse(origPivotMat);

            // Apply delta to each selected entity
            for (size_t i = 0; i < selectedEntities.size(); ++i) {
                glm::mat4 origEntMat = glm::make_mat4(originalMatrices[i].data());
                glm::mat4 newEntMat = delta * origEntMat;

                float newMat[16];
                Mat4ToFloatArray(newEntMat, newMat);
                RaycastUtil::SetEntityTransform(selectedEntities[i], newMat);
            }
        }
    }

    // Draw light direction gizmos for selected light entities
    Entity selectedEntity = selectedEntities.empty() ? static_cast<Entity>(-1) : selectedEntities[0];
    if (selectedEntity != static_cast<Entity>(-1)) {
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

            // Check if selected entity is a directional light
            if (ecsManager.HasComponent<DirectionalLightComponent>(selectedEntity)) {
                DirectionalLightComponent& light = ecsManager.GetComponent<DirectionalLightComponent>(selectedEntity);

                if (light.enabled) {
                    // Use the SAME matrix that ImGuizmo uses for the selected object
                    static float selectedObjectMatrix[16];
                    if (!RaycastUtil::GetEntityTransform(selectedEntity, selectedObjectMatrix)) {
                        memcpy(selectedObjectMatrix, identityMatrix, sizeof(selectedObjectMatrix));
                    }

                    // Create a direction vector matrix for the arrow
                    float lightDirMatrix[16];
                    memcpy(lightDirMatrix, selectedObjectMatrix, sizeof(lightDirMatrix));

                    // Transform the light direction by the entity's rotation and add it to position
                    glm::mat4 entityMat = glm::mat4(
                        selectedObjectMatrix[0], selectedObjectMatrix[4], selectedObjectMatrix[8], selectedObjectMatrix[12],
                        selectedObjectMatrix[1], selectedObjectMatrix[5], selectedObjectMatrix[9], selectedObjectMatrix[13],
                        selectedObjectMatrix[2], selectedObjectMatrix[6], selectedObjectMatrix[10], selectedObjectMatrix[14],
                        selectedObjectMatrix[3], selectedObjectMatrix[7], selectedObjectMatrix[11], selectedObjectMatrix[15]
                    );

                    // Apply rotation to light direction
                    glm::vec4 localDir = glm::vec4(light.direction.ConvertToGLM(), 0.0f);
                    glm::vec4 worldDir = entityMat * localDir;
                    glm::vec3 normalizedWorldDir = glm::normalize(glm::vec3(worldDir));

                    // Create arrow end position
                    glm::vec3 entityPos = glm::vec3(entityMat[3]);
                    glm::vec3 arrowEndPos = entityPos + normalizedWorldDir * 1.5f;

                    // Set the arrow end position in matrix
                    lightDirMatrix[12] = arrowEndPos.x;
                    lightDirMatrix[13] = arrowEndPos.y;
                    lightDirMatrix[14] = arrowEndPos.z;

                    // Use ImDrawList to draw the light direction arrow
                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    // Get screen positions for custom drawing
                    ImVec2 windowPosCust = ImGui::GetWindowPos();
                    ImVec2 windowSizeCust = ImGui::GetWindowSize();

                    // Transform positions to screen space manually for the arrow
                    glm::mat4 matView = glm::mat4(
                        viewMatrix[0], viewMatrix[4], viewMatrix[8], viewMatrix[12],
                        viewMatrix[1], viewMatrix[5], viewMatrix[9], viewMatrix[13],
                        viewMatrix[2], viewMatrix[6], viewMatrix[10], viewMatrix[14],
                        viewMatrix[3], viewMatrix[7], viewMatrix[11], viewMatrix[15]
                    );

                    glm::mat4 proj = glm::mat4(
                        projMatrix[0], projMatrix[4], projMatrix[8], projMatrix[12],
                        projMatrix[1], projMatrix[5], projMatrix[9], projMatrix[13],
                        projMatrix[2], projMatrix[6], projMatrix[10], projMatrix[14],
                        projMatrix[3], projMatrix[7], projMatrix[11], projMatrix[15]
                    );

                    glm::vec4 startScreen = proj * matView * glm::vec4(entityPos, 1.0f);
                    glm::vec4 endScreen = proj * matView * glm::vec4(arrowEndPos, 1.0f);

                    if (startScreen.w > 0 && endScreen.w > 0) {
                        // Convert to screen coordinates
                        ImVec2 startPos = ImVec2(
                            windowPosCust.x + (startScreen.x / startScreen.w * 0.5f + 0.5f) * windowSizeCust.x,
                            windowPosCust.y + (1.0f - (startScreen.y / startScreen.w * 0.5f + 0.5f)) * windowSizeCust.y
                        );
                        ImVec2 endPos = ImVec2(
                            windowPosCust.x + (endScreen.x / endScreen.w * 0.5f + 0.5f) * windowSizeCust.x,
                            windowPosCust.y + (1.0f - (endScreen.y / endScreen.w * 0.5f + 0.5f)) * windowSizeCust.y
                        );

                        // Draw arrow
                        drawList->AddLine(startPos, endPos, IM_COL32(255, 255, 0, 255), 4.0f);

                        // Arrow head
                        ImVec2 dir = ImVec2(endPos.x - startPos.x, endPos.y - startPos.y);
                        float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (len > 0) {
                            dir.x /= len;
                            dir.y /= len;
                            ImVec2 perp = ImVec2(-dir.y, dir.x);

                            ImVec2 head1 = ImVec2(endPos.x - dir.x * 15 + perp.x * 8, endPos.y - dir.y * 15 + perp.y * 8);
                            ImVec2 head2 = ImVec2(endPos.x - dir.x * 15 - perp.x * 8, endPos.y - dir.y * 15 - perp.y * 8);

                            drawList->AddLine(endPos, head1, IM_COL32(255, 255, 0, 255), 3.0f);
                            drawList->AddLine(endPos, head2, IM_COL32(255, 255, 0, 255), 3.0f);
                        }

                        // Light icon at start position
                        drawList->AddCircleFilled(startPos, 10.0f, IM_COL32(255, 255, 100, 180));
                        drawList->AddCircle(startPos, 10.0f, IM_COL32(255, 255, 0, 255), 0, 2.0f);
                    }
                }
            }
        } catch (const std::exception& e) {
            ENGINE_PRINT("[ScenePanel] Failed to delete entity: ", e.what(), "\n");
        }
    }

    // Pop the ID scope
    ImGui::PopID();
}

void ScenePanel::RenderViewGizmo(float sceneWidth, float sceneHeight) {
    // Get matrices from editor camera
    float aspectRatio = sceneWidth / sceneHeight;
    glm::mat4 view = editorCamera.GetViewMatrix();
    glm::mat4 projection = editorCamera.GetProjectionMatrix(aspectRatio);

    float viewMatrix[16], projMatrix[16];
    Mat4ToFloatArray(view, viewMatrix);
    Mat4ToFloatArray(projection, projMatrix);

    // Position the ViewGizmo in the top right corner
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    float gizmoSize = 100.0f; // Size of the ViewGizmo
    float margin = 10.0f;     // Margin from the edges

    // Calculate position for top right corner
    float gizmoX = windowPos.x + windowSize.x - gizmoSize - margin;
    float gizmoY = windowPos.y + margin;

    // Set the ViewGizmo position and size
    ImGuizmo::SetRect(gizmoX, gizmoY, gizmoSize, gizmoSize);

    // Create a copy of the view matrix for manipulation
    float manipulatedViewMatrix[16];
    memcpy(manipulatedViewMatrix, viewMatrix, sizeof(manipulatedViewMatrix));

    // Render the ViewGizmo
    ImGuizmo::ViewManipulate(manipulatedViewMatrix, 8.0f,
                             ImVec2(gizmoX, gizmoY),
                             ImVec2(gizmoSize, gizmoSize),
                             0x10101010);

    // Check if the ViewGizmo was manipulated this frame
    bool wasManipulated = ImGuizmo::IsUsingViewManipulate();

    // Only update camera if ViewGizmo was actively manipulated this frame
    if (wasManipulated) {
        // Convert the manipulated view matrix back to orbit camera parameters
        glm::mat4 newViewMatrix;
        for (int i = 0; i < 16; ++i) {
            glm::value_ptr(newViewMatrix)[i] = manipulatedViewMatrix[i];
        }

        // Extract camera position and orientation from the inverse view matrix
        glm::mat4 inverseView = glm::inverse(newViewMatrix);
        glm::vec3 newPosition = glm::vec3(inverseView[3]);
        glm::vec3 newFront = -glm::normalize(glm::vec3(inverseView[2]));
        glm::vec3 newUp = glm::normalize(glm::vec3(inverseView[1]));

        // For orbit camera, we need to maintain the target point
        // Calculate the new target by projecting forward from the new position
        // Use the current distance to maintain zoom level
        glm::vec3 newTarget = newPosition + newFront * editorCamera.Distance;

        // Calculate new yaw and pitch relative to the target
        glm::vec3 offset = newPosition - newTarget;
        float newDistance = glm::length(offset);

        // Calculate yaw (horizontal rotation around Y axis)
        float newYaw = glm::degrees(atan2(offset.x, offset.z));

        // Calculate pitch (vertical rotation)
        float horizontalDistance = sqrt(offset.x * offset.x + offset.z * offset.z);
        float newPitch = glm::degrees(atan2(offset.y, horizontalDistance));

        // Update the editor camera's orbit parameters
        editorCamera.Position = newPosition;
        editorCamera.Front = newFront;
        editorCamera.Up = newUp;
        editorCamera.Target = newTarget;
        editorCamera.Yaw = newYaw;
        editorCamera.Pitch = newPitch;
        editorCamera.Distance = newDistance;
    }
}

void ScenePanel::HandleModelDragDrop(float sceneWidth, float sceneHeight) {
    // Check if there's an active MODEL_DRAG operation
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    bool isModelPayloadActive = (payload != nullptr && payload->IsDataType("MODEL_DRAG"));
    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // Check if we're hovering over this window
    bool isHovering = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    // Start drag when MODEL_DRAG payload is over the scene and mouse is down
    if (isModelPayloadActive && isMouseDown && isHovering && !isDraggingModel) {
        isDraggingModel = true;
        previewModelGUID = DraggedModelGuid;
        previewModelPath = DraggedModelPath;
        lastDragMousePos = ImVec2(-1.0f, -1.0f); // Reset cache

        // Create preview entity
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
            previewEntity = ecsManager.CreateEntity();

            if (ecsManager.HasComponent<NameComponent>(previewEntity)) {
                ecsManager.GetComponent<NameComponent>(previewEntity).name = "PREVIEW";
            }

            // Add ModelRenderComponent with semi-transparent material
            ModelRenderComponent previewRenderer;
            previewRenderer.model = ResourceManager::GetInstance().GetResource<Model>(previewModelPath);
            previewRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"));

            // Create ghost material
            auto ghostMaterial = std::make_shared<Material>();
            ghostMaterial->SetDiffuse(glm::vec3(0.7f, 1.0f, 0.7f)); // Green tint
            ghostMaterial->SetOpacity(0.5f);
            previewRenderer.material = ghostMaterial;

            ecsManager.AddComponent<ModelRenderComponent>(previewEntity, previewRenderer);

            ENGINE_PRINT("[ScenePanel] Started dragging model: ", previewModelPath, "\n");
        } catch (const std::exception& e) {
            ENGINE_PRINT("[ScenePanel] Failed to create preview entity: ", e.what(), "\n");
            isDraggingModel = false;
        }
    }

    // Handle dragging state FIRST (before cleanup)
    if (isDraggingModel) {
        // Get mouse position relative to scene window
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();

        float relativeX = mousePos.x - (windowPos.x + contentMin.x);
        float relativeY = mousePos.y - (windowPos.y + contentMin.y);

        // Optimization: Only update if mouse moved significantly (more than 1 pixel)
        float dx = relativeX - lastDragMousePos.x;
        float dy = relativeY - lastDragMousePos.y;
        bool mouseMoved = (dx * dx + dy * dy) > 1.0f;

        if (mouseMoved || lastDragMousePos.x < 0) {
            lastDragMousePos = ImVec2(relativeX, relativeY);

            // Perform raycast to find preview position
            if (relativeX >= 0 && relativeX <= sceneWidth && relativeY >= 0 && relativeY <= sceneHeight) {
            float aspectRatio = sceneWidth / sceneHeight;
            glm::mat4 glmViewMatrix = editorCamera.GetViewMatrix();
            glm::mat4 glmProjMatrix = editorCamera.GetProjectionMatrix(aspectRatio);

            // Convert to Matrix4x4 for raycast
            Matrix4x4 viewMatrix(
                glmViewMatrix[0][0], glmViewMatrix[1][0], glmViewMatrix[2][0], glmViewMatrix[3][0],
                glmViewMatrix[0][1], glmViewMatrix[1][1], glmViewMatrix[2][1], glmViewMatrix[3][1],
                glmViewMatrix[0][2], glmViewMatrix[1][2], glmViewMatrix[2][2], glmViewMatrix[3][2],
                glmViewMatrix[0][3], glmViewMatrix[1][3], glmViewMatrix[2][3], glmViewMatrix[3][3]
            );
            Matrix4x4 projMatrix(
                glmProjMatrix[0][0], glmProjMatrix[1][0], glmProjMatrix[2][0], glmProjMatrix[3][0],
                glmProjMatrix[0][1], glmProjMatrix[1][1], glmProjMatrix[2][1], glmProjMatrix[3][1],
                glmProjMatrix[0][2], glmProjMatrix[1][2], glmProjMatrix[2][2], glmProjMatrix[3][2],
                glmProjMatrix[0][3], glmProjMatrix[1][3], glmProjMatrix[2][3], glmProjMatrix[3][3]
            );

            RaycastUtil::Ray ray = RaycastUtil::ScreenToWorldRay(
                relativeX, relativeY,
                sceneWidth, sceneHeight,
                viewMatrix, projMatrix
            );

            // Raycast against scene to find placement position (exclude preview entity, filter by 2D/3D mode)
            EditorState& editorState = EditorState::GetInstance();
            bool is2DMode = editorState.Is2DMode();
            RaycastUtil::RaycastHit hit = RaycastUtil::RaycastScene(ray, previewEntity, true, is2DMode);

            if (hit.hit) {
                // Hit an object - place on surface
                previewPosition = hit.point;
                previewValidPlacement = true;
            } else {
                // No hit - place at fixed distance from camera
                previewPosition = ray.origin + ray.direction * 5.0f;
                previewValidPlacement = true;
            }

            // Update preview entity position
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                if (previewEntity != static_cast<Entity>(-1) && ecsManager.HasComponent<Transform>(previewEntity)) {
                    Transform& transform = ecsManager.GetComponent<Transform>(previewEntity);
                    transform.localPosition = Vector3D(previewPosition.x, previewPosition.y, previewPosition.z);
                    transform.localScale = Vector3D(0.1f, 0.1f, 0.1f);
                    transform.isDirty = true;
                }
            } catch (const std::exception& e) {
                ENGINE_PRINT("[ScenePanel] Failed to update preview position: ", e.what(), "\n");
            }
        }
        }

        // Check if mouse released to spawn entity (only if over scene panel)
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isHovering) {
            // IMPORTANT: Delete preview entity FIRST, before spawning the real entity
            // This avoids a bug where destroying entity N also destroys entity N+1
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                if (previewEntity != static_cast<Entity>(-1)) {
                    ecsManager.DestroyEntity(previewEntity);
                    previewEntity = static_cast<Entity>(-1);
                }
            } catch (const std::exception& e) {
                ENGINE_PRINT("[ScenePanel] Failed to delete preview entity: ", e.what(), "\n");
            }

            // Then spawn the real entity
            Entity realEntity = SpawnModelEntity(previewPosition);
            if (realEntity != static_cast<Entity>(-1)) {
                ENGINE_PRINT("[ScenePanel] Successfully spawned entity ", realEntity, "\n");
            } else {
                ENGINE_PRINT("[ScenePanel] ERROR: SpawnModelEntity returned invalid entity\n");
            }

            isDraggingModel = false;
        }

        // Cancel drag on ESC
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ENGINE_PRINT("[ScenePanel] Drag cancelled\n");

            // Delete preview entity
            try {
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
                if (previewEntity != static_cast<Entity>(-1)) {
                    ecsManager.DestroyEntity(previewEntity);
                    previewEntity = static_cast<Entity>(-1);
                }
            } catch (const std::exception& e) {
                ENGINE_PRINT("[ScenePanel] Failed to delete preview entity: ", e.what(), "\n");
            }

            isDraggingModel = false;
            lastDragMousePos = ImVec2(-1.0f, -1.0f); // Reset cache
        }
    }

    // Cleanup: Stop drag if MODEL_DRAG payload is gone or mouse is released
    if ((!isModelPayloadActive || !isMouseDown) && isDraggingModel) {
        ENGINE_PRINT("[ScenePanel] Drag ended - cleaning up preview\n");

        // Delete preview entity
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
            if (previewEntity != static_cast<Entity>(-1)) {
                ecsManager.DestroyEntity(previewEntity);
                previewEntity = static_cast<Entity>(-1);
            }
        } catch (const std::exception& e) {
            ENGINE_PRINT("[ScenePanel] Failed to delete preview entity: ", e.what(), "\n");
        }

        isDraggingModel = false;
        lastDragMousePos = ImVec2(-1.0f, -1.0f); // Reset cache
    }
}

void ScenePanel::RenderModelPreview(float sceneWidth, float sceneHeight) {
    // Preview entity is now automatically rendered by the ECS rendering system
    // This function can be used for additional visual feedback if needed

    // Update material color based on valid placement
    if (previewEntity != static_cast<Entity>(-1)) {
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
            if (ecsManager.HasComponent<ModelRenderComponent>(previewEntity)) {
                ModelRenderComponent& renderer = ecsManager.GetComponent<ModelRenderComponent>(previewEntity);
                if (renderer.material) {
                    // Update color based on placement validity
                    renderer.material->SetDiffuse(previewValidPlacement ?
                        glm::vec3(0.7f, 1.0f, 0.7f) :  // Green tint for valid
                        glm::vec3(1.0f, 0.7f, 0.7f));   // Red tint for invalid
                }
            }
        } catch (const std::exception& e) {
            ENGINE_PRINT("[ScenePanel] Error updating preview material: ", e.what(), "\n");
        }
    }

    (void)sceneWidth, sceneHeight;
}

Entity ScenePanel::SpawnModelEntity(const glm::vec3& position) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Create entity with name from model file
        std::filesystem::path modelPath(previewModelPath);
        std::string entityName = modelPath.stem().string();

        Entity newEntity = ecsManager.CreateEntity();

        // Update the existing name component (CreateEntity already adds one)
        if (ecsManager.TryGetComponent<NameComponent>(newEntity).has_value()) {
            ecsManager.GetComponent<NameComponent>(newEntity).name = entityName;
        }

        // Set position from raycast (CreateEntity already adds Transform component)
        if (ecsManager.TryGetComponent<Transform>(newEntity).has_value()) {
            Transform& transform = ecsManager.GetComponent<Transform>(newEntity);
            transform.localPosition = Vector3D(position.x, position.y, position.z);
            transform.localScale = Vector3D(0.1f, 0.1f, 0.1f); // Same as cube
            transform.isDirty = true;
        }

        // Add ModelRenderComponent
        if (!ecsManager.TryGetComponent<ModelRenderComponent>(newEntity).has_value()) {
            ModelRenderComponent modelRenderer;
            modelRenderer.model = ResourceManager::GetInstance().GetResource<Model>(previewModelPath);
            modelRenderer.modelGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(previewModelPath);
            modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"));
            modelRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(ResourceManager::GetPlatformShaderPath("default"));
            if (modelRenderer.model->meshes[0].material) {
                modelRenderer.material = modelRenderer.model->meshes[0].material;
                std::string materialPath = AssetManager::GetInstance().GetAssetPathFromAssetName(modelRenderer.material->GetName() + ".mat");
                modelRenderer.materialGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(materialPath);
            }

            if (modelRenderer.model && modelRenderer.shader) {
                ecsManager.AddComponent<ModelRenderComponent>(newEntity, modelRenderer);

                // Select the newly created entity
                GUIManager::SetSelectedEntity(newEntity);

                ENGINE_PRINT("[ScenePanel] Spawned model entity ", entityName, " (ID: ", newEntity, ")\n");
                return newEntity;
            } else {
                ENGINE_PRINT("[ScenePanel] Failed to load model or shader for spawned entity\n");
                ecsManager.DestroyEntity(newEntity);
                return static_cast<Entity>(-1);
            }
        }

        // Entity already has ModelRenderComponent (shouldn't happen for new entities)
        GUIManager::SetSelectedEntity(newEntity);
        return newEntity;
    } catch (const std::exception& e) {
        ENGINE_PRINT("[ScenePanel] Error spawning model entity: ", e.what(), "\n");
        return static_cast<Entity>(-1);
    }
}

void ScenePanel::DrawColliderGizmos() {
    Entity selectedEntity = GUIManager::GetSelectedEntity();
    if (selectedEntity == static_cast<Entity>(-1)) return;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Check if entity has both Transform and Collider
        if (!ecsManager.HasComponent<Transform>(selectedEntity) ||
            !ecsManager.HasComponent<ColliderComponent>(selectedEntity)) {
            return;
        }

        Transform& transform = ecsManager.GetComponent<Transform>(selectedEntity);
        ColliderComponent& collider = ecsManager.GetComponent<ColliderComponent>(selectedEntity);

        // Get world position and scale from transform
        glm::vec3 worldPos = glm::vec3(transform.localPosition.x, transform.localPosition.y, transform.localPosition.z);
        glm::vec3 worldScale = glm::vec3(transform.localScale.x, transform.localScale.y, transform.localScale.z);

        glm::vec3 offset = { collider.center.x * worldScale.x, collider.center.y * worldScale.y, collider.center.z * worldScale.z };

        worldPos += offset; //THIS IS FOR THOSE ORIGIN IS NOT IN THE MIDDLE, IF MIDDLE, OFFSET = 0


        // Get viewport dimensions from current ImGui window
        ImVec2 windowSize = cachedWindowSize;
        if (windowSize.x == 0 || windowSize.y == 0) return;

        // Commented out to fix warning C4189 - unused variable
        // float aspectRatio = windowSize.x / windowSize.y;

        // Project to screen space
        glm::mat4 vp = cachedProjectionMatrix * cachedViewMatrix;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();

        // Green color for collider gizmos (like Unity)
        ImU32 gizmoColor = IM_COL32(0, 255, 0, 255);

        // Draw based on shape type
        switch (collider.shapeType) {
            case ColliderShapeType::Box: {
                // Draw wireframe box - apply transform scale to extents

                glm::vec3 extents = glm::vec3(
                    collider.boxHalfExtents.x * worldScale.x,
                    collider.boxHalfExtents.y * worldScale.y,
                    collider.boxHalfExtents.z * worldScale.z
                );

                // 8 corners of the box
                glm::vec3 corners[8] = {
                    worldPos + glm::vec3(-extents.x, -extents.y, -extents.z),
                    worldPos + glm::vec3( extents.x, -extents.y, -extents.z),
                    worldPos + glm::vec3( extents.x,  extents.y, -extents.z),
                    worldPos + glm::vec3(-extents.x,  extents.y, -extents.z),
                    worldPos + glm::vec3(-extents.x, -extents.y,  extents.z),
                    worldPos + glm::vec3( extents.x, -extents.y,  extents.z),
                    worldPos + glm::vec3( extents.x,  extents.y,  extents.z),
                    worldPos + glm::vec3(-extents.x,  extents.y,  extents.z),
                };

                // Project corners to screen with visibility check
                ImVec2 screenCorners[8];
                bool visible[8];
                for (int i = 0; i < 8; i++) {
                    screenCorners[i] = ProjectToScreen(corners[i], visible[i], vp, windowPos, windowSize);
                }

                // Draw 12 edges of the box - only draw if both endpoints are visible
                int edges[12][2] = {
                    {0,1}, {1,2}, {2,3}, {3,0}, // Bottom face
                    {4,5}, {5,6}, {6,7}, {7,4}, // Top face
                    {0,4}, {1,5}, {2,6}, {3,7}  // Vertical edges
                };

                for (int i = 0; i < 12; i++) {
                    int idx0 = edges[i][0];
                    int idx1 = edges[i][1];
                    if (visible[idx0] && visible[idx1]) {
                        drawList->AddLine(screenCorners[idx0], screenCorners[idx1], gizmoColor, 2.0f);
                    }
                }
                break;
            }

            case ColliderShapeType::Sphere: {
                // Draw wireframe sphere (3 orthogonal circles) - use max scale component
                float maxScale = glm::max(glm::max(worldScale.x, worldScale.y), worldScale.z);
                float radius = collider.sphereRadius * maxScale;
                int segments = 32;

                // XY plane circle
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / segments * 2.0f * 3.14159f;
                    float angle2 = (float)(i + 1) / segments * 2.0f * 3.14159f;

                    glm::vec3 p1 = worldPos + glm::vec3(cos(angle1) * radius, sin(angle1) * radius, 0);
                    glm::vec3 p2 = worldPos + glm::vec3(cos(angle2) * radius, sin(angle2) * radius, 0);

                    bool vis1, vis2;
                    ImVec2 s1 = ProjectToScreen(p1, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(p2, vis2, vp, windowPos, windowSize);
                    if (vis1 && vis2) {
                        drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    }
                }

                // XZ plane circle
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / segments * 2.0f * 3.14159f;
                    float angle2 = (float)(i + 1) / segments * 2.0f * 3.14159f;

                    glm::vec3 p1 = worldPos + glm::vec3(cos(angle1) * radius, 0, sin(angle1) * radius);
                    glm::vec3 p2 = worldPos + glm::vec3(cos(angle2) * radius, 0, sin(angle2) * radius);

                    bool vis1, vis2;
                    ImVec2 s1 = ProjectToScreen(p1, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(p2, vis2, vp, windowPos, windowSize);
                    if (vis1 && vis2) {
                        drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    }
                }

                // YZ plane circle
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / segments * 2.0f * 3.14159f;
                    float angle2 = (float)(i + 1) / segments * 2.0f * 3.14159f;

                    glm::vec3 p1 = worldPos + glm::vec3(0, cos(angle1) * radius, sin(angle1) * radius);
                    glm::vec3 p2 = worldPos + glm::vec3(0, cos(angle2) * radius, sin(angle2) * radius);

                    bool vis1, vis2;
                    ImVec2 s1 = ProjectToScreen(p1, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(p2, vis2, vp, windowPos, windowSize);
                    if (vis1 && vis2) {
                        drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    }
                }
                break;
            }

            case ColliderShapeType::Capsule: {
                // Draw wireframe capsule - scale radius by XZ, height by Y
                float radialScale = glm::max(worldScale.x, worldScale.z);
                float radius = collider.capsuleRadius * radialScale;
                float halfHeight = collider.capsuleHalfHeight * worldScale.y;
                int segments = 16;

                // Draw cylinder body (vertical lines)
                glm::vec3 top = worldPos + glm::vec3(0, halfHeight, 0);
                glm::vec3 bottom = worldPos - glm::vec3(0, halfHeight, 0);

                for (int i = 0; i < segments; i++) {
                    float angle = (float)i / segments * 2.0f * 3.14159f;
                    glm::vec3 offset(cos(angle) * radius, 0, sin(angle) * radius);

                    bool vis1, vis2;
                    ImVec2 s1 = ProjectToScreen(top + offset, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(bottom + offset, vis2, vp, windowPos, windowSize);
                    if (vis1 && vis2) {
                        drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    }
                }

                // Top and bottom circles
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / segments * 2.0f * 3.14159f;
                    float angle2 = (float)(i + 1) / segments * 2.0f * 3.14159f;

                    glm::vec3 p1 = glm::vec3(cos(angle1) * radius, 0, sin(angle1) * radius);
                    glm::vec3 p2 = glm::vec3(cos(angle2) * radius, 0, sin(angle2) * radius);

                    bool vis1, vis2, vis3, vis4;
                    ImVec2 s1 = ProjectToScreen(top + p1, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(top + p2, vis2, vp, windowPos, windowSize);
                    ImVec2 s3 = ProjectToScreen(bottom + p1, vis3, vp, windowPos, windowSize);
                    ImVec2 s4 = ProjectToScreen(bottom + p2, vis4, vp, windowPos, windowSize);

                    if (vis1 && vis2) drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    if (vis3 && vis4) drawList->AddLine(s3, s4, gizmoColor, 2.0f);
                }
                break;
            }

            case ColliderShapeType::Cylinder: {
                // Draw wireframe cylinder - scale radius by XZ, height by Y
                float radialScale = glm::max(worldScale.x, worldScale.z);
                float radius = collider.cylinderRadius * radialScale;
                float halfHeight = collider.cylinderHalfHeight * worldScale.y;
                int segments = 16;

                glm::vec3 top = worldPos + glm::vec3(0, halfHeight, 0);
                glm::vec3 bottom = worldPos - glm::vec3(0, halfHeight, 0);

                // Vertical edges
                for (int i = 0; i < segments; i++) {
                    float angle = (float)i / segments * 2.0f * 3.14159f;
                    glm::vec3 offset(cos(angle) * radius, 0, sin(angle) * radius);

                    bool vis1, vis2;
                    ImVec2 s1 = ProjectToScreen(top + offset, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(bottom + offset, vis2, vp, windowPos, windowSize);
                    if (vis1 && vis2) {
                        drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    }
                }

                // Top and bottom circles
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / segments * 2.0f * 3.14159f;
                    float angle2 = (float)(i + 1) / segments * 2.0f * 3.14159f;

                    glm::vec3 p1 = glm::vec3(cos(angle1) * radius, 0, sin(angle1) * radius);
                    glm::vec3 p2 = glm::vec3(cos(angle2) * radius, 0, sin(angle2) * radius);

                    bool vis1, vis2, vis3, vis4;
                    ImVec2 s1 = ProjectToScreen(top + p1, vis1, vp, windowPos, windowSize);
                    ImVec2 s2 = ProjectToScreen(top + p2, vis2, vp, windowPos, windowSize);
                    ImVec2 s3 = ProjectToScreen(bottom + p1, vis3, vp, windowPos, windowSize);
                    ImVec2 s4 = ProjectToScreen(bottom + p2, vis4, vp, windowPos, windowSize);

                    if (vis1 && vis2) drawList->AddLine(s1, s2, gizmoColor, 2.0f);
                    if (vis3 && vis4) drawList->AddLine(s3, s4, gizmoColor, 2.0f);
                }
                break;
            }
        }
    }
    catch (const std::exception& e) {
        ENGINE_PRINT("[ScenePanel] entity might be deleted: ", e.what(), "\n");
    }
}

void ScenePanel::DrawCameraGizmos() {
    Entity selectedEntity = GUIManager::GetSelectedEntity();
    if (selectedEntity == static_cast<Entity>(-1)) return;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Only draw if entity has a camera component
        if (!ecsManager.HasComponent<CameraComponent>(selectedEntity)) return;
        if (!ecsManager.HasComponent<Transform>(selectedEntity)) return;

        CameraComponent& camera = ecsManager.GetComponent<CameraComponent>(selectedEntity);
        Transform& transform = ecsManager.GetComponent<Transform>(selectedEntity);

        // Get game panel resolution for correct aspect ratio
        float aspectRatio = 16.0f / 9.0f;  // Default
        auto gamePanel = std::dynamic_pointer_cast<GamePanel>(GUIManager::GetPanelManager().GetPanel("Game"));
        if (gamePanel) {
            int gameWidth, gameHeight;
            gamePanel->GetTargetGameResolution(gameWidth, gameHeight);
            if (gameHeight > 0) {
                aspectRatio = static_cast<float>(gameWidth) / static_cast<float>(gameHeight);
            }
        }

        // Get window and viewport info for editor camera
        ImVec2 windowSize = cachedWindowSize;
        // Commented out to fix warning C4189 - unused variable
        // float editorAspectRatio = windowSize.x / windowSize.y;

        // Build editor view-projection matrix
        glm::mat4 vp = cachedProjectionMatrix * cachedViewMatrix;

        // Get camera world position from transform
        glm::vec3 camPos(transform.worldMatrix.m.m03, transform.worldMatrix.m.m13, transform.worldMatrix.m.m23);

        // Calculate camera forward, right, up vectors
        glm::vec3 camForward, camRight, camUp;
        if (camera.useFreeRotation) {
            // Use yaw/pitch to calculate direction
            float yawRad = glm::radians(camera.yaw);
            float pitchRad = glm::radians(camera.pitch);
            camForward.x = cos(yawRad) * cos(pitchRad);
            camForward.y = sin(pitchRad);
            camForward.z = sin(yawRad) * cos(pitchRad);
            camForward = glm::normalize(camForward);
        } else {
            // Use target direction
            camForward = glm::normalize(camera.target);
        }
        camRight = glm::normalize(glm::cross(camForward, camera.up));
        camUp = glm::normalize(glm::cross(camRight, camForward));

        // Get ImGui draw list
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();

        // Colors
        ImU32 frustumColor = IM_COL32(255, 255, 255, 255);  // White
        ImU32 directionColor = IM_COL32(255, 255, 255, 255);  // White

        // ==== 1. Draw Camera Direction Arrow ====
        glm::vec3 arrowEnd = camPos + camForward * 1.5f;
        bool vis1, vis2;
        ImVec2 startScreen = ProjectToScreen(camPos, vis1, vp, windowPos, windowSize);
        ImVec2 endScreen = ProjectToScreen(arrowEnd, vis2, vp, windowPos, windowSize);

        if (vis1 && vis2) {
            // Draw arrow line
            drawList->AddLine(startScreen, endScreen, directionColor, 3.0f);

            // Arrow head
            ImVec2 dir = ImVec2(endScreen.x - startScreen.x, endScreen.y - startScreen.y);
            float len = sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0) {
                dir.x /= len;
                dir.y /= len;
                ImVec2 perp = ImVec2(-dir.y, dir.x);

                ImVec2 head1 = ImVec2(endScreen.x - dir.x * 12 + perp.x * 6, endScreen.y - dir.y * 12 + perp.y * 6);
                ImVec2 head2 = ImVec2(endScreen.x - dir.x * 12 - perp.x * 6, endScreen.y - dir.y * 12 - perp.y * 6);

                drawList->AddLine(endScreen, head1, directionColor, 2.5f);
                drawList->AddLine(endScreen, head2, directionColor, 2.5f);
            }
        }

        // ==== 2. Draw Camera Frustum ====
        float nearDist = camera.nearPlane;
        float farDist = glm::min(camera.farPlane, 20.0f);  // Cap far plane for visualization

        float nearHeight, nearWidth, farHeight, farWidth;
        if (camera.projectionType == ProjectionType::PERSPECTIVE) {
            // Calculate frustum dimensions for perspective camera
            float fovRad = glm::radians(camera.fov);
            nearHeight = 2.0f * nearDist * tan(fovRad / 2.0f);
            nearWidth = nearHeight * aspectRatio;
            farHeight = 2.0f * farDist * tan(fovRad / 2.0f);
            farWidth = farHeight * aspectRatio;
        } else {
            // Orthographic camera - constant size
            nearHeight = camera.orthoSize * 2.0f;
            nearWidth = nearHeight * aspectRatio;
            farHeight = nearHeight;
            farWidth = nearWidth;
        }

        // Calculate frustum corner points in world space
        glm::vec3 nearCenter = camPos + camForward * nearDist;
        glm::vec3 farCenter = camPos + camForward * farDist;

        // Near plane corners
        glm::vec3 nearCorners[4] = {
            nearCenter + camUp * (nearHeight * 0.5f) - camRight * (nearWidth * 0.5f),  // Top-left
            nearCenter + camUp * (nearHeight * 0.5f) + camRight * (nearWidth * 0.5f),  // Top-right
            nearCenter - camUp * (nearHeight * 0.5f) + camRight * (nearWidth * 0.5f),  // Bottom-right
            nearCenter - camUp * (nearHeight * 0.5f) - camRight * (nearWidth * 0.5f)   // Bottom-left
        };

        // Far plane corners
        glm::vec3 farCorners[4] = {
            farCenter + camUp * (farHeight * 0.5f) - camRight * (farWidth * 0.5f),  // Top-left
            farCenter + camUp * (farHeight * 0.5f) + camRight * (farWidth * 0.5f),  // Top-right
            farCenter - camUp * (farHeight * 0.5f) + camRight * (farWidth * 0.5f),  // Bottom-right
            farCenter - camUp * (farHeight * 0.5f) - camRight * (farWidth * 0.5f)   // Bottom-left
        };

        // Project corners to screen
        ImVec2 nearScreenCorners[4];
        ImVec2 farScreenCorners[4];
        bool nearVisible[4], farVisible[4];

        for (int i = 0; i < 4; i++) {
            nearScreenCorners[i] = ProjectToScreen(nearCorners[i], nearVisible[i], vp, windowPos, windowSize);
            farScreenCorners[i] = ProjectToScreen(farCorners[i], farVisible[i], vp, windowPos, windowSize);
        }

        // Draw near plane rectangle
        for (int i = 0; i < 4; i++) {
            int next = (i + 1) % 4;
            if (nearVisible[i] && nearVisible[next]) {
                drawList->AddLine(nearScreenCorners[i], nearScreenCorners[next], frustumColor, 2.0f);
            }
        }

        // Draw far plane rectangle
        for (int i = 0; i < 4; i++) {
            int next = (i + 1) % 4;
            if (farVisible[i] && farVisible[next]) {
                drawList->AddLine(farScreenCorners[i], farScreenCorners[next], frustumColor, 2.0f);
            }
        }

        // Draw connecting lines (near to far)
        for (int i = 0; i < 4; i++) {
            if (nearVisible[i] && farVisible[i]) {
                drawList->AddLine(nearScreenCorners[i], farScreenCorners[i], frustumColor, 2.0f);
            }
        }

        // ==== 3. Draw Camera Icon ====
        bool camIconVis;
        ImVec2 iconPos = ProjectToScreen(camPos, camIconVis, vp, windowPos, windowSize);
        if (camIconVis) {
            // Draw camera body (rectangle)
            drawList->AddRectFilled(
                ImVec2(iconPos.x - 8, iconPos.y - 6),
                ImVec2(iconPos.x + 8, iconPos.y + 6),
                IM_COL32(200, 200, 200, 200)
            );
            drawList->AddRect(
                ImVec2(iconPos.x - 8, iconPos.y - 6),
                ImVec2(iconPos.x + 8, iconPos.y + 6),
                frustumColor,
                0.0f, 0, 2.0f
            );

            // Draw lens (circle)
            drawList->AddCircleFilled(iconPos, 4.0f, IM_COL32(150, 150, 150, 255));
            drawList->AddCircle(iconPos, 4.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
        }

    } catch (const std::exception& e) {
        ENGINE_PRINT("[ScenePanel] Error drawing camera gizmos: ", e.what(), "\n");
    }
}

void ScenePanel::DrawAudioGizmos() {
    Entity selectedEntity = GUIManager::GetSelectedEntity();
    if (selectedEntity == static_cast<Entity>(-1)) return;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Check if entity has transform
        if (!ecsManager.HasComponent<Transform>(selectedEntity)) return;
        Transform& transform = ecsManager.GetComponent<Transform>(selectedEntity);

        // Get audio source position from transform
        glm::vec3 audioPos(transform.worldMatrix.m.m03, transform.worldMatrix.m.m13, transform.worldMatrix.m.m23);

        // Check which audio component types this entity has
        bool hasAudioComponent = ecsManager.HasComponent<AudioComponent>(selectedEntity);
        bool hasReverbZone = ecsManager.HasComponent<AudioReverbZoneComponent>(selectedEntity);

        // Early exit if no audio components
        if (!hasAudioComponent && !hasReverbZone) return;

        // Get window and viewport info for editor camera
        ImVec2 windowSize = cachedWindowSize;
        // Commented out to fix warning C4189 - unused variable
        // float editorAspectRatio = windowSize.x / windowSize.y;

        // Build editor view-projection matrix
        glm::mat4 vp = cachedProjectionMatrix * cachedViewMatrix;

        // Get ImGui draw list
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();

        // Lambda to draw sphere wireframes
        auto drawWireSphere = [&](const glm::vec3& center, float radius, ImU32 color, int segments = 32) {
            // Draw three perpendicular circles to create a sphere wireframe
            
            // Circle in XY plane
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                float angle2 = (float)((i + 1) % segments) / segments * 2.0f * glm::pi<float>();
                
                glm::vec3 p1 = center + glm::vec3(
                    radius * cos(angle1),
                    radius * sin(angle1),
                    0.0f
                );
                glm::vec3 p2 = center + glm::vec3(
                    radius * cos(angle2),
                    radius * sin(angle2),
                    0.0f
                );
                
                bool vis1, vis2;
                ImVec2 screen1 = ProjectToScreen(p1, vis1, vp, windowPos, windowSize);
                ImVec2 screen2 = ProjectToScreen(p2, vis2, vp, windowPos, windowSize);
                
                if (vis1 && vis2) {
                    drawList->AddLine(screen1, screen2, color, 2.0f);
                }
            }
            
            // Circle in XZ plane
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                float angle2 = (float)((i + 1) % segments) / segments * 2.0f * glm::pi<float>();
                
                glm::vec3 p1 = center + glm::vec3(
                    radius * cos(angle1),
                    0.0f,
                    radius * sin(angle1)
                );
                glm::vec3 p2 = center + glm::vec3(
                    radius * cos(angle2),
                    0.0f,
                    radius * sin(angle2)
                );
                
                bool vis1, vis2;
                ImVec2 screen1 = ProjectToScreen(p1, vis1, vp, windowPos, windowSize);
                ImVec2 screen2 = ProjectToScreen(p2, vis2, vp, windowPos, windowSize);
                
                if (vis1 && vis2) {
                    drawList->AddLine(screen1, screen2, color, 2.0f);
                }
            }
            
            // Circle in YZ plane
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * 2.0f * glm::pi<float>();
                float angle2 = (float)((i + 1) % segments) / segments * 2.0f * glm::pi<float>();
                
                glm::vec3 p1 = center + glm::vec3(
                    0.0f,
                    radius * cos(angle1),
                    radius * sin(angle1)
                );
                glm::vec3 p2 = center + glm::vec3(
                    0.0f,
                    radius * cos(angle2),
                    radius * sin(angle2)
                );
                
                bool vis1, vis2;
                ImVec2 screen1 = ProjectToScreen(p1, vis1, vp, windowPos, windowSize);
                ImVec2 screen2 = ProjectToScreen(p2, vis2, vp, windowPos, windowSize);
                
                if (vis1 && vis2) {
                    drawList->AddLine(screen1, screen2, color, 2.0f);
                }
            }
        };

        // Draw AudioComponent gizmos if present and spatialized
        if (hasAudioComponent) {
            AudioComponent& audioComp = ecsManager.GetComponent<AudioComponent>(selectedEntity);
            
            if (audioComp.Spatialize && audioComp.SpatialBlend > 0.0f) {
                // Colors: Min distance = green, Max distance = red
                ImU32 minDistanceColor = IM_COL32(0, 255, 0, 180);    // Green
                ImU32 maxDistanceColor = IM_COL32(255, 0, 0, 180);    // Red

                // Draw min distance sphere (green)
                drawWireSphere(audioPos, audioComp.MinDistance, minDistanceColor);

                // Draw max distance sphere (red)
                drawWireSphere(audioPos, audioComp.MaxDistance, maxDistanceColor);

                // Draw audio source icon at center
                bool iconVis;
                ImVec2 iconPos = ProjectToScreen(audioPos, iconVis, vp, windowPos, windowSize);
                if (iconVis) {
                    // Draw speaker icon (simplified)
                    // Left rectangle (speaker body)
                    drawList->AddRectFilled(
                        ImVec2(iconPos.x - 6, iconPos.y - 5),
                        ImVec2(iconPos.x, iconPos.y + 5),
                        IM_COL32(180, 180, 180, 220)
                    );
                    drawList->AddRect(
                        ImVec2(iconPos.x - 6, iconPos.y - 5),
                        ImVec2(iconPos.x, iconPos.y + 5),
                        IM_COL32(255, 255, 255, 255),
                        0.0f, 0, 1.5f
                    );

                    // Right triangle (speaker cone)
                    ImVec2 tri1(iconPos.x, iconPos.y - 5);
                    ImVec2 tri2(iconPos.x, iconPos.y + 5);
                    ImVec2 tri3(iconPos.x + 6, iconPos.y);
                    drawList->AddTriangleFilled(tri1, tri2, tri3, IM_COL32(180, 180, 180, 220));
                    drawList->AddTriangle(tri1, tri2, tri3, IM_COL32(255, 255, 255, 255), 1.5f);

                    // Sound waves
                    for (int i = 1; i <= 2; i++) {
                        float offsetX = iconPos.x + 8 + (i * 4);
                        drawList->AddLine(
                            ImVec2(offsetX, iconPos.y - 3 - i),
                            ImVec2(offsetX, iconPos.y + 3 + i),
                            IM_COL32(255, 255, 255, 200),
                            1.5f
                        );
                    }
                }
            }
        }

        // Draw AudioReverbZoneComponent gizmos if present
        if (hasReverbZone) {
            AudioReverbZoneComponent& reverbZone = ecsManager.GetComponent<AudioReverbZoneComponent>(selectedEntity);
            
            if (reverbZone.enabled) {
                // Colors: Min distance = cyan, Max distance = blue for reverb zones
                ImU32 minDistanceColor = IM_COL32(0, 255, 255, 150);  // Cyan
                ImU32 maxDistanceColor = IM_COL32(0, 100, 255, 150);  // Blue

                // Draw min distance sphere (cyan)
                drawWireSphere(audioPos, reverbZone.MinDistance, minDistanceColor);

                // Draw max distance sphere (blue)
                drawWireSphere(audioPos, reverbZone.MaxDistance, maxDistanceColor);

                // Draw reverb icon at center
                bool iconVis;
                ImVec2 iconPos = ProjectToScreen(audioPos, iconVis, vp, windowPos, windowSize);
                if (iconVis) {
                    // Draw reverb icon (circle with waves)
                    drawList->AddCircleFilled(iconPos, 7.0f, IM_COL32(100, 150, 255, 200));
                    drawList->AddCircle(iconPos, 7.0f, IM_COL32(0, 200, 255, 255), 0, 2.0f);
                    
                    // Reverb waves (concentric circles)
                    for (int i = 1; i <= 3; i++) {
                        drawList->AddCircle(iconPos, 7.0f + (i * 5.0f), IM_COL32(0, 200, 255, 150 - (i * 40)), 0, 1.5f);
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        ENGINE_PRINT("[ScenePanel] Error drawing audio gizmos: ", e.what(), "\n");
    }
}

void ScenePanel::DrawLightGizmos() {
    Entity selectedEntity = GUIManager::GetSelectedEntity();
    if (selectedEntity == static_cast<Entity>(-1)) return;

    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Check if entity has transform
        if (!ecsManager.HasComponent<Transform>(selectedEntity)) return;
        Transform& transform = ecsManager.GetComponent<Transform>(selectedEntity);

        // Check which light component types this entity has (early check to avoid camera access if not needed)
        bool hasDirectionalLight = ecsManager.HasComponent<DirectionalLightComponent>(selectedEntity);
        bool hasPointLight = ecsManager.HasComponent<PointLightComponent>(selectedEntity);
        bool hasSpotLight = ecsManager.HasComponent<SpotLightComponent>(selectedEntity);

        // Early exit if no light components
        if (!hasDirectionalLight && !hasPointLight && !hasSpotLight) return;

        // Get window and viewport info
        ImVec2 windowSize = ImGui::GetWindowSize();
        float editorAspectRatio = windowSize.x / windowSize.y;

        // Manually construct view matrix to avoid glm::lookAt assertion
        glm::vec3 camPos = editorCamera.Position;
        glm::vec3 camTarget = editorCamera.Target;
        glm::vec3 camUp = editorCamera.Up;

        // Validate camera vectors
        glm::vec3 forward = camTarget - camPos;
        if (glm::length(forward) < 0.001f) return; // Position == Target
        forward = glm::normalize(forward);

        glm::vec3 right = glm::cross(forward, camUp);
        if (glm::length(right) < 0.001f) return; // Up parallel to forward
        right = glm::normalize(right);

        glm::vec3 up = glm::cross(right, forward);

        // Construct view matrix manually (same as glm::lookAt but with validation)
        glm::mat4 view = glm::mat4(1.0f);
        view[0][0] = right.x;
        view[1][0] = right.y;
        view[2][0] = right.z;
        view[0][1] = up.x;
        view[1][1] = up.y;
        view[2][1] = up.z;
        view[0][2] = -forward.x;
        view[1][2] = -forward.y;
        view[2][2] = -forward.z;
        view[3][0] = -glm::dot(right, camPos);
        view[3][1] = -glm::dot(up, camPos);
        view[3][2] = glm::dot(forward, camPos);

        glm::mat4 projection = editorCamera.GetProjectionMatrix(editorAspectRatio);
        glm::mat4 vp = projection * view;

        ImVec2 windowPos = ImGui::GetWindowPos();

        // Get light position from world matrix
        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
        glm::vec3 lightPos = glm::vec3(worldMat[3]); // Extract translation

        // Get ImGui draw list
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Project 3D point to screen space
        auto projectToScreen = [&](const glm::vec3& worldPoint, bool& isVisible) -> ImVec2 {
            glm::vec4 clipSpace = vp * glm::vec4(worldPoint, 1.0f);
            if (clipSpace.w <= 0.0001f) {
                isVisible = false;
                return ImVec2(-10000, -10000);
            }
            glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
            float screenX = (ndc.x + 1.0f) * 0.5f * windowSize.x + windowPos.x;
            float screenY = (1.0f - ndc.y) * 0.5f * windowSize.y + windowPos.y;
            isVisible = true;
            return ImVec2(screenX, screenY);
        };

        // ==== DIRECTIONAL LIGHT ====
        if (ecsManager.HasComponent<DirectionalLightComponent>(selectedEntity)) {
            DirectionalLightComponent& light = ecsManager.GetComponent<DirectionalLightComponent>(selectedEntity);
            if (light.enabled) {
                // Use canonical forward direction (0, 0, -1) and apply transform rotation
                // This makes the transform rotation the sole controller of light direction
                glm::vec3 canonicalDirection(0.0f, 0.0f, -1.0f);
                glm::mat3 rotationMatrix = glm::mat3(worldMat);
                glm::vec3 direction = glm::normalize(rotationMatrix * canonicalDirection);

                glm::vec3 lightColor = light.color.ConvertToGLM();
                ImU32 color = IM_COL32(
                    (int)(lightColor.r * 255),
                    (int)(lightColor.g * 255),
                    (int)(lightColor.b * 255),
                    220
                );

                // Draw central sphere
                bool centerVis;
                ImVec2 centerScreen = projectToScreen(lightPos, centerVis);
                if (centerVis) {
                    drawList->AddCircleFilled(centerScreen, 6.0f, color);
                    drawList->AddCircle(centerScreen, 6.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
                }

                // Draw direction arrow
                glm::vec3 arrowEnd = lightPos + direction * 1.5f;
                bool arrowVis;
                ImVec2 arrowScreen = projectToScreen(arrowEnd, arrowVis);

                if (centerVis && arrowVis) {
                    // Arrow line
                    drawList->AddLine(centerScreen, arrowScreen, color, 3.0f);

                    // Arrow head
                    ImVec2 dir = ImVec2(arrowScreen.x - centerScreen.x, arrowScreen.y - centerScreen.y);
                    float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len > 0) {
                        dir.x /= len;
                        dir.y /= len;
                        ImVec2 perp = ImVec2(-dir.y, dir.x);

                        ImVec2 head1 = ImVec2(arrowScreen.x - dir.x * 12 + perp.x * 6, arrowScreen.y - dir.y * 12 + perp.y * 6);
                        ImVec2 head2 = ImVec2(arrowScreen.x - dir.x * 12 - perp.x * 6, arrowScreen.y - dir.y * 12 - perp.y * 6);

                        drawList->AddLine(arrowScreen, head1, color, 2.5f);
                        drawList->AddLine(arrowScreen, head2, color, 2.5f);
                    }
                }

                // Draw sun rays
                glm::vec3 perpendicular1 = glm::vec3(-direction.y, direction.x, 0.0f);
                if (glm::length(perpendicular1) < 0.01f) perpendicular1 = glm::vec3(1.0f, 0.0f, 0.0f);
                perpendicular1 = glm::normalize(perpendicular1);
                glm::vec3 perpendicular2 = glm::normalize(glm::cross(direction, perpendicular1));

                for (int i = 0; i < 8; i++) {
                    float angle = (i / 8.0f) * 2.0f * 3.14159265f;
                    glm::vec3 rayStart = lightPos + perpendicular1 * cos(angle) * 0.25f + perpendicular2 * sin(angle) * 0.25f;
                    glm::vec3 rayEnd = lightPos + perpendicular1 * cos(angle) * 0.5f + perpendicular2 * sin(angle) * 0.5f;

                    bool vis1, vis2;
                    ImVec2 screen1 = projectToScreen(rayStart, vis1);
                    ImVec2 screen2 = projectToScreen(rayEnd, vis2);

                    if (vis1 && vis2) {
                        drawList->AddLine(screen1, screen2, color, 2.0f);
                    }
                }
            }
        }

        // ==== POINT LIGHT ====
        if (ecsManager.HasComponent<PointLightComponent>(selectedEntity)) {
            PointLightComponent& light = ecsManager.GetComponent<PointLightComponent>(selectedEntity);
            if (light.enabled) {
                glm::vec3 lightColor = light.color.ConvertToGLM();
                ImU32 color = IM_COL32(
                    (int)(lightColor.r * 255),
                    (int)(lightColor.g * 255),
                    (int)(lightColor.b * 255),
                    180
                );

                // Calculate effective radius
                float effectiveRadius = 5.0f;
                if (light.quadratic > 0.0f) {
                    effectiveRadius = sqrt(1.0f / light.quadratic) * 2.0f;
                }

                // Draw central sphere
                bool centerVis;
                ImVec2 centerScreen = projectToScreen(lightPos, centerVis);
                if (centerVis) {
                    drawList->AddCircleFilled(centerScreen, 8.0f, color);
                    drawList->AddCircle(centerScreen, 8.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
                }

                // Draw range sphere (wireframe circles)
                int segments = 32;

                // XY circle
                for (int i = 0; i < segments; i++) {
                    float angle1 = (i / (float)segments) * 2.0f * 3.14159265f;
                    float angle2 = ((i + 1) / (float)segments) * 2.0f * 3.14159265f;

                    glm::vec3 p1 = lightPos + glm::vec3(effectiveRadius * cos(angle1), effectiveRadius * sin(angle1), 0.0f);
                    glm::vec3 p2 = lightPos + glm::vec3(effectiveRadius * cos(angle2), effectiveRadius * sin(angle2), 0.0f);

                    bool vis1, vis2;
                    ImVec2 screen1 = projectToScreen(p1, vis1);
                    ImVec2 screen2 = projectToScreen(p2, vis2);

                    if (vis1 && vis2) {
                        drawList->AddLine(screen1, screen2, color, 1.5f);
                    }
                }

                // XZ circle
                for (int i = 0; i < segments; i++) {
                    float angle1 = (i / (float)segments) * 2.0f * 3.14159265f;
                    float angle2 = ((i + 1) / (float)segments) * 2.0f * 3.14159265f;

                    glm::vec3 p1 = lightPos + glm::vec3(effectiveRadius * cos(angle1), 0.0f, effectiveRadius * sin(angle1));
                    glm::vec3 p2 = lightPos + glm::vec3(effectiveRadius * cos(angle2), 0.0f, effectiveRadius * sin(angle2));

                    bool vis1, vis2;
                    ImVec2 screen1 = projectToScreen(p1, vis1);
                    ImVec2 screen2 = projectToScreen(p2, vis2);

                    if (vis1 && vis2) {
                        drawList->AddLine(screen1, screen2, color, 1.5f);
                    }
                }

                // YZ circle
                for (int i = 0; i < segments; i++) {
                    float angle1 = (i / (float)segments) * 2.0f * 3.14159265f;
                    float angle2 = ((i + 1) / (float)segments) * 2.0f * 3.14159265f;

                    glm::vec3 p1 = lightPos + glm::vec3(0.0f, effectiveRadius * cos(angle1), effectiveRadius * sin(angle1));
                    glm::vec3 p2 = lightPos + glm::vec3(0.0f, effectiveRadius * cos(angle2), effectiveRadius * sin(angle2));

                    bool vis1, vis2;
                    ImVec2 screen1 = projectToScreen(p1, vis1);
                    ImVec2 screen2 = projectToScreen(p2, vis2);

                    if (vis1 && vis2) {
                        drawList->AddLine(screen1, screen2, color, 1.5f);
                    }
                }
            }
        }

        // ==== SPOT LIGHT ====
        if (ecsManager.HasComponent<SpotLightComponent>(selectedEntity)) {
            SpotLightComponent& light = ecsManager.GetComponent<SpotLightComponent>(selectedEntity);
            if (light.enabled) {
                // Use canonical forward direction (0, 0, -1) and apply transform rotation
                // This makes the transform rotation the sole controller of light direction
                glm::vec3 canonicalDirection(0.0f, 0.0f, -1.0f);
                glm::mat3 rotationMatrix = glm::mat3(worldMat);
                glm::vec3 direction = glm::normalize(rotationMatrix * canonicalDirection);

                glm::vec3 lightColor = light.color.ConvertToGLM();
                ImU32 color = IM_COL32(
                    (int)(lightColor.r * 255),
                    (int)(lightColor.g * 255),
                    (int)(lightColor.b * 255),
                    180
                );

                // Calculate range and cone angle
                float range = 10.0f;
                if (light.quadratic > 0.0f) {
                    range = sqrt(1.0f / light.quadratic) * 1.5f;
                }
                float coneAngle = acos(light.cutOff);
                float coneRadius = range * tan(coneAngle);

                // Draw central sphere
                bool centerVis;
                ImVec2 centerScreen = projectToScreen(lightPos, centerVis);
                if (centerVis) {
                    drawList->AddCircleFilled(centerScreen, 6.0f, color);
                    drawList->AddCircle(centerScreen, 6.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
                }

                // Calculate perpendicular vectors for cone
                glm::vec3 perpendicular1 = glm::vec3(-direction.y, direction.x, 0.0f);
                if (glm::length(perpendicular1) < 0.01f) perpendicular1 = glm::vec3(1.0f, 0.0f, 0.0f);
                perpendicular1 = glm::normalize(perpendicular1);
                glm::vec3 perpendicular2 = glm::normalize(glm::cross(direction, perpendicular1));

                glm::vec3 coneEnd = lightPos + direction * range;

                // Draw cone lines from apex to circle
                int numLines = 8;
                for (int i = 0; i < numLines; i++) {
                    float angle = (i / (float)numLines) * 2.0f * 3.14159265f;
                    glm::vec3 pointOnCircle = coneEnd +
                        perpendicular1 * cos(angle) * coneRadius +
                        perpendicular2 * sin(angle) * coneRadius;

                    bool vis1, vis2;
                    ImVec2 screen1 = projectToScreen(lightPos, vis1);
                    ImVec2 screen2 = projectToScreen(pointOnCircle, vis2);

                    if (vis1 && vis2) {
                        drawList->AddLine(screen1, screen2, color, 2.0f);
                    }
                }

                // Draw circle at cone end
                for (int i = 0; i < numLines; i++) {
                    float angle1 = (i / (float)numLines) * 2.0f * 3.14159265f;
                    float angle2 = ((i + 1) / (float)numLines) * 2.0f * 3.14159265f;

                    glm::vec3 point1 = coneEnd + perpendicular1 * cos(angle1) * coneRadius + perpendicular2 * sin(angle1) * coneRadius;
                    glm::vec3 point2 = coneEnd + perpendicular1 * cos(angle2) * coneRadius + perpendicular2 * sin(angle2) * coneRadius;

                    bool vis1, vis2;
                    ImVec2 screen1 = projectToScreen(point1, vis1);
                    ImVec2 screen2 = projectToScreen(point2, vis2);

                    if (vis1 && vis2) {
                        drawList->AddLine(screen1, screen2, color, 1.5f);
                    }
                }

                // Draw center direction line
                bool endVis;
                ImVec2 endScreen = projectToScreen(coneEnd, endVis);
                if (centerVis && endVis) {
                    drawList->AddLine(centerScreen, endScreen, color, 2.5f);
                }
            }
        }

    } catch (const std::exception& e) {
        ENGINE_PRINT("[ScenePanel] Error drawing light gizmos: ", e.what(), "\n");
    }
}

void ScenePanel::DrawSelectionOutline(Entity entity, int sceneWidth, int sceneHeight) {
    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        // Compute view and projection matrices
        float aspectRatio = static_cast<float>(sceneWidth) / sceneHeight;
        glm::mat4 view = editorCamera.GetViewMatrix();
        glm::mat4 projection = editorCamera.GetProjectionMatrix(aspectRatio);

        glm::vec3 localCorners[8];
        bool hasValidBounds = false;

        Matrix4x4 worldMatrix;
        bool hasTransform = ecsManager.HasComponent<Transform>(entity);
        if (hasTransform) {
            auto& transform = ecsManager.GetComponent<Transform>(entity);
            worldMatrix = transform.worldMatrix;
        } else {
            // Identity matrix if no transform
            worldMatrix = Matrix4x4::Identity();
        }

        // Define local corners based on component type
        if (ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
            auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);
            // For sprites, use unit quad (-0.5 to 0.5) in local space
            // The Transform's worldMatrix will apply the scale (extracted from worldMatrix in SpriteSystem)
            // This prevents double-scaling: sprite.scale is already extracted from worldMatrix,
            // so we shouldn't multiply by it again before applying worldMatrix transform
            localCorners[0] = glm::vec3(-0.5f, -0.5f, sprite.is3D ? -0.5f : 0.0f);
            localCorners[1] = glm::vec3( 0.5f, -0.5f, sprite.is3D ? -0.5f : 0.0f);
            localCorners[2] = glm::vec3(-0.5f,  0.5f, sprite.is3D ? -0.5f : 0.0f);
            localCorners[3] = glm::vec3( 0.5f,  0.5f, sprite.is3D ? -0.5f : 0.0f);
            localCorners[4] = glm::vec3(-0.5f, -0.5f, sprite.is3D ?  0.5f : 0.0f);
            localCorners[5] = glm::vec3( 0.5f, -0.5f, sprite.is3D ?  0.5f : 0.0f);
            localCorners[6] = glm::vec3(-0.5f,  0.5f, sprite.is3D ?  0.5f : 0.0f);
            localCorners[7] = glm::vec3( 0.5f,  0.5f, sprite.is3D ?  0.5f : 0.0f);
            hasValidBounds = true;
        } else if (ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
            if (modelComp.model) {
                auto modelAABB = modelComp.model->GetBoundingBox();
                localCorners[0] = glm::vec3(modelAABB.min.x, modelAABB.min.y, modelAABB.min.z);
                localCorners[1] = glm::vec3(modelAABB.max.x, modelAABB.min.y, modelAABB.min.z);
                localCorners[2] = glm::vec3(modelAABB.min.x, modelAABB.max.y, modelAABB.min.z);
                localCorners[3] = glm::vec3(modelAABB.max.x, modelAABB.max.y, modelAABB.min.z);
                localCorners[4] = glm::vec3(modelAABB.min.x, modelAABB.min.y, modelAABB.max.z);
                localCorners[5] = glm::vec3(modelAABB.max.x, modelAABB.min.y, modelAABB.max.z);
                localCorners[6] = glm::vec3(modelAABB.min.x, modelAABB.max.y, modelAABB.max.z);
                localCorners[7] = glm::vec3(modelAABB.max.x, modelAABB.max.y, modelAABB.max.z);
                hasValidBounds = true;
            }
        }

        // Default small box if no specific component
        if (!hasValidBounds) {
            localCorners[0] = glm::vec3(-0.5f, -0.5f, -0.5f);
            localCorners[1] = glm::vec3( 0.5f, -0.5f, -0.5f);
            localCorners[2] = glm::vec3(-0.5f,  0.5f, -0.5f);
            localCorners[3] = glm::vec3( 0.5f,  0.5f, -0.5f);
            localCorners[4] = glm::vec3(-0.5f, -0.5f,  0.5f);
            localCorners[5] = glm::vec3( 0.5f, -0.5f,  0.5f);
            localCorners[6] = glm::vec3(-0.5f,  0.5f,  0.5f);
            localCorners[7] = glm::vec3( 0.5f,  0.5f,  0.5f);
            hasValidBounds = true;
        }

        // Transform local corners to world space
        glm::vec3 worldCorners[8];
        for (int i = 0; i < 8; ++i) {
            Vector3D localVec(localCorners[i].x, localCorners[i].y, localCorners[i].z);
            Vector3D worldVec = worldMatrix.TransformPoint(localVec);
            worldCorners[i] = glm::vec3(worldVec.x, worldVec.y, worldVec.z);
        }

        // Get window position and size for screen coordinate calculation
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Project to screen space
        ImVec2 screenPoints[8];
        bool allVisible = true;
        glm::mat4 vp = projection * view;
        for (int i = 0; i < 8; ++i) {
            bool isVisible;
            screenPoints[i] = ProjectToScreen(worldCorners[i], isVisible, vp, windowPos, windowSize);
            if (!isVisible) {
                allVisible = false;
                break;
            }
        }

        if (!allVisible) {
            return; // Don't draw if any point is behind camera
        }

        // Get ImGui draw list
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw the outline with a bright color (similar to Unity's selection)
        ImU32 outlineColor = IM_COL32(255, 165, 0, 255); // Orange outline
        float thickness = 2.0f;

        // Draw the 12 edges of the box
        // Bottom face
        drawList->AddLine(screenPoints[0], screenPoints[1], outlineColor, thickness);
        drawList->AddLine(screenPoints[1], screenPoints[3], outlineColor, thickness);
        drawList->AddLine(screenPoints[3], screenPoints[2], outlineColor, thickness);
        drawList->AddLine(screenPoints[2], screenPoints[0], outlineColor, thickness);

        // Top face
        drawList->AddLine(screenPoints[4], screenPoints[5], outlineColor, thickness);
        drawList->AddLine(screenPoints[5], screenPoints[7], outlineColor, thickness);
        drawList->AddLine(screenPoints[7], screenPoints[6], outlineColor, thickness);
        drawList->AddLine(screenPoints[6], screenPoints[4], outlineColor, thickness);

        // Vertical edges
        drawList->AddLine(screenPoints[0], screenPoints[4], outlineColor, thickness);
        drawList->AddLine(screenPoints[1], screenPoints[5], outlineColor, thickness);
        drawList->AddLine(screenPoints[2], screenPoints[6], outlineColor, thickness);
        drawList->AddLine(screenPoints[3], screenPoints[7], outlineColor, thickness);

    } catch (const std::exception& e) {
        ENGINE_PRINT("[ScenePanel] Error drawing selection outline: ", e.what(), "\n");
    }
}