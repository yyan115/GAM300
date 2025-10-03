#pragma once
#include <set>
#include "../ECS/Entity.hpp"

struct ChildrenComponent {
	REFL_SERIALIZABLE
	std::vector<GUID_128> children; // Children entity IDs
};