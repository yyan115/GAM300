#pragma once
#include <memory>
#include "ECS/ECSManager.hpp"
#include "ECS/Entity.hpp"
#include "Events.hpp"

struct IBrain {
    virtual ~IBrain() = default;
    virtual void onEnter(class ECSManager&, unsigned /*Entity*/) = 0;
    virtual void onUpdate(class ECSManager&, unsigned /*Entity*/, float dt) = 0;
    virtual void onExit(class ECSManager&, unsigned /*Entity*/) = 0;
};

struct Brain {
    std::shared_ptr<IBrain> impl;
    bool started{ false };
};