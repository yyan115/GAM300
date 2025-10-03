#include "pch.h"
#include "Hierarchy/EntityGUIDRegistry.hpp"

EntityGUIDRegistry& EntityGUIDRegistry::GetInstance() {
	static EntityGUIDRegistry instance;
	return instance;
}