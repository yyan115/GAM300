#include "pch.h"
#include "Prefab.hpp"
#include "PrefabComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include "Asset Manager/AssetManager.hpp"

Prefab::Prefab(PrefabID id_, AssetID assetId_)
	: assetId{ assetId_ }, id{ id_ }, components{} {
}

std::pair<std::set<ComponentID>, std::set<ComponentID>> Prefab::instantiatePrefab(ECSManager& registry, EntityID entityId) const {
	std::set<ComponentID> affectedComponentIds;
	std::set<ComponentID> overridenComponentIds;

	for (auto& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		if (basePrefabComponent->createEntityComponent(registry, entityId)) {
			affectedComponentIds.insert(componentId);
		}
		else {
			overridenComponentIds.insert(componentId);
		}
	}

	return { affectedComponentIds, overridenComponentIds };
}

void Prefab::removeComponent(ComponentID componentId) {
	auto it = components.find(componentId);

	if (it == components.end()) {
		return;
	}

	components.erase(it);
}

PrefabID Prefab::getId() const {
	return id;
}

std::unordered_map<ComponentID, std::unique_ptr<BasePrefabComponent>> const& Prefab::getComponents() const {
	return components;
}

Prefab Prefab::clone() const {
	Prefab clonedPrefab = Prefab{ id, assetId };

	for (auto const& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		clonedPrefab.components.insert({ componentId, basePrefabComponent->clone() });
	}

	return clonedPrefab;
}

AssetID Prefab::getAssetId() const {
	return assetId;
}

#if 0
Json::Value Prefab::serialize() {
	Json::Value jsonObj;
	for (auto it = components.begin(); it != components.end(); ) {
		auto currentIt = it++;
		assert(currentIt->second);
		auto [type, componentJson] = currentIt->second->serializeComponent();
		jsonObj[type] = componentJson;
	}
	return jsonObj;
}
#endif

#ifndef DISABLE_IMGUI_LEVELEDITOR
void Prefab::displayComponentUI(ECSManager& registry, AssetManager& assetManager) {
	for (auto it = components.begin(); it != components.end();) {
		auto currentIt = it++;
		assert(currentIt->second);
		currentIt->second->displayComponentUI(registry, *this, assetManager);
	}
}
#endif

void Prefab::captureOriginalPrefab() {
	for (auto& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		basePrefabComponent->captureOriginalComponent();
	}
}

void Prefab::restoreOriginalPrefab() {
	for (auto& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		basePrefabComponent->restoreOriginalComponent();
	}
}

void Prefab::updateEntities(ECSManager& registry, std::vector<std::pair<EntityID, std::set<ComponentID>>> allEntities) const {
	for (auto const& [entityId, componentIds] : allEntities) {
		for (auto const& componentId : componentIds) {
			auto it = components.find(componentId);
			assert(it != components.end());
			assert(it->second);
			it->second->updateEntity(registry, entityId);
		}
	}
}

