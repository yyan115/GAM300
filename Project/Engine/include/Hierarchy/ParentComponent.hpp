#pragma once
#include "../ECS/Entity.hpp"

struct ParentComponent {
REFL_SERIALIZABLE
	Entity parent;
};