#pragma once
#include "../ECS/Entity.hpp"

struct ParentComponent {
REFL_SERIALIZABLE
	GUID_128 parent;
};