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

    // Called when a new contact point is added
    virtual void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        std::cout << "[Collision] Contact ADDED: Body "
            << inBody1.GetID().GetIndex() << " <-> Body "
            << inBody2.GetID().GetIndex()
            << " at position ("
            << inManifold.mBaseOffset.GetX() << ", "
            << inManifold.mBaseOffset.GetY() << ", "
            << inManifold.mBaseOffset.GetZ() << ")" << std::endl;
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