#include "pch.h"
#include "Asset Manager/AssetManager.hpp"

AssetManager& AssetManager::GetInstance() {
    static AssetManager instance; // lives only in the DLL
    return instance;
}