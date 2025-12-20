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
        : m_physicsSystem(physicsSystem) {}

    ~CharacterControllerSystem() = default;

    void SetPhysicsSystem(JPH::PhysicsSystem* physicsSystem) {
        m_physicsSystem = physicsSystem;
    }

    // ADD INTO MAP
    CharacterController* CreateController(Entity id, ColliderComponent& collider, Transform& transform);
        //std::cout << "Initialise is being called for SYSTEM CONTROLLER" << std::endl;
        
        //LUA CALLS CREATE CONTROLLER -> CALLS



        //auto controller = std::make_unique<CharacterController>(m_physicsSystem);
        //if (!controller->Initialise(collider, transform))
        //    return false;

        //m_controllers[entity] = std::move(controller);
        //return true;

    // Called every frame
    void Update(float deltaTime) {
        //for (auto& [entity, controller] : m_controllers) {
        //    controller->Update(deltaTime, ecsManager);
        //}

        //std::cout << "UPDATE is being called for SYSTEM CONTROLLER" << std::endl;

    }

    //// Get a pointer to the runtime controller
    //CharacterController* GetController(Entity entity) {
    //    auto it = m_controllers.find(entity);
    //    return (it != m_controllers.end()) ? it->second.get() : nullptr;
    //}

    //// Remove controller for an entity
    //void RemoveController(Entity entity) {
    //    m_controllers.erase(entity);
    //}

    //// Shutdown all controllers
    //void Shutdown() {
    //    m_controllers.clear();
    //}

private:
    JPH::PhysicsSystem* m_physicsSystem = nullptr;
    std::unordered_map<Entity, std::unique_ptr<CharacterController>> m_controllers;
};
