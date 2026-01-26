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

    //IF ALREADY CREATED, JUST RESET THE TRANSFORM AND RETURN
    if (m_controllers.contains(id)) {
        std::cerr << "[WARN] Entity " << id << " already has a controller\n";
        CharacterController* existingController = GetController(id);
        existingController->SetPosition(transform);     //Reset Character Position
        return m_controllers[id].get();
    }


    auto controller = std::make_unique<CharacterController>(m_physicsSystem);

    if (!controller->Initialise(collider, transform)) {
        std::cerr << "[ERROR] Failed to initialise controller for entity " << id << "\n";
        return nullptr;
    }

    JPH::CharacterVirtual* character = const_cast<JPH::CharacterVirtual*>(controller->GetCharacterVirtual());

    if (character && m_charVsCharCollision)
    {
        character->SetCharacterVsCharacterCollision(m_charVsCharCollision);
        m_charVsCharCollision->Add(character);
    }

    CharacterController* ptr = controller.get(); // raw pointer for Lua access

    //add into map
    m_controllers.emplace(id, std::move(controller));

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

    // Remove from char-vs-char collision first
    if (m_charVsCharCollision) {
        for (auto& [entityId, controller] : m_controllers) {
            if (!controller) continue;
            JPH::CharacterVirtual* character =
                const_cast<JPH::CharacterVirtual*>(controller->GetCharacterVirtual());
            if (character) {
                m_charVsCharCollision->Remove(character);
            }
        }
    }
    // Clear the map
    m_controllers.clear();
}

void CharacterControllerSystem::RemoveController(Entity entity)
{
    auto it = m_controllers.find(entity);
    if (it != m_controllers.end()) {
        JPH::CharacterVirtual* character = const_cast<JPH::CharacterVirtual*>(it->second->GetCharacterVirtual());
        if (character && m_charVsCharCollision) {
            m_charVsCharCollision->Remove(character);
        }

        m_controllers.erase(it);
    }
}

CharacterController* CharacterControllerSystem::GetController(Entity entity)
{
    auto it = m_controllers.find(entity);
    return (it != m_controllers.end()) ? it->second.get() : nullptr;
}