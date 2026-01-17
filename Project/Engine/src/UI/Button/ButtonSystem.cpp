// ButtonSystem.cpp
#include "pch.h"
#include "UI/Button/ButtonSystem.hpp"
#include "UI/Button/ButtonComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "Logging.hpp"
#include "TimeManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "WindowManager.hpp"
#include "Input/InputManager.h"


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

    // Use unified input system (works on both desktop and Android)
    if (!g_inputManager) {
        ENGINE_LOG_WARN("[ButtonSystem] g_inputManager is null! Cannot process button input.");
        return;
    }

    if (g_inputManager->IsPointerJustPressed()) {
        // Get viewport dimensions (actual render area)
        float viewportWidth = static_cast<float>(WindowManager::GetViewportWidth());
        float viewportHeight = static_cast<float>(WindowManager::GetViewportHeight());

        // Get target game resolution (world coordinate space for 2D UI)
        int gameResWidth, gameResHeight;
        GraphicsManager::GetInstance().GetTargetGameResolution(gameResWidth, gameResHeight);

        // Get pointer position (mouse on desktop, touch on Android)
        glm::vec2 pointerPosNormalized = g_inputManager->GetPointerPosition();

        // Convert normalized coords to viewport pixels (assuming normalized is in pixels already)
        // TODO: Verify if GetPointerPosition returns pixels or normalized 0-1 coords
        float pointerX = pointerPosNormalized.x;
        float pointerY = pointerPosNormalized.y;

        // Map pointer coordinates from viewport space to game resolution space
        float gameX = (pointerX / viewportWidth) * static_cast<float>(gameResWidth);
        float gameY = static_cast<float>(gameResHeight) - (pointerY / viewportHeight) * static_cast<float>(gameResHeight);

        Vector3D pointerPosInGameSpace(gameX, gameY, 0.0f);

        ENGINE_LOG_INFO("Viewport: " + std::to_string(viewportWidth) + "x" + std::to_string(viewportHeight) +
                       " | Game Res: " + std::to_string(gameResWidth) + "x" + std::to_string(gameResHeight) +
                       " | Pointer viewport: (" + std::to_string(pointerX) + ", " + std::to_string(pointerY) + ")" +
                       " | Pointer game: (" + std::to_string(gameX) + ", " + std::to_string(gameY) + ")");

        for (Entity e : m_ecs->GetActiveEntities()) {
            if (!m_ecs->HasComponent<ButtonComponent>(e)) continue;
            HandlePointerClick(e, pointerPosInGameSpace);
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

void ButtonSystem::HandlePointerClick(Entity buttonEntity, Vector3D pointerPos)
{
    if (!m_ecs->HasComponent<ButtonComponent>(buttonEntity)) return;

    auto& buttonComp = m_ecs->GetComponent<ButtonComponent>(buttonEntity);

    // Skip if button is not interactable
    if (!buttonComp.interactable) return;

    if (m_ecs->HasComponent<SpriteRenderComponent>(buttonEntity)) {
        auto& spriteComponent = m_ecs->GetComponent<SpriteRenderComponent>(buttonEntity);
        if (spriteComponent.is3D) {
            ENGINE_LOG_WARN("[ButtonSystem] 3D buttons not supported yet.");
            return;
        }

        auto& transform = m_ecs->GetComponent<Transform>(buttonEntity);
        bool hit = false;

        // Check collision based on button shape
        if (buttonComp.shape == ButtonShape::CIRCLE) {
            // Circle collision detection
            float worldRadius = buttonComp.circleRadius * transform.localScale.x;
            float dx = pointerPos.x - transform.localPosition.x;
            float dy = pointerPos.y - transform.localPosition.y;
            float distanceSquared = dx * dx + dy * dy;
            float radiusSquared = worldRadius * worldRadius;

            hit = (distanceSquared <= radiusSquared);

            if (hit) {
                ENGINE_LOG_INFO("[ButtonSystem] Circle button hit! Entity: " + std::to_string(buttonEntity) +
                              " Center: (" + std::to_string(transform.localPosition.x) + ", " +
                              std::to_string(transform.localPosition.y) + ") Radius: " + std::to_string(worldRadius));
            }
        }
        else {
            // Rectangle collision detection (default)
            float halfExtentsX = transform.localScale.x / 2.0f;
            float halfExtentsY = transform.localScale.y / 2.0f;
            float minX = transform.localPosition.x - halfExtentsX;
            float maxX = transform.localPosition.x + halfExtentsX;
            float minY = transform.localPosition.y - halfExtentsY;
            float maxY = transform.localPosition.y + halfExtentsY;

            hit = (pointerPos.x >= minX && pointerPos.x <= maxX &&
                   pointerPos.y >= minY && pointerPos.y <= maxY);

            if (hit) {
                ENGINE_LOG_INFO("[ButtonSystem] Rect button hit! Entity: " + std::to_string(buttonEntity) +
                              " Bounds: (" + std::to_string(minX) + "-" + std::to_string(maxX) + ", " +
                              std::to_string(minY) + "-" + std::to_string(maxY) + ")");
            }
        }

        if (hit) {
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