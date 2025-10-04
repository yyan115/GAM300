#pragma once
#include <memory>
#include <rapidjson/document.h>

class ECSManager;
class AssetManager;
struct BasePrefabComponent;

#include "ECS/System.hpp"
#include "Engine.h"

using PrefabID = std::size_t;
using EntityID = std::size_t;
using ComponentID = uint8_t;
using AssetID = unsigned;

class Prefab {
public:
	Prefab(PrefabID id, AssetID assetId);

	template <typename ...Args>
	Prefab(PrefabID id, AssetID assetId, Args... args);

	// non-copyable (unique_ptr inside)
	Prefab(const Prefab&) = delete;
	Prefab& operator=(const Prefab&) = delete;

	// movable is fine
	Prefab(Prefab&&) noexcept = default;
	Prefab& operator=(Prefab&&) noexcept = default;

	// this modifies all entities that has this particular prefab
	void UpdateEntities(ECSManager& registry, std::vector<std::pair<EntityID, std::set<ComponentID>>> allEntities) const;

	std::pair<std::set<ComponentID>, std::set<ComponentID>> InstantiatePrefab(ECSManager& registry, EntityID id) const;

	rapidjson::Value Serialize(rapidjson::Document::AllocatorType& alloc) const;
	rapidjson::Document ToDocument() const;

#ifndef DISABLE_IMGUI_LEVELEDITOR
	void DisplayComponentUI(ECSManager& registry, AssetManager& assetManager);
#endif
	void CaptureOriginalPrefab();
	void RestoreOriginalPrefab();

	void RemoveComponent(ComponentID id);

	template <typename T>
	void RemoveComponent();

	template <typename T>
	void AddComponent(T const& component);

	template <typename T>
	T* GetComponent();

	PrefabID GetId() const;
	std::unordered_map<ComponentID, std::unique_ptr<BasePrefabComponent>> const& GetComponents() const;

	Prefab Clone() const;

	AssetID GetAssetId() const;

private:
	AssetID assetId;
	PrefabID id;
	std::unordered_map<ComponentID, std::unique_ptr<BasePrefabComponent>> components;
};