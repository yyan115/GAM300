#pragma once
#include <memory>

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

	// this modifies all entities that has this particular prefab
	void updateEntities(ECSManager& registry, std::vector<std::pair<EntityID, std::set<ComponentID>>> allEntities) const;

	// instantiate a copy of the prefab components to entity id.
	// if entity already has a particular component, it is not instantiated.
	// the first set of component ids holds the component ids that is successfully instantied.
	// the second set of component ids holds the component ids that are not successfully instantied, and thus overriden. 
	// (because entity already has that component)
	std::pair<std::set<ComponentID>, std::set<ComponentID>> instantiatePrefab(ECSManager& registry, EntityID id) const;

	//Json::Value serialize();

#ifndef DISABLE_IMGUI_LEVELEDITOR
	void displayComponentUI(ECSManager& registry, AssetManager& assetManager);
#endif
	void captureOriginalPrefab();
	void restoreOriginalPrefab();

	// Use the ECS interface if you want to add or remove component from prefab! That way you can update all existing entities.
	void removeComponent(ComponentID id);

	template <typename T>
	void removeComponent();

	template <typename T>
	void addComponent(T const& component);

	template <typename T>
	T* getComponent();

	PrefabID getId() const;
	std::unordered_map<ComponentID, std::unique_ptr<BasePrefabComponent>> const& getComponents() const;

	Prefab clone() const;

	AssetID getAssetId() const;

private:
	AssetID assetId;
	PrefabID id;
	std::unordered_map<ComponentID, std::unique_ptr<BasePrefabComponent>> components;
};

#if 0
template<typename ...Args>
Prefab::Prefab(PrefabID id, AssetID assetId, Args... args) :
	id{ id },
	assetId{ assetId },
	components{}
{
	(components.insert({ Family::getID<Args>(), std::make_unique<PrefabComponent<Args>>(PrefabComponent<Args>{args}) }), ...);
}

template<typename T>
void Prefab::removeComponent() {
	removeComponent(Family::getID<T>());
}

template <typename T>
void Prefab::addComponent(T const& component) {
	components.insert({ Family::getID<T>(), std::make_unique<PrefabComponent<T>>(PrefabComponent<T>{component}) });
}

template<typename T>
T* Prefab::getComponent() {
	auto it = components.find(Family::getID<T>());

	if (it == components.end()) {
		return nullptr;
	}
	else {
		auto& [componentId, basePrefabComponent] = *it;
		assert(basePrefabComponent);
		PrefabComponent<T>& prefabComponent = dynamic_cast<PrefabComponent<T>&>(*basePrefabComponent);
		return &prefabComponent.component;
	}
}
#endif