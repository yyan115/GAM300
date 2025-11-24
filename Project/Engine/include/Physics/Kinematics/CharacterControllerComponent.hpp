#pragma once
#include "Reflection/ReflectionBase.hpp"
#include "Physics/Kinematics/CharacterController.hpp"

struct CharacterControllerComponent
{
    REFL_SERIALIZABLE
public:
    bool enabled = true;
    float speed = 5.0f;
    float jumpHeight = 2.0f;

    //// Optionally, store a pointer to the runtime controller for the entity
    //// Not serialized, just runtime use
    class CharacterController* runtimeController = nullptr;

    void Move(float x, float y, float z) { if (runtimeController) runtimeController->Move(x, y, z); }
    void Jump() { if (runtimeController) runtimeController->Jump(jumpHeight); }
    //JPH::Vec3 GetPosition() const { return runtimeController ? runtimeController->GetPosition() : JPH::Vec3::sZero(); }


};