#include "Panels/GamePanel.hpp"
#include "imgui.h"
#include "Graphics/SceneRenderer.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "EditorState.hpp"
#include "Engine.h"
#include "RunTimeVar.hpp"
#include "EditorComponents.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "WindowManager.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

GamePanel::GamePanel()
    : EditorPanel("Game", true), selectedResolutionIndex(0), useCustomAspectRatio(false),
      customAspectRatio(16.0f / 9.0f), freeAspect(false), viewportScale(1.0f) {

    // Initialize common resolutions
    resolutions.emplace_back(1920, 1080, "Full HD (1920x1080)");
    resolutions.emplace_back(1280, 720, "HD (1280x720)");
    resolutions.emplace_back(1600, 900, "HD+ (1600x900)");

    // Android device resolutions (portrait)
    resolutions.emplace_back(1080, 2400, "Galaxy S21 (1080x2400)");
    resolutions.emplace_back(1440, 3200, "Galaxy S22 Ultra (1440x3200)");
    resolutions.emplace_back(1080, 2340, "Pixel 7 (1080x2340)");

    // iPhone device resolutions (portrait)
    resolutions.emplace_back(1179, 2556, "iPhone 14 Pro (1179x2556)");
    resolutions.emplace_back(1284, 2778, "iPhone 14 Pro Max (1284x2778)");
}

