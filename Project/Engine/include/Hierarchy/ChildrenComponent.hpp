#pragma once
#include <cstdint>
#include <limits>
#include <vector>
#include "../ECS/Entity.hpp"
#include "EntityGUIDRegistry.hpp"

struct ChildrenComponent {
	REFL_SERIALIZABLE
	std::vector<GUID_128> children; // Children entity IDs

	const std::vector<Entity>& ResolveEntities() const {
		auto& registry = EntityGUIDRegistry::GetInstance();
		const std::uint64_t registryRevision = registry.GetRevision();
		if (resolvedRegistryRevision == registryRevision &&
			resolvedGUIDs == children) {
			return resolvedEntities;
		}

		resolvedGUIDs = children;
		resolvedEntities.clear();
		resolvedEntities.reserve(children.size());
		for (const GUID_128& childGUID : children) {
			resolvedEntities.push_back(registry.GetEntityByGUID(childGUID));
		}
		resolvedRegistryRevision = registryRevision;
		return resolvedEntities;
	}

private:
	// Runtime-only mirror. Serialized GUIDs remain authoritative, while hot
	// hierarchy walks use direct entity IDs until either side changes.
	mutable std::vector<GUID_128> resolvedGUIDs;
	mutable std::vector<Entity> resolvedEntities;
	mutable std::uint64_t resolvedRegistryRevision =
		std::numeric_limits<std::uint64_t>::max();
};
