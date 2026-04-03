#pragma once

#include "pch.h"
#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Transform/TransformComponent.hpp"
#include "Transform/TransformSystem.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Geometry/RayAABox.h>

// ============================================================================
// CharacterVsCharacterCollisionFiltered
// ============================================================================

void CharacterVsCharacterCollisionFiltered::Remove(const JPH::CharacterVirtual* inCharacter)
{
    auto i = std::find(mCharacters.begin(), mCharacters.end(), inCharacter);
    if (i != mCharacters.end())
        mCharacters.erase(i);
    mImmovableCharacters.erase(inCharacter);
}

void CharacterVsCharacterCollisionFiltered::SetImmovable(const JPH::CharacterVirtual* inCharacter, bool immovable)
{
    if (immovable)
        mImmovableCharacters.insert(inCharacter);
    else
        mImmovableCharacters.erase(inCharacter);
}

bool CharacterVsCharacterCollisionFiltered::IsImmovable(const JPH::CharacterVirtual* inCharacter) const
{
    return mImmovableCharacters.count(inCharacter) > 0;
}

void CharacterVsCharacterCollisionFiltered::CollideCharacter(const JPH::CharacterVirtual* inCharacter, JPH::RMat44Arg inCenterOfMassTransform, const JPH::CollideShapeSettings& inCollideShapeSettings, JPH::RVec3Arg inBaseOffset, JPH::CollideShapeCollector& ioCollector) const
{
    bool queryerIsImmovable = IsImmovable(inCharacter);

    JPH::Mat44 transform1 = inCenterOfMassTransform.PostTranslated(-inBaseOffset).ToMat44();
    const JPH::Shape* shape1 = inCharacter->GetShape();
    JPH::CollideShapeSettings settings = inCollideShapeSettings;
    JPH::AABox bounds1 = shape1->GetWorldSpaceBounds(transform1, JPH::Vec3::sOne());

    for (const JPH::CharacterVirtual* c : mCharacters)
    {
        if (c == inCharacter || ioCollector.ShouldEarlyOut())
            continue;

        // Skip collision when an immovable character queries against a non-immovable one (the player).
        // Immovable vs immovable (enemy vs enemy) still collides so they don't overlap.
        if (queryerIsImmovable && !IsImmovable(c))
            continue;

        JPH::Mat44 transform2 = c->GetCenterOfMassTransform().PostTranslated(-inBaseOffset).ToMat44();
        settings.mMaxSeparationDistance = inCollideShapeSettings.mMaxSeparationDistance + c->GetCharacterPadding();

        const JPH::Shape* shape2 = c->GetShape();
        JPH::AABox bounds2 = shape2->GetWorldSpaceBounds(transform2, JPH::Vec3::sOne());
        bounds2.ExpandBy(JPH::Vec3::sReplicate(settings.mMaxSeparationDistance));
        if (!bounds1.Overlaps(bounds2))
            continue;

        ioCollector.SetUserData(reinterpret_cast<uint64_t>(c));
        JPH::CollisionDispatch::sCollideShapeVsShape(shape1, shape2, JPH::Vec3::sOne(), JPH::Vec3::sOne(), transform1, transform2, JPH::SubShapeIDCreator(), JPH::SubShapeIDCreator(), settings, ioCollector);
    }
    ioCollector.SetUserData(0);
}

void CharacterVsCharacterCollisionFiltered::CastCharacter(const JPH::CharacterVirtual* inCharacter, JPH::RMat44Arg inCenterOfMassTransform, JPH::Vec3Arg inDirection, const JPH::ShapeCastSettings& inShapeCastSettings, JPH::RVec3Arg inBaseOffset, JPH::CastShapeCollector& ioCollector) const
{
    bool queryerIsImmovable = IsImmovable(inCharacter);

    JPH::Mat44 transform1 = inCenterOfMassTransform.PostTranslated(-inBaseOffset).ToMat44();
    JPH::ShapeCast shape_cast(inCharacter->GetShape(), JPH::Vec3::sOne(), transform1, inDirection);

    JPH::Vec3 origin = shape_cast.mShapeWorldBounds.GetCenter();
    JPH::Vec3 extents = shape_cast.mShapeWorldBounds.GetExtent();

    for (const JPH::CharacterVirtual* c : mCharacters)
    {
        if (c == inCharacter || ioCollector.ShouldEarlyOut())
            continue;

        // Same filtering: immovable only skips non-immovable targets
        if (queryerIsImmovable && !IsImmovable(c))
            continue;

        JPH::Mat44 transform2 = c->GetCenterOfMassTransform().PostTranslated(-inBaseOffset).ToMat44();
        const JPH::Shape* shape2 = c->GetShape();
        JPH::AABox bounds2 = shape2->GetWorldSpaceBounds(transform2, JPH::Vec3::sOne());
        bounds2.ExpandBy(extents);
        if (!JPH::RayAABoxHits(origin, inDirection, bounds2.mMin, bounds2.mMax))
            continue;

        ioCollector.SetUserData(reinterpret_cast<uint64_t>(c));
        JPH::CollisionDispatch::sCastShapeVsShapeWorldSpace(shape_cast, inShapeCastSettings, shape2, JPH::Vec3::sOne(), {}, transform2, JPH::SubShapeIDCreator(), JPH::SubShapeIDCreator(), ioCollector);
    }
    ioCollector.SetUserData(0);
}

// ============================================================================
// CharacterControllerSystem
// ============================================================================

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

    // Remove the regular physics body — CharacterVirtual handles movement.
    // Hurtboxes are separate bone-attached colliders on the HURTBOX layer (set up in editor).
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs.physicsSystem) {
        ecs.physicsSystem->RemoveBody(id);
    }
    if (ecs.HasComponent<RigidBodyComponent>(id)) {
        auto& rb = ecs.GetComponent<RigidBodyComponent>(id);
        rb.enabled = false;
        rb.gravityFactor = 0.0f;
    }

    return ptr;
}


//ITERATE MAP CONTROLLERS

void CharacterControllerSystem::Update(float deltaTime, ECSManager& ecsManager) {
    PROFILE_FUNCTION();

    for (auto& [entityId, controller] : m_controllers) {
        if (!controller) continue;

        controller->Update(deltaTime);

        // Sync CharacterVirtual position back to Transform (same role as PhysicsSyncBack for regular bodies)
        if (ecsManager.HasComponent<Transform>(entityId)) {
            Vector3D pos = controller->GetPosition();
            if (ecsManager.HasComponent<ParentComponent>(entityId))
                ecsManager.transformSystem->SetWorldPosition(entityId, pos);
            else
                ecsManager.transformSystem->SetLocalPosition(entityId, pos);
        }
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

void CharacterControllerSystem::DisableCollision(Entity entity)
{
    auto it = m_controllers.find(entity);
    if (it == m_controllers.end()) return;
    JPH::CharacterVirtual* ch = const_cast<JPH::CharacterVirtual*>(it->second->GetCharacterVirtual());
    if (!ch) return;
    if (m_charVsCharCollision)
        m_charVsCharCollision->Remove(ch);
    ch->SetCharacterVsCharacterCollision(nullptr);
}

void CharacterControllerSystem::SetImmovable(Entity entity, bool immovable)
{
    auto it = m_controllers.find(entity);
    if (it == m_controllers.end()) return;

    it->second->SetImmovable(immovable);

    const JPH::CharacterVirtual* ch = it->second->GetCharacterVirtual();
    if (ch && m_charVsCharCollision)
        m_charVsCharCollision->SetImmovable(ch, immovable);
}