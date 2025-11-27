/* Start Header ************************************************************************/
/*!
\file       SiblingIndexComponent.hpp
\author     Muhammad Zikry
\date       2025
\brief      Manages layers within the ECS system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <array>
#include <string>
#include <unordered_map>
#include "Engine.h"

class ENGINE_API LayerManager {
public:
    static constexpr int MAX_LAYERS = 32;

    static LayerManager& GetInstance();

    // Layer management
    bool SetLayerName(int index, const std::string& name);
    int AddLayer(const std::string& name);
    bool RemoveLayer(int index);
    const std::string& GetLayerName(int index) const;
    int GetLayerIndex(const std::string& name) const;
    const std::array<std::string, MAX_LAYERS>& GetAllLayers() const { return layerNames; }

    // Default layers
    void InitializeDefaultLayers();

private:
    LayerManager();
    ~LayerManager() = default;
    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    std::array<std::string, MAX_LAYERS> layerNames;
    std::unordered_map<std::string, int> nameToIndex;
};