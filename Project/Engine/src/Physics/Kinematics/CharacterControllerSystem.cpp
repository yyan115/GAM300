#pragma once

#include "pch.h"
#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Performance/PerformanceProfiler.hpp"


CharacterController* CharacterControllerSystem::CreateController(Entity id,
    ColliderComponent& collider,
    Transform& transform)
{
    if (m_controllers.contains(id)) {
        std::cerr << "[WARN] Entity " << id << " already has a controller\n";
        return m_controllers[id].get();
    }

    auto controller = std::make_unique<CharacterController>(m_physicsSystem);

    if (!controller->Initialise(collider, transform)) {
        std::cerr << "[ERROR] Failed to initialise controller for entity " << id << "\n";
        return nullptr;
    }

    //add into map
    m_controllers.emplace(id, std::move(controller));

    CharacterController* ptr = controller.get(); // raw pointer for Lua access

    return ptr;
}




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