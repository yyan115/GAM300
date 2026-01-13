#pragma once
#include "pch.h"
#include "Physics/JoltInclude.hpp"
#include "ECS/System.hpp"
#include "Math/Vector3D.hpp"


struct ColliderComponent;
struct Transform;
class ECSManager;


// Pure runtime class that handles movement logic
class CharacterController : public System
{
public:
    // Constructor: Needs a PhysicsSystem reference
    CharacterController(JPH::PhysicsSystem* physicsSystem);

    // Destructor
    ~CharacterController();

    bool Initialise(ColliderComponent& collider, Transform& transform);

    // Called each frame
    void Update(float deltaTime);

    // Expose raw character for internal use
    const JPH::CharacterVirtual* GetCharacterVirtual() const { return mCharacter; }


    CharacterController* CreateController(ColliderComponent& collider, Transform& transform);


    //GETTER FUNCTIONS
    Vector3D GetPosition() const;
    Vector3D GetVelocity() const;

    // Set/Get velocity
    void SetVelocity(const Vector3D vel);


    // BASIC MOVEMENT FUNCTIONS 
    void Move(float x, float y, float z);   // Set Move Velocity, Position updated in "Update", change name to clearer?
    void Jump(float height);                // Jump

    bool IsGrounded() const;

    Vector3D GetGravity() const;

    void SetGravity(Vector3D gravity);

    //TODO:
    //void SetRotation(float yaw);            // Set facing direction (degrees)
    //void GetRotation();            // Add to current rotation

    //void SetMass(float mass);               // Character mass
    //float GetMass() const;






private:
    JPH::PhysicsSystem* mPhysicsSystem = nullptr;
    JPH::CharacterVirtual* mCharacter = nullptr;
    JPH::ObjectLayer mCharacterLayer;

    // Cache velocity
    JPH::Vec3 mVelocity = JPH::Vec3::sZero();

    float collider_offsetY;
    bool jump_Requested = false;

};