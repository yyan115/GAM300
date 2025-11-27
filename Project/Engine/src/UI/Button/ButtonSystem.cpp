// ButtonSystem.cpp
#include "pch.h"
#include "UI/Button/ButtonSystem.hpp"
#include "UI/Button/ButtonComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "Logging.hpp"
#include "TimeManager.hpp"


void ButtonSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;
}

void ButtonSystem::Update() {
    // This only runs during play mode in editor
    // Update any button-related state here
    UpdateButtonStates();
}

// Honestly think that you can proa
void ButtonSystem::UpdateButtonStates() {
    if (!m_ecs) return;

    //float dt = static_cast<float>(TimeManager::GewtDeltaTime());

    // Update cooldown timers or other runtime state if needed
    for (Entity e : m_ecs->GetActiveEntities()) {
        //if (m_ecs->HasComponent<ScriptComponentData>(e)) {
        //    auto& scriptComp = m_ecs->GetComponent<ScriptComponentData>(e);

        //    for (const auto& script : scriptComp.scripts) {
        //        std::cout << "Script GUID: " << script.scriptGuidStr << std::endl;
        //        std::cout << "Script Path: " << script.scriptPath << std::endl;
        //    }
        //}

        if (!m_ecs->HasComponent<ButtonComponentData>(e)) continue;

        auto& buttonData = m_ecs->GetComponent<ButtonComponentData>(e);


        // Example: Update any time-based state here
        // (cooldowns, animations, etc.)

        // Check for collision
        HandleMouseClick(e, Vector3D(InputManager::GetMouseX(), InputManager::GetMouseY(), 0.0f));
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
            // Ignore 3D buttons for now
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

        //if (mousePos.x >= minX && mousePos.x <= maxX && mousePos.y >= minY && mousePos.y <= maxY) {
            TriggerButton(buttonEntity);
        //}
    }
   //I just place here because this demands spritecomponent otherwise
   TriggerButton(buttonEntity);
}

void ButtonSystem::TriggerButton(Entity buttonEntity) {
    if (!m_ecs) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonSystem] Cannot trigger button: ECSManager not available");
        return;
    }

    if (!m_ecs->HasComponent<ButtonComponentData>(buttonEntity)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonSystem] Entity ", buttonEntity, " has no ButtonComponentData");
        return;
    }

    const auto& buttonData = m_ecs->GetComponent<ButtonComponentData>(buttonEntity);

    if (!buttonData.interactable) {
        ENGINE_PRINT(EngineLogging::LogLevel::Debug,
            "[ButtonSystem] Button ", buttonEntity, " is not interactable");
        return;
    }

    ProcessButtonClick(buttonEntity);
}

void ButtonSystem::ProcessButtonClick(Entity buttonEntity) {
    // The actual work is delegated to ButtonComponent
    // Create a temporary ButtonComponent to handle the click
    ButtonComponent tempButton(buttonEntity);
    tempButton.OnClick();

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[ButtonSystem] Processed click for button ", buttonEntity);
}