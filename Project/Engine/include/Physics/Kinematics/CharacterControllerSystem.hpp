//#pragma once
//// CharacterControllerSystem.hpp
//
//#include "ECS/System.hpp"
//#include "CharacterController.hpp"
//#include <unordered_map>
//
//class ECSManager;
//using Entity = unsigned int;
//
//class CharacterControllerSystem : public System {
//public:
//    CharacterControllerSystem(JPH::PhysicsSystem* physicsSystem)
//        : m_physicsSystem(physicsSystem) {}
//
//    ~CharacterControllerSystem() = default;
//
//    // Initialise a controller for a specific entity
//    bool Initialise(ColliderComponent& collider, Transform& transform) {
//        auto controller = std::make_unique<CharacterController>(m_physicsSystem);
//        if (!controller->Initialise(collider, transform))
//            return false;
//
//        m_controllers[entity] = std::move(controller);
//        return true;
//    }
//
//    // Called every frame
//    void Update(float deltaTime, ECSManager& ecsManager) {
//        for (auto& [entity, controller] : m_controllers) {
//            controller->Update(deltaTime, ecsManager);
//        }
//    }
//
//    // Get a pointer to the runtime controller
//    CharacterController* GetController(Entity entity) {
//        auto it = m_controllers.find(entity);
//        return (it != m_controllers.end()) ? it->second.get() : nullptr;
//    }
//
//    // Remove controller for an entity
//    void RemoveController(Entity entity) {
//        m_controllers.erase(entity);
//    }
//
//    // Shutdown all controllers
//    void Shutdown() {
//        m_controllers.clear();
//    }
//
//private:
//    JPH::PhysicsSystem* m_physicsSystem = nullptr;
//    std::unordered_map<Entity, std::unique_ptr<CharacterController>> m_controllers;
//};
