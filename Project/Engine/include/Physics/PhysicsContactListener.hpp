/*****************************************************************************
* @File			PhysicsContactListener.hpp
* @Author		Ang Jia Jun Austin, a.jiajunaustin@digipen.edu
* @Co - Author -
*@Date			9 / 11 / 2025
* @Brief		Contact listener implementation for Jolt Physics collision callbacks.
* Handles collision validation and events, mapping physics bodies
* to game entities for event processing.
*
* Copyright(C) 2025 DigiPen Institute of Technology.Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
* ********************************************************************************/

#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>
#include "Logging.hpp"
#include "Physics/CollisionLayers.hpp"

// Collision event data structure
struct CollisionEvent {
    int entityA;
    int entityB;
    JPH::Vec3 contactPoint;
    JPH::Vec3 contactNormal;
    float penetrationDepth;
};

class MyContactListener : public JPH::ContactListener {
public:
    using CollisionCallback = std::function<void(const CollisionEvent&)>;

    // Constructor
    MyContactListener(const std::unordered_map<JPH::BodyID, int>& idMap)
        : bodyToEntityMap(idMap)
        , enableLogging(false)
        , enableDetailedLogging(false)
    {}

    // Register callbacks for collision events
    void SetOnCollisionEnter(CollisionCallback callback) { onCollisionEnter = callback; }
    void SetOnCollisionExit(CollisionCallback callback) { onCollisionExit = callback; }
    void SetRootResolver(std::function<int(int)> resolver) { rootResolver = std::move(resolver); }

    // Toggle logging
    void EnableLogging(bool enable) { enableLogging = enable; }
    void EnableDetailedLogging(bool enable) { enableDetailedLogging = enable; }

    // Check if two entities are currently colliding
    bool AreEntitiesColliding(int entityA, int entityB) const {
        uint64_t key = MakeCollisionKey(entityA, entityB);
        return activeCollisions.find(key) != activeCollisions.end();
    }

    // Drain buffered collision events (call from main thread after physics.Update())
    void DrainEvents(std::vector<CollisionEvent>& outEnter, std::vector<CollisionEvent>& outExit) {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        outEnter.swap(m_pendingEnter);
        outExit.swap(m_pendingExit);
    }

