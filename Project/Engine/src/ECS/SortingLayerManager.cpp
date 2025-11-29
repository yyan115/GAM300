#include "pch.h"
#include "ECS/SortingLayerManager.hpp"

SortingLayerManager& SortingLayerManager::GetInstance() {
    static SortingLayerManager instance;
    return instance;
}

SortingLayerManager::SortingLayerManager() {
    InitializeDefaultLayers();
}

bool SortingLayerManager::SetLayerName(int id, const std::string& name) {
    // Find layer with this ID
    for (auto& layer : sortingLayers) {
        if (layer.id == id) {
            // Remove old name from mapping if it exists
            if (!layer.name.empty()) {
                nameToID.erase(layer.name);
            }

            layer.name = name;
            if (!name.empty()) {
                nameToID[name] = id;
            }
            return true;
        }
    }

    return false;
}

int SortingLayerManager::AddLayer(const std::string& name) {
    // Check if layer already exists
    if (GetLayerID(name) != -1) {
        return -1; // Already exists
    }

    // Check if we've hit the max
    if (sortingLayers.size() >= MAX_SORTING_LAYERS) {
        return -1; // Too many layers
    }

    // Create new layer
    int newID = nextID++;
    int order = static_cast<int>(sortingLayers.size());

    SortingLayer newLayer;
    newLayer.id = newID;
    newLayer.name = name;
    newLayer.order = order;

    sortingLayers.push_back(newLayer);
    nameToID[name] = newID;
    idToOrderIndex[newID] = order;

    return newID;
}

bool SortingLayerManager::RemoveLayer(int id) {
    // Can't remove Default layer
    if (id == 0) return false;

    // Find and remove layer
    for (size_t i = 0; i < sortingLayers.size(); ++i) {
        if (sortingLayers[i].id == id) {
            // Remove from name mapping
            nameToID.erase(sortingLayers[i].name);
            idToOrderIndex.erase(id);

            // Remove from vector
            sortingLayers.erase(sortingLayers.begin() + i);

            // Update order values for remaining layers
            for (size_t j = i; j < sortingLayers.size(); ++j) {
                sortingLayers[j].order = static_cast<int>(j);
                idToOrderIndex[sortingLayers[j].id] = static_cast<int>(j);
            }

            return true;
        }
    }

    return false;
}

const std::string& SortingLayerManager::GetLayerName(int id) const {
    for (const auto& layer : sortingLayers) {
        if (layer.id == id) {
            return layer.name;
        }
    }
    static const std::string emptyString = "";
    return emptyString;
}

int SortingLayerManager::GetLayerID(const std::string& name) const {
    auto it = nameToID.find(name);
    return (it != nameToID.end()) ? it->second : -1;
}

int SortingLayerManager::GetLayerOrder(int id) const {
    auto it = idToOrderIndex.find(id);
    return (it != idToOrderIndex.end()) ? it->second : -1;
}

bool SortingLayerManager::MoveLayer(int id, int newOrder) {
    // Can't move Default layer
    if (id == 0) return false;

    // Find current position
    int currentIndex = -1;
    for (size_t i = 0; i < sortingLayers.size(); ++i) {
        if (sortingLayers[i].id == id) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    if (currentIndex == -1 || newOrder < 0 || newOrder >= static_cast<int>(sortingLayers.size())) {
        return false;
    }

    // Move the layer
    SortingLayer layer = sortingLayers[currentIndex];
    sortingLayers.erase(sortingLayers.begin() + currentIndex);
    sortingLayers.insert(sortingLayers.begin() + newOrder, layer);

    // Update order values
    for (size_t i = 0; i < sortingLayers.size(); ++i) {
        sortingLayers[i].order = static_cast<int>(i);
        idToOrderIndex[sortingLayers[i].id] = static_cast<int>(i);
    }

    return true;
}

void SortingLayerManager::InitializeDefaultLayers() {
    Clear();

    // Default sorting layers (like Unity)
    AddLayer("Default");
    AddLayer("Background");
    AddLayer("Foreground");
    AddLayer("UI");
}

void SortingLayerManager::Clear() {
    sortingLayers.clear();
    nameToID.clear();
    idToOrderIndex.clear();
    nextID = 0;
}
