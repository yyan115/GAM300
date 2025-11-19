#pragma once
#include <memory>
#include <vector>
#include "ECS/System.hpp"

class SpriteAnimationSystem : public System
{
public:
    SpriteAnimationSystem() = default;
    ~SpriteAnimationSystem() = default;

    bool Initialise();
    void Update();
};