void GamePanel::OnImGuiRender() {
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_VIEWPORT);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_VIEWPORT);

    if (ImGui::Begin(name.c_str(), &isOpen)) {

        // Render the resolution panel toolbar (always at top)
        RenderResolutionPanel();

        // Get available space after toolbar for game view
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        int availableWidth = (int)availableSize.x;
        int availableHeight = (int)availableSize.y;

        // Ensure minimum size
        if (availableWidth < 100) availableWidth = 100;
        if (availableHeight < 100) availableHeight = 100;

        // Wrap game view in child window to prevent overlap with toolbar
        ImGui::BeginChild("GameViewport", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Recalculate available size inside the child window
        availableSize = ImGui::GetContentRegionAvail();
        availableWidth = (int)availableSize.x;
        availableHeight = (int)availableSize.y;
        if (availableWidth < 100) availableWidth = 100;
        if (availableHeight < 100) availableHeight = 100;

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

        // Calculate display viewport dimensions with aspect ratio preservation
        int displayWidth, displayHeight;
        float offsetX, offsetY;
        CalculateViewportDimensions(availableWidth, availableHeight,
                                  displayWidth, displayHeight, offsetX, offsetY);
        RunTimeVar::window.gameViewportWidth = displayWidth;
        RunTimeVar::window.gameViewportHeight = displayHeight;

        // Apply scale factor
        displayWidth = (int)((float)displayWidth * viewportScale);
        displayHeight = (int)((float)displayHeight * viewportScale);

        // Use display dimensions for rendering (actual panel size)
        int renderWidth = displayWidth;
        int renderHeight = displayHeight;

        // Recalculate offsets for centering after scaling
        offsetX = (availableWidth - displayWidth) * 0.5f;
        offsetY = (availableHeight - displayHeight) * 0.5f;

        // Set cursor position to center the viewport
        ImVec2 startPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(startPos.x + offsetX, startPos.y + offsetY));

        // Only render if we should (optimization for unfocused panels)
        if (shouldRender) {
            auto& gfx = GraphicsManager::GetInstance();

            // Set viewport to actual render dimensions
            gfx.SetViewportSize(renderWidth, renderHeight);

            SceneRenderer::BeginGameRender(renderWidth, renderHeight);

            // Update frustum with Game Panel's viewport BEFORE rendering
            gfx.UpdateFrustum();

            if (Engine::ShouldRunGameLogic() || Engine::IsPaused()) {
                // Render 3D scene with game logic running
                SceneRenderer::RenderScene(); // This will run with game logic
            } else {
                // Render scene preview with game camera (no game logic)
                SceneRenderer::RenderScene(); // This should show the game camera view
            }

            SceneRenderer::EndGameRender();
        }

        // Check if there are any active cameras in the scene
        bool hasActiveCamera = false;
        try {
            ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
            Entity activeCameraEntity = ecsManager.cameraSystem ? ecsManager.cameraSystem->GetActiveCameraEntity() : UINT32_MAX;

            if (activeCameraEntity != UINT32_MAX) {
                // Also check if the camera's entity is active (not just the camera component)
                if (ecsManager.HasComponent<ActiveComponent>(activeCameraEntity)) {
                    auto& activeComp = ecsManager.GetComponent<ActiveComponent>(activeCameraEntity);
                    hasActiveCamera = activeComp.isActive;
                } else {
                    // No ActiveComponent means entity is active by default
                    hasActiveCamera = true;
                }
            }
        } catch (...) {
            hasActiveCamera = false;
        }

        // Get the texture from Game framebuffer (not Scene framebuffer)
        unsigned int sceneTexture = SceneRenderer::GetGameTexture();
        if (sceneTexture != 0) {
            // Render texture matches display size exactly - no cropping needed
            ImVec2 uv0 = ImVec2(0, 1);  // Bottom-left
            ImVec2 uv1 = ImVec2(1, 0);  // Top-right

            ImGui::Image(
                (void*)(intptr_t)sceneTexture,
                ImVec2((float)displayWidth, (float)displayHeight),
                uv0, uv1
            );

            // Auto-focus on interaction (any mouse click)
            if (ImGui::IsItemHovered() && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                                           ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                                           ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
                ImGui::SetWindowFocus();

                // Capture cursor when clicking in game panel during play mode
                // Only if game code has requested cursor lock (respects main menu wanting cursor free)
                if (Engine::ShouldRunGameLogic() && !cursorCaptured && WindowManager::IsCursorLockRequested()) {
                    SetCursorCaptured(true);
                }
            }

            // Release cursor on Escape key during play mode
            if (cursorCaptured && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                SetCursorCaptured(false);
            }

            // Release cursor when game stops
            if (cursorCaptured && !Engine::ShouldRunGameLogic()) {
                SetCursorCaptured(false);
            }

            // Release cursor if game code explicitly unlocked it
            if (cursorCaptured && !WindowManager::IsCursorLockRequested()) {
                SetCursorCaptured(false);
            }

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetItemRectMin();
            ImVec2 pos_max = ImGui::GetItemRectMax();
            draw_list->AddRect(pos, pos_max, IM_COL32(40, 40, 40, 255));

            // Overlay "No cameras" message if no active camera
            if (!hasActiveCamera) {
                ImVec2 center = ImVec2((pos.x + pos_max.x) * 0.5f, (pos.y + pos_max.y) * 0.5f);

                // Draw pure black overlay (fully opaque)
                draw_list->AddRectFilled(pos, pos_max, IM_COL32(0, 0, 0, 255));

                // Draw warning message
                const char* msg1 = "No cameras rendering";
                const char* msg2 = "Add a Camera component to an entity";

                ImVec2 textSize1 = ImGui::CalcTextSize(msg1);
                ImVec2 textSize2 = ImGui::CalcTextSize(msg2);

                ImVec2 textPos1 = ImVec2(center.x - textSize1.x * 0.5f, center.y - 20.0f);
                ImVec2 textPos2 = ImVec2(center.x - textSize2.x * 0.5f, center.y + 5.0f);

                draw_list->AddText(textPos1, IM_COL32(255, 255, 255, 255), msg1);
                draw_list->AddText(textPos2, IM_COL32(180, 180, 180, 255), msg2);
            }

            // Update the engine's InputManager mouse position relative to the game panel.
            // Panel bounds
            ImVec2 panelMin = ImGui::GetItemRectMin();
            ImVec2 panelMax = ImGui::GetItemRectMax();

            // When cursor is captured, use GLFW raw mouse position for unlimited movement
            // Otherwise use ImGui position with bounds check
            if (cursorCaptured) {
                // Get raw GLFW mouse position - this gives unlimited virtual movement
                double glfwMouseX, glfwMouseY;
                GLFWwindow* window = static_cast<GLFWwindow*>(WindowManager::getWindow());
                glfwGetCursorPos(window, &glfwMouseX, &glfwMouseY);

                // Feed raw position directly to InputManager for camera rotation
                // The camera script uses mouse delta, so absolute position works
                InputManager::SetGamePanelMousePos((float)glfwMouseX, (float)glfwMouseY);
            } else {
                // Normal mode - only update when inside panel
                ImVec2 mousePos = ImGui::GetMousePos();
                float relX = mousePos.x - panelMin.x;
                float relY = mousePos.y - panelMin.y;

                bool insidePanel = (mousePos.x >= panelMin.x && mousePos.x <= panelMax.x &&
                    mousePos.y >= panelMin.y && mousePos.y <= panelMax.y);

                if (insidePanel) {
                    // Scale relative coords to framebuffer resolution
                    float scaleX = (float)renderWidth / (panelMax.x - panelMin.x);
                    float scaleY = (float)renderHeight / (panelMax.y - panelMin.y);

                    float gameX = relX * scaleX;
                    float gameY = relY * scaleY;

                    // Feed into InputManager (game-space coordinates)
                    InputManager::SetGamePanelMousePos(gameX, gameY);
                }
            }

        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Game View - Framebuffer not ready");
            ImGui::Text("Size: %dx%d", displayWidth, displayHeight);
        }

        ImGui::EndChild(); // End GameViewport child window
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
}