    // Called when a contact point is being validated
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg /*inBaseOffset*/,
        const JPH::CollideShapeResult& /*inCollisionResult*/) override
    {
        if (enableDetailedLogging) {
            ENGINE_PRINT("[Collision] Validating contact between entities ",
                GetEntityID(inBody1), " (", GetMotionTypeName(inBody1), ") and ",
                GetEntityID(inBody2), " (", GetMotionTypeName(inBody2), ")\n");
        }

        const int entityA = GetEntityID(inBody1);
        const int entityB = GetEntityID(inBody2);
        if (entityA == -1 || entityB == -1) {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        const bool aIsSensor = inBody1.GetObjectLayer() == Layers::SENSOR;
        const bool bIsSensor = inBody2.GetObjectLayer() == Layers::SENSOR;
        if (rootResolver && (aIsSensor || bIsSensor)) {
            const int rootA = rootResolver(entityA);
            const int rootB = rootResolver(entityB);
            if (rootA != -1 && rootA == rootB) {
                return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
            }
        }

        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    // Called when a contact point is added
    virtual void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& /*ioSettings*/) override
    {
        int entityA = GetEntityID(inBody1);
        int entityB = GetEntityID(inBody2);

        if (entityA == -1 || entityB == -1) return;

        uint64_t key = MakeCollisionKey(entityA, entityB);

        // [FIX] LOCK THE MUTEX AT THE START
        // This protects 'activeCollisions', the vectors, AND your logging/callbacks.
        std::lock_guard<std::mutex> lock(m_eventMutex);

        // Only trigger callback if this is a new collision
        if (activeCollisions.insert(key).second) {
            if (enableLogging) {
                ENGINE_PRINT("[Collision] Enter: Entity ", entityA,
                    " (", GetMotionTypeName(inBody1), ") <-> Entity ", entityB,
                    " (", GetMotionTypeName(inBody2), ")\n");
            }

            CollisionEvent event;
            event.entityA = entityA;
            event.entityB = entityB;
            if (inManifold.mRelativeContactPointsOn1.size() > 0) {
                event.contactPoint = inManifold.GetWorldSpaceContactPointOn1(0);
                event.contactNormal = inManifold.mWorldSpaceNormal;
                event.penetrationDepth = inManifold.mPenetrationDepth;
            }

            if (onCollisionEnter) {
                onCollisionEnter(event);
            }

            // Buffer for main-thread dispatch (Lua callbacks)
            m_pendingEnter.push_back(event);

            if (enableDetailedLogging) {
                LogCollisionDetails(inBody1, inBody2, inManifold);
            }
        }
    }

    // Called when a contact is removed
    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
    {
        int entityA = GetEntityID(inSubShapePair.GetBody1ID());
        int entityB = GetEntityID(inSubShapePair.GetBody2ID());

        if (entityA == -1 || entityB == -1) return;

        uint64_t key = MakeCollisionKey(entityA, entityB);

        // [FIX] LOCK THE MUTEX AT THE START
        std::lock_guard<std::mutex> lock(m_eventMutex);

        if (activeCollisions.erase(key) > 0) {
            if (enableLogging) {
                ENGINE_PRINT("[Collision] Exit: Entity ", entityA,
                    " <-> Entity ", entityB, "\n");
            }

            CollisionEvent event;
            event.entityA = entityA;
            event.entityB = entityB;

            if (onCollisionExit) {
                onCollisionExit(event);
            }

            // Buffer for main-thread dispatch (Lua callbacks)
            m_pendingExit.push_back(event);
        }
    }

    // Clear all tracked collisions (useful for scene changes)
    void ClearCollisions() {
        activeCollisions.clear();
    }

private:
    const std::unordered_map<JPH::BodyID, int>& bodyToEntityMap;
    std::unordered_set<uint64_t> activeCollisions;

    CollisionCallback onCollisionEnter;
    CollisionCallback onCollisionExit;

    bool enableLogging;
    bool enableDetailedLogging;
    std::function<int(int)> rootResolver;

    // Thread-safe event buffers for main-thread dispatch
    std::mutex m_eventMutex;
    std::vector<CollisionEvent> m_pendingEnter;
    std::vector<CollisionEvent> m_pendingExit;

    // Helper: Get entity ID from body
    int GetEntityID(const JPH::Body& body) const {
        auto it = bodyToEntityMap.find(body.GetID());
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    int GetEntityID(const JPH::BodyID& bodyID) const {
        auto it = bodyToEntityMap.find(bodyID);
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    // Get motion type name as string
    const char* GetMotionTypeName(const JPH::Body& body) const {
        switch (body.GetMotionType()) {
        case JPH::EMotionType::Static:    return "Static";
        case JPH::EMotionType::Kinematic: return "Kinematic";
        case JPH::EMotionType::Dynamic:   return "Dynamic";
        default:                          return "Unknown";
        }
    }

    // Create unique collision key (order-independent)
    uint64_t MakeCollisionKey(int entityA, int entityB) const {
        if (entityA > entityB) std::swap(entityA, entityB);
        return (static_cast<uint64_t>(entityA) << 32) | static_cast<uint64_t>(entityB);
    }

    // Detailed logging
    void LogCollisionDetails(const JPH::Body& body1, const JPH::Body& body2,
        const JPH::ContactManifold& manifold) {
        ENGINE_PRINT("========= COLLISION DETAIL =========\n");
        ENGINE_PRINT("Entity ", GetEntityID(body1), " (", GetMotionTypeName(body1),
            ") <-> Entity ", GetEntityID(body2), " (", GetMotionTypeName(body2), ")\n");
        ENGINE_PRINT("Contact points: ", manifold.mRelativeContactPointsOn1.size(), "\n");

        ENGINE_PRINT("Normal: (", manifold.mWorldSpaceNormal.GetX(), ", ",
            manifold.mWorldSpaceNormal.GetY(), ", ", manifold.mWorldSpaceNormal.GetZ(), ")\n");
        ENGINE_PRINT("Penetration depth: ", manifold.mPenetrationDepth, "\n");

        LogAngularVelocity(body1);
        LogAngularVelocity(body2);

        ENGINE_PRINT("====================================\n");
    }

    void LogAngularVelocity(const JPH::Body& body) {
        JPH::Vec3 angVel = body.GetAngularVelocity();
        float speed = angVel.Length();

        if (speed > 0.5f) {
            ENGINE_PRINT("Entity ", GetEntityID(body), " (", GetMotionTypeName(body),
                ") angular speed: ", speed, " rad/s\n");
            ENGINE_PRINT("  Vector: (", angVel.GetX(), ", ",
                angVel.GetY(), ", ", angVel.GetZ(), ")\n");
        }
    }
};
