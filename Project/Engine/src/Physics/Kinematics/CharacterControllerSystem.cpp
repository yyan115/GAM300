//// CharacterControllerSystem.cpp
//#pragma once
//
//#include "pch.h"
//#include "Physics/Kinematics/CharacterController.hpp"
//#include "ECS/ECSManager.hpp"
//#include "ECS/ECSRegistry.hpp"
//#include "Physics/ColliderComponent.hpp"
//#include "Transform/TransformComponent.hpp"
//#include "Performance/PerformanceProfiler.hpp"
//
//#ifdef __ANDROID__
//#include <android/log.h>
//#endif
//
//void CharacterControllerSystem::Initialise(ECSManager& ecsManager, JPH::PhysicsSystem* physicsSystem) {
//    if (m_initialized) {
//        std::cout << "[CharacterController] Already initialized, skipping..." << std::endl;
//        return;
//    }
//
//    if (!physicsSystem) {
//        std::cerr << "[CharacterController] ERROR: PhysicsSystem is null!" << std::endl;
//        return;
//    }
//
//    m_physicsSystem = physicsSystem;
//
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300",
//        "[CharacterController] Initialise called, entities=%zu", entities.size());
//#endif
//
//    // Clear any existing controllers from previous play session
//    m_controllers.clear();
//
//    // Create controllers for all entities that have Collider + Transform
//    // (You can add a tag component later if you want to mark specific entities as character controllers)
//    for (auto& e : entities) {
//        // Skip entities that don't have the required components
//        if (!ecsManager.HasComponent<ColliderComponent>(e) ||
//            !ecsManager.HasComponent<Transform>(e)) {
//            continue;
//        }
//
//        auto& collider = ecsManager.GetComponent<ColliderComponent>(e);
//        auto& transform = ecsManager.GetComponent<Transform>(e);
//
//        // Create the character controller
//        auto controller = std::make_unique<CharacterController>(m_physicsSystem);
//
//        if (controller->Initialise(collider, transform)) {
//            m_controllers[e] = std::move(controller);
//
//#ifdef __ANDROID__
//            __android_log_print(ANDROID_LOG_INFO, "GAM300",
//                "[CharacterController] Created controller for entity %d", e);
//#endif
//        }
//        else {
//            std::cerr << "[CharacterController] Failed to initialize controller for entity " << e << std::endl;
//        }
//    }
//
//    m_initialized = true;
//    std::cout << "[CharacterController] Initialized " << m_controllers.size() << " controllers" << std::endl;
//}
//
//void CharacterControllerSystem::Update(float deltaTime, ECSManager& ecsManager) {
//    PROFILE_FUNCTION();
//
//    if (!m_initialized || !m_physicsSystem) {
//        return;
//    }
//
//#ifdef __ANDROID__
//    static int updateCount = 0;
//    if (updateCount++ % 60 == 0) {
//        __android_log_print(ANDROID_LOG_INFO, "GAM300",
//            "[CharacterController] Update called, deltaTime=%f, controllers=%zu",
//            deltaTime, m_controllers.size());
//    }
//#endif
//
//    if (m_controllers.empty()) return;
//
//    // Update each character controller
//    for (auto& [entityID, controller] : m_controllers) {
//        // Skip if entity no longer has required components
//        if (!ecsManager.HasComponent<Transform>(entityID)) {
//            continue;
//        }
//
//        // Just call the controller's Update - it handles everything internally
//        controller->Update(deltaTime, ecsManager);
//
//#ifdef __ANDROID__
//        if (updateCount % 60 == 0) {
//            Vector3D pos = controller->GetPosition();
//            __android_log_print(ANDROID_LOG_INFO, "GAM300",
//                "[CharacterController] Entity %d pos: (%f, %f, %f)",
//                entityID, pos.x, pos.y, pos.z);
//        }
//#endif
//    }
//}
//
//CharacterController* CharacterControllerSystem::GetController(int entityID) {
//    auto it = m_controllers.find(entityID);
//    return (it != m_controllers.end()) ? it->second.get() : nullptr;
//}
//
//void CharacterControllerSystem::Shutdown() {
//    std::cout << "[CharacterController] Shutting down " << m_controllers.size() << " controllers..." << std::endl;
//
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300",
//        "[CharacterController] Shutdown called");
//#endif
//
//    // Clear all controllers (unique_ptr will handle cleanup)
//    m_controllers.clear();
//
//    m_physicsSystem = nullptr;
//    m_initialized = false;
//
//    std::cout << "[CharacterController] Shutdown complete" << std::endl;
//}