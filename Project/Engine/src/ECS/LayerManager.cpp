/* Start Header ************************************************************************/
/*!
\file       LayerManager.cpp
\author     Muhammad Zikry
\date       2025
\brief      Manages layers within the ECS system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "pch.h"
#include "ECS/LayerManager.hpp"

LayerManager& LayerManager::GetInstance() {
    static LayerManager instance;
    return instance;
}

LayerManager::LayerManager() {
    InitializeDefaultLayers();
}

bool LayerManager::SetLayerName(int index, const std::string& name) {
    if (index < 0 || index >= MAX_LAYERS) return false;

    // Remove old name from mapping if it exists
    if (!layerNames[index].empty()) {
        nameToIndex.erase(layerNames[index]);
    }

    layerNames[index] = name;
    if (!name.empty()) {
        nameToIndex[name] = index;
    }

    return true;
}

int LayerManager::AddLayer(const std::string& name) {
    // Check if layer already exists
    if (GetLayerIndex(name) != -1) {
        return -1; // Already exists
    }

    // Find first empty slot (starting from index 1, since 0 is Default)
    for (int i = 1; i < MAX_LAYERS; ++i) {
        if (layerNames[i].empty()) {
            SetLayerName(i, name);
            return i;
        }
    }

    return -1; // No empty slots
}

bool LayerManager::RemoveLayer(int index) {
    if (index <= 0 || index >= MAX_LAYERS) return false; // Can't remove Default layer

    return SetLayerName(index, ""); // Clear the name
}

const std::string& LayerManager::GetLayerName(int index) const {
    if (index >= 0 && index < MAX_LAYERS) {
        return layerNames[index];
    }
    static const std::string emptyString = "";
    return emptyString;
}

int LayerManager::GetLayerIndex(const std::string& name) const {
    auto it = nameToIndex.find(name);
    return (it != nameToIndex.end()) ? it->second : -1;
}

void LayerManager::InitializeDefaultLayers() {
    // default layers
    SetLayerName(0, "Default");
    SetLayerName(1, "TransparentFX");
    SetLayerName(2, "Ignore Raycast");
    SetLayerName(3, "Water");
    SetLayerName(4, "UI");
    SetLayerName(5, "Player");
    SetLayerName(6, "Enemy");
    SetLayerName(7, "Ground");
    SetLayerName(8, "Obstacle");
    SetLayerName(9, "Collectible");

    // Leave the rest as empty strings (unnamed layers)
    for (int i = 10; i < MAX_LAYERS; ++i) {
        layerNames[i] = "";
    }
}