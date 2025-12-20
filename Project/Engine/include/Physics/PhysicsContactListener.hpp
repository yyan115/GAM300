/*********************************************************************************
* @File			PhysicsContactListener.hpp
* @Author		Ang Jia Jun Austin, a.jiajunaustin@digipen.edu
* @Co-Author	-
* @Date			9/11/2025
* @Brief		Contact listener implementation for Jolt Physics collision callbacks.
*				Handles collision validation and events, mapping physics bodies
*				to game entities for event processing.
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <iomanip>

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
        , enableLogging(true)
        , enableDetailedLogging(false)
    {}

    // Register callbacks for collision events
    void SetOnCollisionEnter(CollisionCallback callback) { onCollisionEnter = callback; }
    void SetOnCollisionExit(CollisionCallback callback) { onCollisionExit = callback; }

    // Toggle logging
    void EnableLogging(bool enable) { enableLogging = enable; }
    void EnableDetailedLogging(bool enable) { enableDetailedLogging = enable; }

    // Check if two entities are currently colliding
    bool AreEntitiesColliding(int entityA, int entityB) const {
        uint64_t key = MakeCollisionKey(entityA, entityB);
        return activeCollisions.find(key) != activeCollisions.end();
    }

    // Called when a contact point is being validated
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg /*inBaseOffset*/,
        const JPH::CollideShapeResult& /*inCollisionResult*/) override
    {
        if (enableDetailedLogging) {
            std::cout << "[Collision] Validating contact between entities "
                << GetEntityID(inBody1) << " and " << GetEntityID(inBody2) << std::endl;
        }

        // You can add custom validation logic here
        // For example, ignore collisions between certain entity types

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

        // Only trigger callback if this is a new collision
        if (activeCollisions.insert(key).second) {
            if (enableLogging) {
                std::cout << "[Collision] Enter: Entity " << entityA
                    << " <-> Entity " << entityB << std::endl;
            }

            if (onCollisionEnter && inManifold.mRelativeContactPointsOn1.size() > 0) {
                CollisionEvent event;
                event.entityA = entityA;
                event.entityB = entityB;
                event.contactPoint = inManifold.GetWorldSpaceContactPointOn1(0);
                event.contactNormal = inManifold.mWorldSpaceNormal;
                event.penetrationDepth = inManifold.mPenetrationDepth;

                onCollisionEnter(event);
            }

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

        if (activeCollisions.erase(key) > 0) {
            if (enableLogging) {
                std::cout << "[Collision] Exit: Entity " << entityA
                    << " <-> Entity " << entityB << std::endl;
            }

            if (onCollisionExit) {
                CollisionEvent event;
                event.entityA = entityA;
                event.entityB = entityB;

                onCollisionExit(event);
            }
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

    // Helper: Get entity ID from body
    int GetEntityID(const JPH::Body& body) const {
        auto it = bodyToEntityMap.find(body.GetID());
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    int GetEntityID(const JPH::BodyID& bodyID) const {
        auto it = bodyToEntityMap.find(bodyID);
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    // Create unique collision key (order-independent)
    uint64_t MakeCollisionKey(int entityA, int entityB) const {
        if (entityA > entityB) std::swap(entityA, entityB);
        return (static_cast<uint64_t>(entityA) << 32) | static_cast<uint64_t>(entityB);
    }

    // Detailed logging
    void LogCollisionDetails(const JPH::Body& body1, const JPH::Body& body2,
        const JPH::ContactManifold& manifold) {
        std::cout << "========= COLLISION DETAIL =========" << std::endl;
        std::cout << "Entity " << GetEntityID(body1) << " <-> Entity " << GetEntityID(body2) << std::endl;
        std::cout << "Contact points: " << manifold.mRelativeContactPointsOn1.size() << std::endl;

        for (size_t i = 0; i < manifold.mRelativeContactPointsOn1.size(); ++i) {
            JPH::Vec3 p1 = manifold.GetWorldSpaceContactPointOn1(i);
            JPH::Vec3 p2 = manifold.GetWorldSpaceContactPointOn2(i);
            std::cout << "  Point " << i << ": ("
                << std::fixed << std::setprecision(2)
                << p1.GetX() << ", " << p1.GetY() << ", " << p1.GetZ() << ") <-> ("
                << p2.GetX() << ", " << p2.GetY() << ", " << p2.GetZ() << ")" << std::endl;
        }

        JPH::Vec3 normal = manifold.mWorldSpaceNormal;
        std::cout << "Normal: (" << std::fixed << std::setprecision(2)
            << normal.GetX() << ", " << normal.GetY() << ", " << normal.GetZ() << ")" << std::endl;
        std::cout << "Penetration depth: " << std::fixed << std::setprecision(4)
            << manifold.mPenetrationDepth << std::endl;

        LogAngularVelocity(body1);
        LogAngularVelocity(body2);

        std::cout << "====================================" << std::endl;
    }

    void LogAngularVelocity(const JPH::Body& body) {
        JPH::Vec3 angVel = body.GetAngularVelocity();
        float speed = angVel.Length();

        if (speed > 0.5f) {
            std::cout << "Entity " << GetEntityID(body) << " angular speed: "
                << std::fixed << std::setprecision(2) << speed << " rad/s" << std::endl;
            std::cout << "  Vector: (" << angVel.GetX() << ", "
                << angVel.GetY() << ", " << angVel.GetZ() << ")" << std::endl;
        }
    }
};