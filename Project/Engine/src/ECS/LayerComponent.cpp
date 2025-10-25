#include "pch.h"
#include "ECS/LayerComponent.hpp"
#include "ECS/LayerManager.hpp"
#include "Reflection/ReflectionBase.hpp"

#pragma region Reflection
REFL_REGISTER_START(LayerComponent)
	REFL_REGISTER_PROPERTY(overrideFromPrefab)
	REFL_REGISTER_PROPERTY(layerIndex)
REFL_REGISTER_END
#pragma endregion

void LayerComponent::SetLayer(const std::string& layerName) {
	int index = LayerManager::GetInstance().GetLayerIndex(layerName);
	if (index != -1) {
		layerIndex = index;
	}
}

const std::string& LayerComponent::GetLayerName() const {
	return LayerManager::GetInstance().GetLayerName(layerIndex);
}

bool LayerComponent::IsInLayer(const std::string& layerName) const {
	return GetLayerName() == layerName;
}