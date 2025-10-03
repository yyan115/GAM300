#pragma once
#include <set>
#include "../ECS/Entity.hpp"

struct ChildrenComponent {
	REFL_SERIALIZABLE
	std::vector<Entity> children; // Children entity IDs
};