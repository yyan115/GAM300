#include "pch.h"
#include "Asset Manager/AssetManager.hpp"

AssetManager& AssetManager::GetInstance() {
    static AssetManager instance; // lives only in the DLL
    return instance;
}

void AssetManager::AddToCompilationQueue(const std::filesystem::path& assetPath) {
    compilationQueue.push(assetPath);
}

void AssetManager::RunCompilationQueue() {
    if (!compilationQueue.empty()) {
        auto assetPath = compilationQueue.front();
        std::cout << "[Asset Manager] Running compilation queue... Compiling asset: " << assetPath.generic_string() << std::endl;
        compilationQueue.pop();
        std::string extension = assetPath.extension().string();

        CompileAsset(assetPath.generic_string(), true);
    }
}
