#pragma once
#include "Engine.h"
#include <string>
#include "ECS/ECSManager.hpp"
#include "Asset Manager/AssetManager.hpp"

ENGINE_API bool InstantiatePrefabFromFile(ECSManager& ecs,
    AssetManager& assets,
    const std::string& prefabPath,
    Entity e);

// NEW: Save an entity to a prefab file using reflection JSON
ENGINE_API bool SaveEntityToPrefabFile(ECSManager& ecs,
    AssetManager& assets,
    Entity entity,
    const std::string& dstPath);