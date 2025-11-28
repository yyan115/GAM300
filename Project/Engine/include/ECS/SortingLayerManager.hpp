#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "Engine.h"

struct SortingLayer {
    int id;
    std::string name;
    int order; // Position in the list, lower = rendered first (behind), higher = rendered last (on top)
};

class ENGINE_API SortingLayerManager {
public:
    static constexpr int MAX_SORTING_LAYERS = 32;

    static SortingLayerManager& GetInstance();

    // Sorting layer management
    bool SetLayerName(int id, const std::string& name);
    int AddLayer(const std::string& name);
    bool RemoveLayer(int id);
    const std::string& GetLayerName(int id) const;
    int GetLayerID(const std::string& name) const;
    int GetLayerOrder(int id) const; // Get the rendering order (position in list)

    // Get all layers in order
    const std::vector<SortingLayer>& GetAllLayers() const { return sortingLayers; }

    // Reorder layers (for future drag-drop support)
    bool MoveLayer(int id, int newOrder);

    // Default layers
    void InitializeDefaultLayers();

    // Serialization support
    void Clear();

private:
    SortingLayerManager();
    ~SortingLayerManager() = default;
    SortingLayerManager(const SortingLayerManager&) = delete;
    SortingLayerManager& operator=(const SortingLayerManager&) = delete;

    std::vector<SortingLayer> sortingLayers; // Ordered list of sorting layers
    std::unordered_map<std::string, int> nameToID;
    std::unordered_map<int, int> idToOrderIndex; // Maps layer ID to its position in sortingLayers vector
    int nextID = 0;
};
