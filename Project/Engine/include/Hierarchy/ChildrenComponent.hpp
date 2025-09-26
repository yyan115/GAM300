#pragma once
#include <set>
#include "../ECS/Entity.hpp"

struct ChildrenComponent {
	std::set<Entity> children; // Children entity IDs
};