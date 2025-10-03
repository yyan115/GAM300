#include "pch.h"
#include "Asset Manager/ResourceManager.hpp"

// MUST DEFINE THIS IN A SEPARATE CPP FILE SO THE SINGLETON PERSISTS ACROSS EDITOR AND ENGINE.
ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}