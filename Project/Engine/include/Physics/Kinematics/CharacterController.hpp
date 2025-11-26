#pragma once
#include "pch.h"
#include "Physics/JoltInclude.hpp"
#include "Math/Vector3D.hpp"


struct ColliderComponent;
struct CharacterControllerComponent;
struct Transform;


// Pure runtime class that handles movement logic
class CharacterController
{
public:
    // Constructor: Needs a PhysicsSystem reference
    CharacterController(JPH::PhysicsSystem* physicsSystem);

    // Destructor
    ~CharacterController();

    void Initialise(ColliderComponent &collider, Transform &transform);

    // Called each frame
    void Update(float deltaTime);

    // Expose raw character for internal use
    const JPH::CharacterVirtual* GetCharacterVirtual() const { return mCharacter; }


    //GETTER FUNCTIONS
    Vector3D GetPosition() const;
    Vector3D GetVelocity() const;

    // Set/Get velocity
    void SetVelocity(const Vector3D vel);


   // BASIC MOVEMENT FUNCTIONS 
    void Move(float x, float y, float z);   // Set Move Velocity, Position updated in "Update", change name to clearer?
    void Jump(float height);                // Jump



    //TODO:
    //void SetRotation(float yaw);            // Set facing direction (degrees)
    //void GetRotation();            // Add to current rotation

    //void SetGravity(float gravity);         // Adjust gravity (-9.81 default)
    //float GetGravity() const;

    //void SetMass(float mass);               // Character mass
    //float GetMass() const;






private:
    JPH::PhysicsSystem* mPhysicsSystem = nullptr;
    JPH::CharacterVirtual* mCharacter = nullptr;
    JPH::ObjectLayer mCharacterLayer;

    // Cache velocity
    JPH::Vec3 mVelocity = JPH::Vec3::sZero();
};
