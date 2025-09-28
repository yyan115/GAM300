#pragma once
#include <memory>
#include "ECS/System.hpp"
#include "ECS/Entity.hpp"
#include "Math/Vector3D.hpp"
#include "../Engine.h"

// Forward declarations to avoid including Jolt headers in public interface
namespace JPH {
    class PhysicsSystem;
    class JobSystem;
    class TempAllocator;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    class BodyInterface;
}

class ENGINE_API PhysicsSystem : public System {
public:
    PhysicsSystem();
    ~PhysicsSystem();

    void Initialise();
    void Update(float deltaTime);
    void Shutdown();

    // Physics world management
    void SetGravity(const Vector3D& gravity);
    Vector3D GetGravity() const;

    // Body management functions
    void CreatePhysicsBody(Entity entity);
    void UpdatePhysicsBody(Entity entity);
    void RemovePhysicsBody(Entity entity);

    // Physics queries
    bool Raycast(const Vector3D& origin, const Vector3D& direction, float maxDistance, Entity& hitEntity, Vector3D& hitPoint) const;

    // Force and velocity manipulation
    void ApplyForce(Entity entity, const Vector3D& force);
    void SetVelocity(Entity entity, const Vector3D& velocity);
    Vector3D GetVelocity(Entity entity) const;

private:
    // Jolt physics system components
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
    std::unique_ptr<JPH::JobSystem> m_jobSystem;
    std::unique_ptr<JPH::TempAllocator> m_tempAllocator;
    std::unique_ptr<JPH::BroadPhaseLayerInterface> m_broadPhaseLayerInterface;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_objectVsBroadPhaseLayerFilter;
    std::unique_ptr<JPH::ObjectLayerPairFilter> m_objectLayerPairFilter;

    // Helper functions
    void SyncTransformFromPhysics(Entity entity);
    void SyncTransformToPhysics(Entity entity);
    void CreateColliderShape(Entity entity);
    void UpdateColliderShape(Entity entity);

    // Constants for physics configuration
    static constexpr unsigned int cMaxBodies = 65536;
    static constexpr unsigned int cNumBodyMutexes = 0; // Autodetect
    static constexpr unsigned int cMaxBodyPairs = 65536;
    static constexpr unsigned int cMaxContactConstraints = 20480;
};