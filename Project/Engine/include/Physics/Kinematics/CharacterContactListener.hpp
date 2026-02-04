#pragma once

/********************************************************************************
 * @File CharacterContactListener.hpp
 * @Author Ang Jia Jun Austin, a.jiajunaustin@digipen.edu
 * @Co-Author -
 * @Date 13/12/2025
 * @Brief Character contact listener implementation for Jolt Physics character
 *        collision callbacks. Handles character-specific collision events,
 *        ground detection, and surface interaction tracking.
 *
 * Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
 * of this file or its contents without the prior written consent of DigiPen
 * Institute of Technology is prohibited.
 *********************************************************************************/
#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <iomanip>
#include "Logging.hpp"

 // Character collision event data structure
struct CharacterCollisionEvent {
    int characterEntity;
    int otherEntity;
    JPH::Vec3 contactPosition;
    JPH::Vec3 contactNormal;
    JPH::Vec3 contactVelocity;
    float penetrationDepth;
    bool isGroundContact;
    bool isSteepSlope;
};

class CharacterContactListener : public JPH::CharacterContactListener {
public:
    using CharacterCollisionCallback = std::function<void(const CharacterCollisionEvent&)>;

    // Constructor
    CharacterContactListener(const std::unordered_map<JPH::BodyID, int>& idMap)
        : bodyToEntityMap(idMap)
        , characterEntityID(-1)
        , enableLogging(true)
        , enableDetailedLogging(false)
        , maxSlopeAngle(45.0f)
        , isGrounded(false)
        , groundBodyID(JPH::BodyID())
    {}

    // Set the entity ID for the character being tracked
    void SetCharacterEntity(int entityID) {
        characterEntityID = entityID;
    }

    // Register callbacks for character collision events
    void SetOnCharacterContactAdded(CharacterCollisionCallback callback) {
        onContactAdded = callback;
    }

    void SetOnCharacterContactRemoved(CharacterCollisionCallback callback) {
        onContactRemoved = callback;
    }

    void SetOnGroundContactAdded(CharacterCollisionCallback callback) {
        onGroundContactAdded = callback;
    }

    void SetOnGroundContactRemoved(CharacterCollisionCallback callback) {
        onGroundContactRemoved = callback;
    }

    // Toggle logging
    void EnableLogging(bool enable) { enableLogging = enable; }
    void EnableDetailedLogging(bool enable) { enableDetailedLogging = enable; }

    // Set maximum slope angle (in degrees) for ground detection
    void SetMaxSlopeAngle(float angleDegrees) {
        maxSlopeAngle = angleDegrees;
    }

    // Query grounded state
    bool IsGrounded() const { return isGrounded; }
    JPH::BodyID GetGroundBodyID() const { return groundBodyID; }
    JPH::Vec3 GetGroundNormal() const { return groundNormal; }
    JPH::Vec3 GetGroundVelocity() const { return groundVelocity; }

    // Check if character is touching a specific entity
    bool IsTouchingEntity(int entityID) const {
        return activeContacts.find(entityID) != activeContacts.end();
    }

    // Get all entities currently in contact
    std::vector<int> GetContactingEntities() const {
        return std::vector<int>(activeContacts.begin(), activeContacts.end());
    }

    // Called when a contact is being validated (before collision)
    virtual void OnContactValidate(
        const JPH::CharacterVirtual* inCharacter,
        const JPH::BodyID& inBodyID2,
        const JPH::SubShapeID& /*inSubShapeID2*/) override
    {
        if (enableDetailedLogging) {
            int otherEntity = GetEntityID(inBodyID2);
            ENGINE_PRINT("[Character] Validating contact with entity {}", otherEntity);
        }
    }

    // Called when a contact is added
    virtual void OnContactAdded(
        const JPH::CharacterVirtual* inCharacter,
        const JPH::BodyID& inBodyID2,
        const JPH::SubShapeID& /*inSubShapeID2*/,
        JPH::RVec3Arg inContactPosition,
        JPH::Vec3Arg inContactNormal,
        JPH::CharacterContactSettings& ioSettings) override
    {
        int otherEntity = GetEntityID(inBodyID2);
        if (otherEntity == -1) return;

        // Track active contact
        bool isNewContact = activeContacts.insert(otherEntity).second;

        // Determine if this is a ground contact
        float slopeAngle = CalculateSlopeAngle(inContactNormal);
        bool isValidGround = slopeAngle <= maxSlopeAngle;
        bool isSteep = slopeAngle > maxSlopeAngle;

        // Update ground state
        if (isValidGround && inContactNormal.GetY() > 0.7f) {
            if (!isGrounded) {
                isGrounded = true;
                groundBodyID = inBodyID2;
                groundNormal = inContactNormal;

                if (enableLogging) {
                    ENGINE_PRINT("[Character] Grounded on entity {} (slope: {:.1f})", otherEntity, slopeAngle);
                }
            }
        }

        // Log contact
        if (enableLogging && isNewContact) {
            std::string contactType = isValidGround ? " [GROUND]" : (isSteep ? " [STEEP SLOPE]" : "");
            ENGINE_PRINT("[Character] Contact added with entity {}{}", otherEntity, contactType);
        }

        // Trigger callbacks
        if (isNewContact && onContactAdded) {
            CharacterCollisionEvent event = CreateCollisionEvent(
                inBodyID2, inContactPosition, inContactNormal,
                ioSettings.mCanPushCharacter, isValidGround, isSteep);
            onContactAdded(event);
        }

        if (isValidGround && onGroundContactAdded) {
            CharacterCollisionEvent event = CreateCollisionEvent(
                inBodyID2, inContactPosition, inContactNormal,
                ioSettings.mCanPushCharacter, true, false);
            onGroundContactAdded(event);
        }

        if (enableDetailedLogging) {
            LogContactDetails(inBodyID2, inContactPosition, inContactNormal,
                slopeAngle, isValidGround, isSteep);
        }
    }

