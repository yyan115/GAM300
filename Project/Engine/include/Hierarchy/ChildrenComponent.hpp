#pragma once
#include <vector>
#include "../ECS/Entity.hpp"

struct ChildrenComponent {
	std::vector<Entity> children; // Children entity IDs
};