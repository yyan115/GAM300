#pragma once
#include "Prefab.hpp"

template <typename T>
struct showPrefabComponentFunctor {
	void operator()(ECSManager& registry, Prefab& prefab, AssetManager& assetManager, T& component);

	ComponentID componentToBeDeleted = std::numeric_limits<std::size_t>::max();
};

#if 0
template <typename T>
struct serializePrefabComponentFunctor {
	std::pair<std::string, Json::Value> operator()(T& component);
};
#endif

struct BasePrefabComponent {
	BasePrefabComponent() = default;

	BasePrefabComponent(BasePrefabComponent const&) = delete;
	BasePrefabComponent& operator=(BasePrefabComponent const&) = delete;
	BasePrefabComponent(BasePrefabComponent&&) = default;
	BasePrefabComponent& operator=(BasePrefabComponent&&) = default;

	virtual ~BasePrefabComponent() = 0 {};

public:
#ifndef DISABLE_IMGUI_LEVELEDITOR
	virtual void displayComponentUI(ECSManager& registry, Prefab& prefab, AssetManager& assetManager) = 0;
#endif

	//virtual std::pair<std::string, Json::Value> serializeComponent() = 0;
	virtual void captureOriginalComponent() = 0;
	virtual void restoreOriginalComponent() = 0;
	virtual void updateEntity(ECSManager& registry, EntityID id) = 0;
	virtual std::unique_ptr<BasePrefabComponent> clone() const = 0;
public:
	virtual bool createEntityComponent(ECSManager& registry, EntityID id) = 0;
};

template <typename T>
struct PrefabComponent : BasePrefabComponent {
	PrefabComponent(T component) : component{ std::move(component) }, originalComponentCopy{} {}

	bool createEntityComponent(ECSManager& registry, EntityID id) final;

	void captureOriginalComponent() final;
	void restoreOriginalComponent() final;
	void updateEntity(ECSManager& registry, EntityID id) final;
	std::unique_ptr<BasePrefabComponent> clone() const final;

#ifndef DISABLE_IMGUI_LEVELEDITOR
	void displayComponentUI(ECSManager& registry, Prefab& prefab, AssetManager& assetManager) final;
#endif

	//std::pair<std::string, Json::Value> serializeComponent() final;

	T component;
	T originalComponentCopy;

#ifndef DISABLE_IMGUI_LEVELEDITOR
	showPrefabComponentFunctor<T> functor;
#endif 

	//serializePrefabComponentFunctor<T> serializer;
};

#if 0
template<typename T>
bool PrefabComponent<T>::createEntityComponent(ECSManager& registry, EntityID id) {
	if (registry.hasComponent<T>(id)) {
		return false;
	}

	registry.addComponent<T>(id, component);
	return true;
}
#endif

template<typename T>
void PrefabComponent<T>::captureOriginalComponent() {
	originalComponentCopy = component;
}

template<typename T>
void PrefabComponent<T>::restoreOriginalComponent() {
	component = originalComponentCopy;
}

#if 0
template<typename T>
void PrefabComponent<T>::updateEntity(ECSManager& registry, EntityID id) {
	if (registry.hasComponent<T>(id)) {
		registry.unsafeGetComponent<T>(id) = component;
	}
}
#endif

template<typename T>
std::unique_ptr<BasePrefabComponent> PrefabComponent<T>::clone() const {
	return std::make_unique<PrefabComponent<T>>(component);
}

#ifndef DISABLE_IMGUI_LEVELEDITOR
template<typename T>
void PrefabComponent<T>::displayComponentUI(ECSManager& registry, Prefab& prefab, AssetManager& assetManager) {
	return functor(registry, prefab, assetManager, component);
}
#endif

#if 0
template <typename T>
std::pair<std::string, Json::Value> PrefabComponent<T>::serializeComponent() {
	return serializer(component);
}
#endif