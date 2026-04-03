#pragma once
#include "pch.h"
#include <memory>
#include <vector>
#include "ECS/System.hpp"

class SpriteAnimationSystem : public System
{
public:
    SpriteAnimationSystem() = default;
    ~SpriteAnimationSystem() = default;

    bool ENGINE_API Initialise();
    void Update();

private:
    bool wasInEditMode = true;  // Track if we were in edit mode last frame
};