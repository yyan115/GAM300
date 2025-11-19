#include "pch.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Physics/JoltInclude.hpp"
#include "Physics/Kinematics/CharacterControllerComponent.hpp"
#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "Transform/TransformComponent.hpp"

void CharacterControllerSystem::Initialise(ECSManager& ecs, PhysicsSystem* physics)
{
    physicsSystem = physics;
}

void CharacterControllerSystem::Update(float dt, ECSManager& ecs)
{
    if (!physicsSystem) return;

    // If you later want to interact with Jolt, you can still get it:
    // auto& jolt = physicsSystem->GetJoltSystem();

    for (auto e : ecs.GetAllEntities())
    {
        if (!ecs.HasComponent<CharacterControllerComponent>(e) ||
            !ecs.HasComponent<Transform>(e))
            continue;

        auto& cc = ecs.GetComponent<CharacterControllerComponent>(e);
        auto& tr = ecs.GetComponent<Transform>(e);

        if (!cc.enabled)
            continue;

        // Mark transform as dirty so ECS knows it changed
        tr.isDirty = true;
    }
}

void CharacterControllerSystem::Shutdown()
{
    entities.clear();
}