    // Called when contact is removed
    virtual void OnContactSolve(
        const JPH::CharacterVirtual* /*inCharacter*/,
        const JPH::BodyID& inBodyID2,
        const JPH::SubShapeID& /*inSubShapeID2*/,
        JPH::RVec3Arg inContactPosition,
        JPH::Vec3Arg inContactNormal,
        JPH::Vec3Arg inContactVelocity,
        const JPH::PhysicsMaterial* /*inContactMaterial*/,
        JPH::Vec3Arg inCharacterVelocity,
        JPH::Vec3& ioNewCharacterVelocity) override
    {
        // Update ground velocity if this is the ground body
        if (inBodyID2 == groundBodyID) {
            groundVelocity = inContactVelocity;
        }
    }

    // Clear all tracked contacts (useful for teleports or scene changes)
    void ClearContacts() {
        activeContacts.clear();
        isGrounded = false;
        groundBodyID = JPH::BodyID();
        groundNormal = JPH::Vec3::sZero();
        groundVelocity = JPH::Vec3::sZero();
    }

    // Manually set grounded state (useful for one-way platforms, etc.)
    void SetGrounded(bool grounded) {
        if (!grounded && isGrounded) {
            // Trigger ground exit callback
            if (onGroundContactRemoved) {
                CharacterCollisionEvent event;
                event.characterEntity = characterEntityID;
                event.otherEntity = GetEntityID(groundBodyID);
                onGroundContactRemoved(event);
            }

            if (enableLogging) {
                ENGINE_PRINT("[Character] Left ground (entity {})", GetEntityID(groundBodyID));
            }
        }

        isGrounded = grounded;
        if (!grounded) {
            groundBodyID = JPH::BodyID();
            groundNormal = JPH::Vec3::sZero();
            groundVelocity = JPH::Vec3::sZero();
        }
    }

private:
    const std::unordered_map<JPH::BodyID, int>& bodyToEntityMap;
    std::unordered_set<int> activeContacts;

    CharacterCollisionCallback onContactAdded;
    CharacterCollisionCallback onContactRemoved;
    CharacterCollisionCallback onGroundContactAdded;
    CharacterCollisionCallback onGroundContactRemoved;

    int characterEntityID;
    bool enableLogging;
    bool enableDetailedLogging;
    float maxSlopeAngle;

    // Ground state
    bool isGrounded;
    JPH::BodyID groundBodyID;
    JPH::Vec3 groundNormal;
    JPH::Vec3 groundVelocity;

    // Helper: Get entity ID from body ID
    int GetEntityID(const JPH::BodyID& bodyID) const {
        auto it = bodyToEntityMap.find(bodyID);
        return it != bodyToEntityMap.end() ? it->second : -1;
    }

    // Calculate slope angle from contact normal (in degrees)
    float CalculateSlopeAngle(JPH::Vec3Arg normal) const {
        float dotUp = normal.Dot(JPH::Vec3::sAxisY());
        dotUp = JPH::Clamp(dotUp, -1.0f, 1.0f);
        return JPH::RadiansToDegrees(JPH::ACos(dotUp));
    }

    // Create collision event
    CharacterCollisionEvent CreateCollisionEvent(
        const JPH::BodyID& bodyID,
        JPH::RVec3Arg position,
        JPH::Vec3Arg normal,
        float penetration,
        bool isGround,
        bool isSteep) const
    {
        CharacterCollisionEvent event;
        event.characterEntity = characterEntityID;
        event.otherEntity = GetEntityID(bodyID);
        event.contactPosition = position;
        event.contactNormal = normal;
        event.contactVelocity = JPH::Vec3::sZero();
        event.penetrationDepth = penetration;
        event.isGroundContact = isGround;
        event.isSteepSlope = isSteep;
        return event;
    }

    // Detailed logging
    void LogContactDetails(
        const JPH::BodyID& bodyID,
        JPH::RVec3Arg position,
        JPH::Vec3Arg normal,
        float slopeAngle,
        bool isGround,
        bool isSteep) const
    {
        ENGINE_PRINT("========= CHARACTER CONTACT DETAIL =========");
        ENGINE_PRINT("Character Entity: {}", characterEntityID);
        ENGINE_PRINT("Other Entity: {}", GetEntityID(bodyID));
        ENGINE_PRINT("Position: ({:.2f}, {:.2f}, {:.2f})", position.GetX(), position.GetY(), position.GetZ());
        ENGINE_PRINT("Normal: ({:.2f}, {:.2f}, {:.2f})", normal.GetX(), normal.GetY(), normal.GetZ());
        ENGINE_PRINT("Slope angle: {:.1f}", slopeAngle);
        std::string contactType = isGround ? "GROUND" : (isSteep ? "STEEP SLOPE" : "WALL");
        ENGINE_PRINT("Type: {}", contactType);
        ENGINE_PRINT("============================================");
    }
};