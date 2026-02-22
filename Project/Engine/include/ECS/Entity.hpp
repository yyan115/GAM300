#pragma once

#include <stdint.h>

#include "Reflection/ReflectionBase.hpp"

using Entity = uint32_t;
const Entity MAX_ENTITIES = 5000;
const Entity INVALID_ENTITY = static_cast<Entity>(-1);