void GamePanel::RenderResolutionPanel() {
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.15f, 1.0f)); // Subtle border
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4)); // Padding inside toolbar

    // Taller toolbar to fit content properly
    if (ImGui::BeginChild("ResolutionToolbar", ImVec2(0, 32), true, ImGuiWindowFlags_NoScrollbar)) {

        // Resolution dropdown (no label, just combo)
        ImGui::SetNextItemWidth(180);

        std::string previewText;
        if (freeAspect) {
            previewText = "Free Aspect";
        } else if (useCustomAspectRatio) {
            previewText = "Custom Aspect (" + std::to_string(customAspectRatio) + ":1)";
        } else {
            previewText = resolutions[selectedResolutionIndex].name;
        }

        RunTimeVar::window.gameResolutionWidth = resolutions[selectedResolutionIndex].width;
        RunTimeVar::window.gameResolutionHeight = resolutions[selectedResolutionIndex].height;

        if (ImGui::BeginCombo("##Resolution", previewText.c_str())) {
            // Free aspect option
            if (ImGui::Selectable("Free Aspect", freeAspect)) {
                freeAspect = true;
                useCustomAspectRatio = false;
            }

            // Preset resolutions
            for (size_t i = 0; i < resolutions.size(); i++) {
                bool isSelected = !freeAspect && !useCustomAspectRatio && (i == selectedResolutionIndex);
                if (ImGui::Selectable(resolutions[i].name.c_str(), isSelected)) {
                    selectedResolutionIndex = (int)i;
                    freeAspect = false;
                    useCustomAspectRatio = false;
                }
            }

            // Custom aspect ratio option
            if (ImGui::Selectable("Custom Aspect", useCustomAspectRatio)) {
                useCustomAspectRatio = true;
                freeAspect = false;
            }

            ImGui::EndCombo();
        }

        // Custom aspect ratio input
        if (useCustomAspectRatio) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::DragFloat("##AspectRatio", &customAspectRatio, 0.01f, 0.1f, 10.0f, "%.2f:1");
        }

        // Display current resolution info
        if (!freeAspect && !useCustomAspectRatio) {
            const auto& res = resolutions[selectedResolutionIndex];
            ImGui::SameLine();
            ImGui::TextDisabled("%dx%d", res.width, res.height);
        }

        
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        EditorComponents::DrawScaleSlider("Scale", &viewportScale, 0.1f, 2.0f, 80.0f);
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2); // Pop ItemSpacing and WindowPadding
    ImGui::PopStyleColor(2); // Pop ChildBg and Border
}

void GamePanel::CalculateViewportDimensions(int availableWidth, int availableHeight,
                                          int& viewportWidth, int& viewportHeight,
                                          float& offsetX, float& offsetY) {
    if (freeAspect) {
        // Use full available space
        viewportWidth = availableWidth;
        viewportHeight = availableHeight;
        offsetX = 0.0f;
        offsetY = 0.0f;
        return;
    }

    // Calculate target aspect ratio
    float targetAspectRatio;
    if (useCustomAspectRatio) {
        targetAspectRatio = customAspectRatio;
    } else {
        const auto& res = resolutions[selectedResolutionIndex];
        targetAspectRatio = (float)res.width / (float)res.height;
    }

    // Calculate viewport dimensions maintaining aspect ratio
    float availableAspectRatio = (float)availableWidth / (float)availableHeight;

    if (availableAspectRatio > targetAspectRatio) {
        // Available area is wider than target - letterbox horizontally
        viewportHeight = availableHeight;
        viewportWidth = (int)(availableHeight * targetAspectRatio);
        offsetX = (availableWidth - viewportWidth) * 0.5f;
        offsetY = 0.0f;
    } else {
        // Available area is taller than target - letterbox vertically
        viewportWidth = availableWidth;
        viewportHeight = (int)(availableWidth / targetAspectRatio);
        offsetX = 0.0f;
        offsetY = (availableHeight - viewportHeight) * 0.5f;
    }

    // Ensure minimum dimensions
    if (viewportWidth < 100) viewportWidth = 100;
    if (viewportHeight < 100) viewportHeight = 100;
}

void GamePanel::GetTargetGameResolution(int& outWidth, int& outHeight) const {
    if (freeAspect) {
        // For free aspect, return the window dimensions
        outWidth = RunTimeVar::window.width;
        outHeight = RunTimeVar::window.height;
    } else if (useCustomAspectRatio) {
        // For custom aspect, calculate based on current window and aspect ratio
        float currentAspect = (float)RunTimeVar::window.width / (float)RunTimeVar::window.height;
        if (currentAspect > customAspectRatio) {
            outHeight = RunTimeVar::window.height;
            outWidth = (int)(outHeight * customAspectRatio);
        } else {
            outWidth = RunTimeVar::window.width;
            outHeight = (int)(outWidth / customAspectRatio);
        }
    } else {
        // Return the selected resolution
        const auto& res = resolutions[selectedResolutionIndex];
        outWidth = res.width;
        outHeight = res.height;
    }
}

void GamePanel::SetCursorCaptured(bool captured) {
    if (cursorCaptured == captured) return;

    cursorCaptured = captured;
    GLFWwindow* window = static_cast<GLFWwindow*>(WindowManager::getWindow());
    if (window) {
        if (captured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}