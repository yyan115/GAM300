// ButtonSystem.cpp
#include "pch.h"
#include "UI/Button/ButtonSystem.hpp"
#include "UI/Button/ButtonComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "Logging.hpp"
#include "TimeManager.hpp"
#include "Graphics/GraphicsManager.hpp"


void ButtonSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;
}

void ButtonSystem::Update() {
    // This only runs during play mode in editor
    // Update any button-related state here
    UpdateButtonStates();
}

void ButtonSystem::UpdateButtonStates() {
    if (!m_ecs) return;

    if (InputManager::GetMouseButtonDown(Input::MouseButton::LEFT)) {
        // Get viewport dimensions (actual render area)
        float viewportWidth = static_cast<float>(WindowManager::GetViewportWidth());
        float viewportHeight = static_cast<float>(WindowManager::GetViewportHeight());

        // Get target game resolution (world coordinate space for 2D UI)
        int gameResWidth, gameResHeight;
        GraphicsManager::GetInstance().GetTargetGameResolution(gameResWidth, gameResHeight);

        // Get raw mouse position in viewport coordinates
        float mouseX = static_cast<float>(InputManager::GetMouseX());
        float mouseY = static_cast<float>(InputManager::GetMouseY());

        // Map mouse coordinates from viewport space to game resolution space
        // X: scale from viewport width to game resolution width
        // Y: scale from viewport height to game resolution height, and flip (OpenGL has Y=0 at bottom)
        float gameX = (mouseX / viewportWidth) * static_cast<float>(gameResWidth);
        float gameY = static_cast<float>(gameResHeight) - (mouseY / viewportHeight) * static_cast<float>(gameResHeight);

        Vector3D mousePosInGameSpace(gameX, gameY, 0.0f);

        ENGINE_LOG_INFO("Viewport: " + std::to_string(viewportWidth) + "x" + std::to_string(viewportHeight) +
                       " | Game Res: " + std::to_string(gameResWidth) + "x" + std::to_string(gameResHeight) +
                       " | Mouse viewport: (" + std::to_string(mouseX) + ", " + std::to_string(mouseY) + ")" +
                       " | Mouse game: (" + std::to_string(gameX) + ", " + std::to_string(gameY) + ")");

        for (Entity e : m_ecs->GetActiveEntities()) {
            if (!m_ecs->HasComponent<ButtonComponent>(e)) continue;
            HandleMouseClick(e, mousePosInGameSpace);
        }
    }
}

void ButtonSystem::Shutdown() {
    m_ecs = nullptr;

    ENGINE_PRINT("[ButtonSystem] Shutdown complete");
}

/****************************************************************
The following functions can be subsumed into each other.
***************************************************************/

void ButtonSystem::HandleMouseClick(Entity buttonEntity, Vector3D mousePos)
{
    if (m_ecs->HasComponent<SpriteRenderComponent>(buttonEntity)) {
        auto& spriteComponent = m_ecs->GetComponent<SpriteRenderComponent>(buttonEntity);
        if (spriteComponent.is3D) {
            ENGINE_LOG_WARN("[ButtonSystem] 3D buttons not supported yet.");
            return;
        }

        auto& transform = m_ecs->GetComponent<Transform>(buttonEntity);
        float halfExtentsX = transform.localScale.x / 2.0f;
        float halfExtentsY = transform.localScale.y / 2.0f;
        float minX = transform.localPosition.x - halfExtentsX;
        float maxX = transform.localPosition.x + halfExtentsX;
        float minY = transform.localPosition.y - halfExtentsY;
        float maxY = transform.localPosition.y + halfExtentsY;

        // Check if mouse is within button bounds
        if (mousePos.x >= minX && mousePos.x <= maxX && mousePos.y >= minY && mousePos.y <= maxY) {
            ENGINE_LOG_INFO("[ButtonSystem] Button hit! Entity: " + std::to_string(buttonEntity) +
                          " Bounds: (" + std::to_string(minX) + "-" + std::to_string(maxX) + ", " +
                          std::to_string(minY) + "-" + std::to_string(maxY) + ")");
            TriggerButton(buttonEntity);
        }
    }
}

void ButtonSystem::TriggerButton(Entity buttonEntity) {
    if (!m_ecs) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonSystem] Cannot trigger button: ECSManager not available");
        return;
    }

    if (!m_ecs->HasComponent<ButtonComponent>(buttonEntity)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonSystem] Entity ", buttonEntity, " has no ButtonComponent");
        return;
    }

    const auto& buttonData = m_ecs->GetComponent<ButtonComponent>(buttonEntity);

    if (!buttonData.interactable) {
        ENGINE_PRINT(EngineLogging::LogLevel::Debug,
            "[ButtonSystem] Button ", buttonEntity, " is not interactable");
        return;
    }

    ProcessButtonClick(buttonEntity);
}

void ButtonSystem::ProcessButtonClick(Entity buttonEntity) {
    // The actual work is delegated to ButtonController
    // Create a temporary ButtonController to handle the click
    ButtonController tempButton(buttonEntity);
    tempButton.OnClick();

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[ButtonSystem] Processed click for button ", buttonEntity);
}

//if (m_ecs->HasComponent<ScriptComponentData>(e)) {
//    auto& scriptComp = m_ecs->GetComponent<ScriptComponentData>(e);

//    for (const auto& script : scriptComp.scripts) {
//        std::cout << "Script GUID: " << script.scriptGuidStr << std::endl;
//        std::cout << "Script Path: " << script.scriptPath << std::endl;
//    }
//}