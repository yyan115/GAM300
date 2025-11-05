#pragma once
#include <memory>
#include <string>
#include "ECS/ECSManager.hpp"
#include "ECS/Entity.hpp"
#include "Events.hpp"

enum class BrainKind : int { None = 0, Grunt, Boss };

struct IBrain {
    virtual ~IBrain() = default;
    virtual void onEnter(class ECSManager&, unsigned /*Entity*/) = 0;
    virtual void onUpdate(class ECSManager&, unsigned /*Entity*/, float dt) = 0;
    virtual void onExit(class ECSManager&, unsigned /*Entity*/) = 0;
    virtual const char* activeStateName() const { return ""; } // default
};

struct Brain {
    REFL_SERIALIZABLE
    BrainKind kind{ BrainKind::None };
    std::shared_ptr<IBrain> impl;
    bool started{ false };
    std::string activeState;
};