#include "Panels/GamePanel.hpp"
#include "imgui.h"
#include "Graphics/SceneRenderer.hpp"
#include "EditorState.hpp"
#include "Engine.h"

GamePanel::GamePanel()
    : EditorPanel("Game", true) {
}

void GamePanel::OnImGuiRender() {
    if (ImGui::Begin(m_Name.c_str(), &m_IsOpen)) {

        // Get the content region size
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        int gameViewWidth = (int)viewportPanelSize.x;
        int gameViewHeight = (int)viewportPanelSize.y;

        // Ensure minimum size
        if (gameViewWidth < 100) gameViewWidth = 100;
        if (gameViewHeight < 100) gameViewHeight = 100;

        EditorState& editorState = EditorState::GetInstance();

        // Render the game when in play mode or paused (show frozen game scene)
        if (Engine::ShouldRunGameLogic() || Engine::IsPaused()) {
            // Render 3D scene with game logic running
            SceneRenderer::BeginSceneRender(gameViewWidth, gameViewHeight);
            SceneRenderer::RenderScene(); // This will run with game logic
            SceneRenderer::EndSceneRender();

            // Get the texture from SceneRenderer and display it
            unsigned int sceneTexture = SceneRenderer::GetSceneTexture();
            if (sceneTexture != 0) {
                ImGui::Image(
                    (void*)(intptr_t)sceneTexture,
                    ImVec2((float)gameViewWidth, (float)gameViewHeight),
                    ImVec2(0, 1), ImVec2(1, 0)  // Flip Y coordinate for OpenGL
                );
            }
            else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Game View - Framebuffer not ready");
                ImGui::Text("Size: %dx%d", gameViewWidth, gameViewHeight);
            }
        } else {
            // Show a placeholder when not playing
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            ImVec2 textSize = ImGui::CalcTextSize("Press Play to start the game");
            ImVec2 textPos = ImVec2(
                (windowSize.x - textSize.x) * 0.5f,
                (windowSize.y - textSize.y) * 0.5f
            );
            
            ImGui::SetCursorPos(textPos);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press Play to start the game");
            
            // Show current state
            ImGui::SetCursorPos(ImVec2(textPos.x - 20.0f, textPos.y + 30.0f));
            const char* stateText = Engine::IsPaused() ? "Game Paused" : "Edit Mode";
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), stateText);
        }
    }
    ImGui::End();
}