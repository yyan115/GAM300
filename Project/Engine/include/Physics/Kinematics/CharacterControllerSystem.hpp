#pragma once
#include "ECS/System.hpp"
#include "CharacterController.hpp"
#include <unordered_map>
#include <unordered_set>

class ECSManager;
using Entity = unsigned int;

// Custom char-vs-char collision that skips detection for immovable characters.
// Immovable characters won't be pushed by others, but others still collide with them.
class CharacterVsCharacterCollisionFiltered : public JPH::CharacterVsCharacterCollision
{
public:
    void Add(JPH::CharacterVirtual* inCharacter) { mCharacters.push_back(inCharacter); }
    void Remove(const JPH::CharacterVirtual* inCharacter);

    void SetImmovable(const JPH::CharacterVirtual* inCharacter, bool immovable);
    bool IsImmovable(const JPH::CharacterVirtual* inCharacter) const;

    virtual void CollideCharacter(const JPH::CharacterVirtual* inCharacter, JPH::RMat44Arg inCenterOfMassTransform, const JPH::CollideShapeSettings& inCollideShapeSettings, JPH::RVec3Arg inBaseOffset, JPH::CollideShapeCollector& ioCollector) const override;
    virtual void CastCharacter(const JPH::CharacterVirtual* inCharacter, JPH::RMat44Arg inCenterOfMassTransform, JPH::Vec3Arg inDirection, const JPH::ShapeCastSettings& inShapeCastSettings, JPH::RVec3Arg inBaseOffset, JPH::CastShapeCollector& ioCollector) const override;

    JPH::Array<JPH::CharacterVirtual*> mCharacters;
    std::unordered_set<const JPH::CharacterVirtual*> mImmovableCharacters;
};

class CharacterControllerSystem : public System {
public:
    CharacterControllerSystem() = default;

    CharacterControllerSystem(JPH::PhysicsSystem* physicsSystem)
        : m_physicsSystem(physicsSystem)
    {
        m_charVsCharCollision = new CharacterVsCharacterCollisionFiltered();
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
            m_charVsCharCollision = new CharacterVsCharacterCollisionFiltered();
        }
    }

    // ADD INTO MAP
    CharacterController* CreateController(Entity id, ColliderComponent& collider, Transform& transform);

    //LUA CALLS CREATE CONTROLLER -> CALLS


    void Update(float deltaTime, ECSManager& ecsManager);
    void Shutdown();

    void RemoveController(Entity entity);
    CharacterController* GetController(Entity entity);
    void DisableCollision(Entity entity);
    void SetImmovable(Entity entity, bool immovable);


private:
    JPH::PhysicsSystem* m_physicsSystem = nullptr;
    std::unordered_map<Entity, std::unique_ptr<CharacterController>> m_controllers;
    CharacterVsCharacterCollisionFiltered* m_charVsCharCollision = nullptr;
};