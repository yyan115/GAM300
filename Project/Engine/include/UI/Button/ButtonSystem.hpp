#pragma once
// ButtonSystem.hpp
#include "ECS/System.hpp"
#include <Performance/PerformanceProfiler.hpp>
#include <Graphics/GraphicsManager.hpp>
#include <Input/InputManager.h>
class ECSManager;
using Entity = unsigned int;

class ButtonSystem : public System {
public:
    ButtonSystem() = default;
    ~ButtonSystem() = default;

    void Initialise(ECSManager& ecsManager);
    void Update();  // Only runs during play mode
    void Shutdown();

    // Called when a button UI element is clicked (from input/UI system)
    void TriggerButton(Entity buttonEntity);

private:
    void ProcessButtonClick(Entity buttonEntity);
    void UpdateButtonStates();
    void HandlePointerClick(Entity buttonEntity, Vector3D pointerPos);
    ECSManager* m_ecs = nullptr;
};