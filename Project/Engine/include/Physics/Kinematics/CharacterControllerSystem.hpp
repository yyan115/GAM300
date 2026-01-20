#pragma once
#include "ECS/System.hpp"
#include "CharacterController.hpp"
#include <unordered_map>

class ECSManager;
using Entity = unsigned int;

class CharacterControllerSystem : public System {
public:
    CharacterControllerSystem() = default;

    CharacterControllerSystem(JPH::PhysicsSystem* physicsSystem)
        : m_physicsSystem(physicsSystem)
    {
        m_charVsCharCollision = new JPH::CharacterVsCharacterCollisionSimple();
    }

    ~CharacterControllerSystem()
    {
        Shutdown();
        delete m_charVsCharCollision;
    }

    void SetPhysicsSystem(JPH::PhysicsSystem* physicsSystem) {
        m_physicsSystem = physicsSystem;
        // CREATE collision system if it doesn't exist
        if (!m_charVsCharCollision) {
            m_charVsCharCollision = new JPH::CharacterVsCharacterCollisionSimple();
        }
    }

    // ADD INTO MAP
    CharacterController* CreateController(Entity id, ColliderComponent& collider, Transform& transform);

    //LUA CALLS CREATE CONTROLLER -> CALLS


    void Update(float deltaTime, ECSManager& ecsManager);
    void Shutdown();

    void RemoveController(Entity entity);
    CharacterController* GetController(Entity entity);


private:
    JPH::PhysicsSystem* m_physicsSystem = nullptr;
    std::unordered_map<Entity, std::unique_ptr<CharacterController>> m_controllers;
    JPH::CharacterVsCharacterCollisionSimple* m_charVsCharCollision = nullptr;
};