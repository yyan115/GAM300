#include "ECS/ECSManager.hpp"
#include "pch.h"
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

    auto& jolt = physicsSystem->GetJoltSystem();

    for (auto e : ecs.GetAllEntities()) // Assumes ECSManager provides all entities
    {
        if (!ecs.HasComponent<CharacterControllerComponent>(e) ||
            !ecs.HasComponent<Transform>(e))
            continue;

        auto& cc = ecs.GetComponent<CharacterControllerComponent>(e);
        auto& tr = ecs.GetComponent<Transform>(e);

        // Lazy initialization of runtime controller
        if (!cc.runtimeController)
        {
            cc.runtimeController = new CharacterController(&jolt);
        }

        if (!cc.enabled || !cc.runtimeController) continue;

        // Update the character controller
        cc.runtimeController->Update(dt);

        // Sync position back to ECS transform
        auto pos = cc.runtimeController->GetPosition();
        tr.localPosition = Vector3D(pos.GetX(), pos.GetY(), pos.GetZ());
        tr.isDirty = true;
    }
}

void CharacterControllerSystem::Shutdown(ECSManager& ecs)
{
    for (auto e : ecs.GetAllEntities())
    {
        if (!ecs.HasComponent<CharacterControllerComponent>(e)) continue;

        auto& cc = ecs.GetComponent<CharacterControllerComponent>(e);
        if (cc.runtimeController)
        {
            delete cc.runtimeController;
            cc.runtimeController = nullptr;
        }
    }
} 