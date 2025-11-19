#pragma once
#include "Engine.h"   // For ENGINE_API macro
#include "ECS/System.hpp"
#include "ECS/ECSManager.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Physics/Kinematics/CharacterControllerComponent.hpp"


class CharacterControllerSystem : public System
{
public:
    CharacterControllerSystem() = default;
    ~CharacterControllerSystem() = default;

    // Initialise with reference to ECS and PhysicsSystem
    void Initialise(ECSManager& ecs, PhysicsSystem* physics);

    // Update all character controllers
    void Update(float dt, ECSManager& ecs);

    // Shutdown and clean up runtime controllers
    void Shutdown(ECSManager& ecs);

private:
    PhysicsSystem* physicsSystem = nullptr;
};