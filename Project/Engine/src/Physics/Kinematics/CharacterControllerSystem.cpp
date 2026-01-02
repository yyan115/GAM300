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


//ITERATE MAP CONTROLLERS

void CharacterControllerSystem::Update(float deltaTime, ECSManager& ecsManager) {
    PROFILE_FUNCTION();
    for (auto& [entityId, controller] : m_controllers) {
        if (controller)
            controller->Update(deltaTime);
    }
}

void CharacterControllerSystem::Shutdown() {
    PROFILE_FUNCTION();

    // Manually destroy each controller 
    for (auto& [entityId, controller] : m_controllers) {
        if (controller) {
            controller.reset(); // Explicitly destroy
        }
    }
    // Clear the map
    m_controllers.clear();
}

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