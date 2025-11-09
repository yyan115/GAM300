/*********************************************************************************
* @File			PhysicsContactListener.hpp
* @Author		Ang Jia Jun Austin, a.jiajunaustin@digipen.edu
* @Co-Author	-
* @Date			9/11/2025
* @Brief		Contact listener implementation for Jolt Physics collision callbacks.
*				Handles collision validation and removal events, mapping physics
*				bodies to game entities for event processing.
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <Logging.hpp>
#include <iostream>
#include <unordered_map>

class MyContactListener : public JPH::ContactListener {
public:
    // Constructor — take reference to the existing body→entity map
    MyContactListener(const std::unordered_map<JPH::BodyID, int>& idMap)
        : bodyToEntityMap(idMap) {}

    // Called when a contact point is being validated
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg /*inBaseOffset*/,
        const JPH::CollideShapeResult& /*inCollisionResult*/) override
    {
        std::cout << "[Collision] Validating contact between entities "
            << GetEntityID(inBody1) << " and "
            << GetEntityID(inBody2) << std::endl;

        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    //// Called when a contact point is added
    //virtual void OnContactAdded(
    //    const JPH::Body& inBody1,
    //    const JPH::Body& inBody2,
    //    const JPH::ContactManifold& inManifold,
    //    JPH::ContactSettings& ioSettings) override
    //{
    //    std::cout << "========= COLLISION DETAIL =========" << std::endl;
    //    std::cout << "Contact ADDED: Entity " << GetEntityID(inBody1)
    //        << " <-> Entity " << GetEntityID(inBody2) << std::endl;

    //    std::cout << "Number of contact points: " << inManifold.mRelativeContactPointsOn1.size() << std::endl;
    //    for (int i = 0; i < inManifold.mRelativeContactPointsOn1.size(); i++) {
    //        JPH::Vec3 p1 = inManifold.GetWorldSpaceContactPointOn1(i);
    //        JPH::Vec3 p2 = inManifold.GetWorldSpaceContactPointOn2(i);
    //        std::cout << "  Point " << i << ": Entity1(" << p1.GetX() << "," << p1.GetY() << "," << p1.GetZ()
    //            << ") Entity2(" << p2.GetX() << "," << p2.GetY() << "," << p2.GetZ() << ")" << std::endl;
    //    }

    //    // Angular velocity logging
    //    PrintAngularVelocity(inBody1);
    //    PrintAngularVelocity(inBody2);

    //    std::cout << "====================================" << std::endl;
    //}

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
    {
        std::cout << "[Collision] Contact REMOVED: Entity "
            << GetEntityID(inSubShapePair.GetBody1ID()) << " <-> Entity "
            << GetEntityID(inSubShapePair.GetBody2ID()) << std::endl;
    }

private:
    const std::unordered_map<JPH::BodyID, int>& bodyToEntityMap;

    int GetEntityID(const JPH::Body& body) const {
        auto it = bodyToEntityMap.find(body.GetID());
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    int GetEntityID(const JPH::BodyID& bodyID) const {
        auto it = bodyToEntityMap.find(bodyID);
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    //void PrintAngularVelocity(const JPH::Body& body)
    //{
    //    JPH::Vec3 angVel = body.GetAngularVelocity();
    //    float speed = angVel.Length();
    //    if (speed > 0.5f) {
    //        std::cout << "Entity " << GetEntityID(body) << " angular speed: "
    //            << speed << " rad/s" << std::endl;
    //        std::cout << "Angular velocity vector: ("
    //            << angVel.GetX() << ", " << angVel.GetY() << ", " << angVel.GetZ() << ")"
    //            << std::endl;
    //    }
    //}
};
