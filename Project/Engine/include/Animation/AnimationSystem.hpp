#pragma once
#include "pch.h"
#include <memory>
#include <vector>
#include "ECS/System.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Animation/AnimationComponent.hpp"

class ENGINE_API AnimationSystem : public System
{
public:
    AnimationSystem() = default;
    ~AnimationSystem() = default;

    bool ENGINE_API Initialise();
    void InitialiseAnimationComponent(Entity entity, ModelRenderComponent& modelComp, AnimationComponent& animComp);
    void Update();
};