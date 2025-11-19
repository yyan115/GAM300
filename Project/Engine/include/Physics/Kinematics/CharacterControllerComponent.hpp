#pragma once
#include "Reflection/ReflectionBase.hpp"

struct CharacterControllerComponent
{
    REFL_SERIALIZABLE
public:
    bool enabled = true;
    float speed = 5.0f;
    float jumpHeight = 2.0f;

    // Optionally, store a pointer to the runtime controller for the entity
    // Not serialized, just runtime use
    class CharacterController* runtimeController = nullptr;
};