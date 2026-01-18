#pragma once
#include "ECS/System.hpp"

class ECSManager;

/**
 * @brief System that positions UI elements based on their UIAnchorComponent settings
 *
 * This system should run BEFORE rendering systems (SpriteSystem, ButtonSystem, TextSystem)
 * so that Transform positions are updated before rendering.
 *
 * The system:
 * 1. Gets the current viewport size from GraphicsManager
 * 2. For each entity with UIAnchorComponent:
 *    - Calculates screen position from anchor + offset
 *    - Updates Transform.localPosition
 *    - For stretch/scale modes, updates Transform.localScale
 */
class UIAnchorSystem : public System
{
public:
    UIAnchorSystem() = default;
    ~UIAnchorSystem() = default;

    void Initialise(ECSManager& ecsManager);
    void Update();
    void Shutdown();

private:
    ECSManager* m_ecs = nullptr;

    // Cache viewport size to detect changes
    int m_lastViewportWidth = 0;
    int m_lastViewportHeight = 0;
};
