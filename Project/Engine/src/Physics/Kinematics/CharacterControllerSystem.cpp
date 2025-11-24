#include "pch.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Physics/JoltInclude.hpp"
#include "Physics/Kinematics/CharacterControllerComponent.hpp"
#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Transform/TransformComponent.hpp"

void CharacterControllerSystem::Initialise(ECSManager& ecs, PhysicsSystem* physics)
{
    physicsSystem = physics;

    for (auto e : ecs.GetAllEntities())
    {
        if (!ecs.HasComponent<CharacterControllerComponent>(e))
            continue;

        auto& cc = ecs.GetComponent<CharacterControllerComponent>(e);
        auto& collider = ecs.GetComponent<ColliderComponent>(e);
        auto& transform = ecs.GetComponent<Transform>(e);

        if (!cc.runtimeController)
        {
            // Create the runtime controller for this entity
            cc.runtimeController = new CharacterController(&physicsSystem->GetJoltSystem());
            cc.runtimeController->Initialise(cc, collider, transform);
        }
    }
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

        // TEMP: Move forward along z
        float moveZ = cc.speed * dt;

        //// Option 1: If runtimeController exists, call Move()
        if (cc.runtimeController)
        {
            cc.runtimeController->Move(0, 0, moveZ);        //here for testing only

            cc.runtimeController->Update(dt);

            //SYNC ECS TRANSFORM WITH RUNTIME
            JPH::Vec3 pos = cc.runtimeController->GetPosition();
            tr.localPosition = Vector3D(pos.GetX(), pos.GetY(), pos.GetZ());

        }
    }
}

void CharacterControllerSystem::Shutdown(ECSManager& ecs)
{
    // Iterate all entities with CharacterControllerComponent
    for (auto e : ecs.GetAllEntities())
    {
        if (!ecs.HasComponent<CharacterControllerComponent>(e))
            continue;

        auto& cc = ecs.GetComponent<CharacterControllerComponent>(e);
        if (cc.runtimeController)
        {
            delete cc.runtimeController;
            cc.runtimeController = nullptr;
        }
    }
    entities.clear();
}