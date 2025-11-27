// ButtonSystem.cpp
#include "pch.h"
#include "UI/Button/ButtonSystem.hpp"
#include "UI/Button/ButtonComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "Logging.hpp"
#include "TimeManager.hpp"

void ButtonSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;

    ENGINE_PRINT("[ButtonSystem] Initialised");
}

void ButtonSystem::Update() {
    // This only runs during play mode in editor
    // Update any button-related state here
    UpdateButtonStates();
}

void ButtonSystem::UpdateButtonStates() {
    if (!m_ecs) return;

    float dt = static_cast<float>(TimeManager::GetDeltaTime());

    // Update cooldown timers or other runtime state if needed
    for (Entity e : entities) {
        if (!m_ecs->HasComponent<ButtonComponentData>(e)) continue;

        auto& buttonData = m_ecs->GetComponent<ButtonComponentData>(e);

        // Example: Update any time-based state here
        // (cooldowns, animations, etc.)
    }
}

void ButtonSystem::Shutdown() {
    m_ecs = nullptr;

    ENGINE_PRINT("[ButtonSystem] Shutdown complete");
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