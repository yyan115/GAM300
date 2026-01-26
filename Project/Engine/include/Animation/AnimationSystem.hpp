#pragma once
#include "pch.h"
#include <memory>
#include <vector>
#include "ECS/System.hpp"

class AnimationSystem : public System
{
public:
    AnimationSystem() = default;
    ~AnimationSystem() = default;

    bool Initialise();
    void Update();
};