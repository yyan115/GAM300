#pragma once
#include <rapidjson/document.h>
#include "Prefab.hpp"

template <typename T>
struct ShowPrefabComponentFunctor {
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

	virtual ~BasePrefabComponent() = default;

public:
#ifndef DISABLE_IMGUI_LEVELEDITOR
	virtual void DisplayComponentUI(ECSManager& registry, Prefab& prefab, AssetManager& assetManager) = 0;
#endif

	virtual std::pair<std::string, rapidjson::Value>
		SerializeComponent(rapidjson::Document::AllocatorType& alloc) const = 0;
	virtual void CaptureOriginalComponent() = 0;
	virtual void RestoreOriginalComponent() = 0;
	virtual void UpdateEntity(ECSManager& registry, EntityID id) = 0;
	virtual std::unique_ptr<BasePrefabComponent> Clone() const = 0;
public:
	virtual bool CreateEntityComponent(ECSManager& registry, EntityID id) = 0;
};

template <typename T>
struct PrefabComponent : BasePrefabComponent {
	PrefabComponent(T component) : component{ std::move(component) }, originalComponentCopy{} {}

	bool CreateEntityComponent(ECSManager& registry, EntityID id) final;

	void CaptureOriginalComponent() final;
	void RestoreOriginalComponent() final;
	void UpdateEntity(ECSManager& registry, EntityID id) final;
	std::unique_ptr<BasePrefabComponent> Clone() const final;

#ifndef DISABLE_IMGUI_LEVELEDITOR
	void DisplayComponentUI(ECSManager& registry, Prefab& prefab, AssetManager& assetManager) final;
#endif

	T component;
	T originalComponentCopy;

#ifndef DISABLE_IMGUI_LEVELEDITOR
	ShowPrefabComponentFunctor<T> functor;
#endif 
};

template<typename T>
void PrefabComponent<T>::CaptureOriginalComponent() {
	originalComponentCopy = component;
}

template<typename T>
void PrefabComponent<T>::RestoreOriginalComponent() {
	component = originalComponentCopy;
}

template<typename T>
std::unique_ptr<BasePrefabComponent> PrefabComponent<T>::Clone() const {
	return std::make_unique<PrefabComponent<T>>(component);
}

#ifndef DISABLE_IMGUI_LEVELEDITOR
template<typename T>
void PrefabComponent<T>::DisplayComponentUI(ECSManager& registry, Prefab& prefab, AssetManager& assetManager) {
	return functor(registry, prefab, assetManager, component);
}
#endif