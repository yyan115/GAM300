#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <Logging.hpp>
#include <iostream>

//TODO: CHANGE STD:: TO ENGINE_PRINT

class MyContactListener : public JPH::ContactListener {
public:
    // Called when a contact point is being validated (before collision resolution)
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult) override
    {
        std::cout << "[Collision] Validating contact between bodies "
            << inBody1.GetID().GetIndex() << " and "
            << inBody2.GetID().GetIndex() << std::endl;

        // Return AcceptAllContactsForThisBodyPair to allow the collision
        // Return RejectAllContactsForThisBodyPair to reject the collision
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    // Replace your OnContactAdded function with this:
    virtual void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        // Only debug body 0 (your falling cube)
        if (inBody1.GetID().GetIndex() == 0 || inBody2.GetID().GetIndex() == 0) {
            std::cout << "========= COLLISION DETAIL =========" << std::endl;
            std::cout << "[Collision] Contact ADDED: Body "
                << inBody1.GetID().GetIndex() << " <-> Body "
                << inBody2.GetID().GetIndex() << std::endl;

            std::cout << "Base offset position: ("
                << inManifold.mBaseOffset.GetX() << ", "
                << inManifold.mBaseOffset.GetY() << ", "
                << inManifold.mBaseOffset.GetZ() << ")" << std::endl;

            std::cout << "Number of contact points: "
                << inManifold.mRelativeContactPointsOn1.size() << std::endl;

            // Print ALL individual contact points
            for (int i = 0; i < inManifold.mRelativeContactPointsOn1.size(); i++) {
                JPH::Vec3 worldPoint1 = inManifold.GetWorldSpaceContactPointOn1(i);
                JPH::Vec3 worldPoint2 = inManifold.GetWorldSpaceContactPointOn2(i);
                std::cout << "  Contact Point " << i << ":" << std::endl;
                std::cout << "    Body1 (cube) world pos: ("
                    << worldPoint1.GetX() << ", "
                    << worldPoint1.GetY() << ", "
                    << worldPoint1.GetZ() << ")" << std::endl;
                std::cout << "    Body2 (floor) world pos: ("
                    << worldPoint2.GetX() << ", "
                    << worldPoint2.GetY() << ", "
                    << worldPoint2.GetZ() << ")" << std::endl;
            }

            std::cout << "Contact normal: ("
                << inManifold.mWorldSpaceNormal.GetX() << ", "
                << inManifold.mWorldSpaceNormal.GetY() << ", "
                << inManifold.mWorldSpaceNormal.GetZ() << ")" << std::endl;

            std::cout << "Penetration depth: " << inManifold.mPenetrationDepth << std::endl;

            // Get body centers for reference
            JPH::RVec3 center1 = inBody1.GetCenterOfMassPosition();
            JPH::RVec3 center2 = inBody2.GetCenterOfMassPosition();
            std::cout << "Body1 (cube) center: ("
                << center1.GetX() << ", " << center1.GetY() << ", " << center1.GetZ() << ")" << std::endl;
            std::cout << "Body2 (floor) center: ("
                << center2.GetX() << ", " << center2.GetY() << ", " << center2.GetZ() << ")" << std::endl;

            std::cout << "====================================" << std::endl;
        }
    }
    // Called when a contact point is persisting (each frame the bodies stay in contact)
    virtual void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        // Usually too spammy to log every frame
        // Uncomment if you need detailed debugging
        /*
        std::cout << "[Collision] Contact PERSISTED: Body "
                  << inBody1.GetID().GetIndex() << " <-> Body "
                  << inBody2.GetID().GetIndex() << std::endl;
        */
    }

    // Called when a contact point is removed
    virtual void OnContactRemoved(
        const JPH::SubShapeIDPair& inSubShapePair) override
    {
        std::cout << "[Collision] Contact REMOVED: Body "
            << inSubShapePair.GetBody1ID().GetIndex() << " <-> Body "
            << inSubShapePair.GetBody2ID().GetIndex() << std::endl;
    }
};

//// Optional: Body Activation Listener (useful for debugging sleeping/waking bodies)
//class MyBodyActivationListener : public JPH::BodyActivationListener {
//public:
//    virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {
//        std::cout << "[Physics] Body " << inBodyID.GetIndex() << " ACTIVATED" << std::endl;
//    }
//
//    virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {
//        std::cout << "[Physics] Body " << inBodyID.GetIndex()
//            << " DEACTIVATED (went to sleep)" << std::endl;
//    }
//};