#include "pch.h"
#include "Asset Manager/ResourceManager.hpp"

// MUST DEFINE THIS IN A SEPARATE CPP FILE SO THE SINGLETON PERSISTS ACROSS EDITOR AND ENGINE.
ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}

void ResourceManager::Shutdown() {
    // These asset types use explicit GL cleanup rather than destructors.
    for (auto& entry : GetResourceMap<Shader>()) {
        if (entry.second) entry.second->Delete();
    }
    for (auto& entry : GetResourceMap<Texture>()) {
        if (entry.second) entry.second->Delete();
    }
    resourceMaps.clear();
}
