#include "pch.h"
#include <rapidjson/document.h>
#include "Prefab.hpp"
#include "PrefabComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Reflection/ReflectionBase.hpp"

Prefab::Prefab(PrefabID id_, AssetID assetId_)
	: assetId{ assetId_ }, id{ id_ }, components{} {
}

std::pair<std::set<ComponentID>, std::set<ComponentID>> Prefab::InstantiatePrefab(ECSManager& registry, EntityID entityId) const {
	std::set<ComponentID> affectedComponentIds;
	std::set<ComponentID> overridenComponentIds;

	for (auto& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		if (basePrefabComponent->CreateEntityComponent(registry, entityId)) {
			affectedComponentIds.insert(componentId);
		}
		else {
			overridenComponentIds.insert(componentId);
		}
	}

	return { affectedComponentIds, overridenComponentIds };
}

void Prefab::RemoveComponent(ComponentID componentId) {
	auto it = components.find(componentId);

	if (it == components.end()) {
		return;
	}

	components.erase(it);
}

PrefabID Prefab::GetId() const {
	return id;
}

std::unordered_map<ComponentID, std::unique_ptr<BasePrefabComponent>> const& Prefab::GetComponents() const {
	return components;
}

Prefab Prefab::Clone() const {
	Prefab clonedPrefab = Prefab{ id, assetId };

	for (auto const& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		clonedPrefab.components.insert({ componentId, basePrefabComponent->Clone() });
	}

	return clonedPrefab;
}

AssetID Prefab::GetAssetId() const {
	return assetId;
}

rapidjson::Value Prefab::Serialize(rapidjson::Document::AllocatorType& alloc) const {
	rapidjson::Value obj(rapidjson::kObjectType);

	for (auto it = components.begin(); it != components.end(); ) {
		auto currentIt = it++;
		const auto& uptr = currentIt->second;
		assert(uptr);

		auto [typeName, compJson] = uptr->SerializeComponent(alloc);

		// key must live in the same allocator
		rapidjson::Value key(typeName.c_str(), static_cast<rapidjson::SizeType>(typeName.size()), alloc);
		obj.AddMember(key, compJson, alloc);
	}

	return obj;
}

rapidjson::Document Prefab::ToDocument() const {
	rapidjson::Document doc;
	doc.SetObject();
	auto& alloc = doc.GetAllocator();

	rapidjson::Value root = Serialize(alloc);
	doc.Swap(root);  // make the document itself the prefab object

	return doc;
}

#ifndef DISABLE_IMGUI_LEVELEDITOR
void Prefab::DisplayComponentUI(ECSManager& registry, AssetManager& assetManager) {
	for (auto it = components.begin(); it != components.end();) {
		auto currentIt = it++;
		assert(currentIt->second);
		currentIt->second->DisplayComponentUI(registry, *this, assetManager);
	}
}
#endif

void Prefab::CaptureOriginalPrefab() {
	for (auto& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		basePrefabComponent->CaptureOriginalComponent();
	}
}

void Prefab::RestoreOriginalPrefab() {
	for (auto& [componentId, basePrefabComponent] : components) {
		assert(basePrefabComponent);

		basePrefabComponent->RestoreOriginalComponent();
	}
}

void Prefab::UpdateEntities(ECSManager& registry, std::vector<std::pair<EntityID, std::set<ComponentID>>> allEntities) const {
	for (auto const& [entityId, componentIds] : allEntities) {
		for (auto const& componentId : componentIds) {
			auto it = components.find(componentId);
			assert(it != components.end());
			assert(it->second);
			it->second->UpdateEntity(registry, entityId);
		}
	}
}

