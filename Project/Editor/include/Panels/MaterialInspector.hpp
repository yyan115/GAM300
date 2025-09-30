#pragma once

#include <memory>
#include <string>
#include <functional>
#include <Graphics/Material.hpp>
#include <ECS/Entity.hpp>
#include "Utilities/GUID.hpp"

class MaterialInspector {
public:
    // GUI rendering method
    static void DrawMaterialAsset(std::shared_ptr<Material> material, const std::string& assetPath, bool showLockButton = false, bool* isLocked = nullptr, std::function<void()> lockCallback = nullptr);

    // Material application methods
    static void ApplyMaterialToModel(Entity entity, const GUID_128& materialGuid);
    static void ApplyMaterialToModelByPath(Entity entity, const std::string& materialPath);
};