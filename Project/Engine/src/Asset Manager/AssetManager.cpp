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
void AssetManager::AddAssetMetaToMap(const std::string& assetPath) {
	std::filesystem::path p(assetPath);
	std::string extension = p.extension().string();
	std::string metaFilePath = assetPath + ".meta";
	std::shared_ptr<AssetMeta> assetMeta;
	if (textureExtensions.find(extension) != textureExtensions.end()) {
		assetMeta = std::make_shared<TextureMeta>();
		assetMeta->PopulateAssetMetaFromFile(metaFilePath);
	}
	else {
		assetMeta = std::make_shared<AssetMeta>();
		assetMeta->PopulateAssetMetaFromFile(metaFilePath);
	}

	// Check if the compiled resource file exists. If not, we need to recompile the asset.
	if (!std::filesystem::exists(assetMeta->compiledFilePath)) {
		std::cout << "[AssetManager] WARNING: Compiled resource file missing for asset: " << assetPath << ". Recompiling..." << std::endl;
		CompileAsset(assetPath);
	}

	assetMetaMap[assetMeta->guid] = assetMeta;
}

bool AssetManager::CompileAsset(const std::string& filePathStr) {
	std::filesystem::path filePathObj(filePathStr);
	std::string extension = filePathObj.extension().string();
	if (textureExtensions.find(extension) != textureExtensions.end()) {
		return CompileTexture(filePathStr, "diffuse", -1);
	}
	//else if (audioExtensions.find(extension) != audioExtensions.end()) {
	//	return CompileAsset<Audio>(filePathStr);
	//}
	else if (fontExtensions.find(extension) != fontExtensions.end()) {
		return CompileAsset<Font>(filePathStr);
	}
	else if (modelExtensions.find(extension) != modelExtensions.end()) {
		return CompileAsset<Model>(filePathStr);
	}
	else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
		return CompileAsset<Shader>(filePathStr);
	}
	else {
		std::cerr << "[AssetManager] ERROR: Attempting to compile unsupported asset extension: " << extension << std::endl;
		return false;
	}
}

bool AssetManager::CompileTexture(std::string filePath, std::string texType, GLint slot) {
	GUID_128 guid{};
	if (!MetaFilesManager::MetaFileExists(filePath) || !MetaFilesManager::MetaFileUpdated(filePath)) {
		GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
		guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
	}
	else {
		guid = MetaFilesManager::GetGUID128FromAssetFile(filePath);
	}

	auto it = assetMetaMap.find(guid);
	if (it != assetMetaMap.end()) {
		return true;
	}
	else {
		return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot);
	}
}

bool AssetManager::IsAssetCompiled(GUID_128 guid) {
	return assetMetaMap.find(guid) != assetMetaMap.end();
}

void AssetManager::UnloadAsset(const std::string& assetPath) {
	GUID_128 guid{};

	// Fallback if the user accidentally deleted the .meta file.
	if (!MetaFilesManager::MetaFileExists(assetPath)) {
		std::cout << "[AssetManager] WARNING: .meta file missing for asset: " << assetPath << ". Resulting to fallback." << std::endl;
		guid = GetGUID128FromAssetMeta(assetPath);
	}
	else {
		guid = MetaFilesManager::GetGUID128FromAssetFile(assetPath);
	}

	auto it = assetMetaMap.find(guid);
	if (it != assetMetaMap.end()) {
		std::filesystem::path filePathObj(assetPath);
		std::string extension = filePathObj.extension().string();
		if (textureExtensions.find(extension) != textureExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Texture>(guid, assetPath, it->second->compiledFilePath);
			MetaFilesManager::DeleteMetaFile(assetPath);
		}
		//else if (audioExtensions.find(extension) != audioExtensions.end()) {
		//	return CompileAsset<Audio>(filePathStr);
		//}
		else if (fontExtensions.find(extension) != fontExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Font>(guid, assetPath, it->second->compiledFilePath);
			MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (modelExtensions.find(extension) != modelExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Model>(guid, assetPath, it->second->compiledFilePath);
			MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Shader>(guid, assetPath, it->second->compiledFilePath);
			MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else {
			std::cerr << "[AssetManager] ERROR: Trying to unload unsupported asset extension: " << extension << std::endl;
		}

		assetMetaMap.erase(it);
		std::cout << "[AssetManager] Unloaded asset: " << assetPath << std::endl << std::endl;
	}
}

GUID_128 AssetManager::GetGUID128FromAssetMeta(const std::string& assetPath) {
	for (const auto& pair : assetMetaMap) {
		if (pair.second->sourceFilePath == assetPath) {
			return pair.first;
		}
	}

	std::cerr << "[AssetManager] ERROR: Failed to get GUID_128 from AssetMeta, asset not found in AssetMetaMap: " << assetPath << std::endl;
	return GUID_128{};
}

std::shared_ptr<AssetMeta> AssetManager::GetAssetMeta(GUID_128 guid) {
	auto it = assetMetaMap.find(guid);
	if (it != assetMetaMap.end()) {
		return it->second;
	}
	else {
		std::cerr << "[AssetManager] ERROR: AssetMeta not found for GUID_128." << std::endl;
		return nullptr;
	}
}

void AssetManager::InitializeSupportedExtensions() {
	supportedAssetExtensions.insert(textureExtensions.begin(), textureExtensions.end());
	//supportedExtensions.insert(audioExtensions.begin(), audioExtensions.end());
	supportedAssetExtensions.insert(fontExtensions.begin(), fontExtensions.end());
	supportedAssetExtensions.insert(modelExtensions.begin(), modelExtensions.end());
	supportedAssetExtensions.insert(shaderExtensions.begin(), shaderExtensions.end());
}

std::unordered_set<std::string>& AssetManager::GetSupportedExtensions() {
	return supportedAssetExtensions;
}

const std::unordered_set<std::string>& AssetManager::GetShaderExtensions() const {
	return shaderExtensions;
}

bool AssetManager::IsAssetExtensionSupported(const std::string& extension) const {
	return supportedAssetExtensions.find(extension) != supportedAssetExtensions.end();
}

bool AssetManager::IsExtensionMetaFile(const std::string& extension) const {
	return extension == ".meta";
}

bool AssetManager::HandleMetaFileDeletion(const std::string& metaFilePath) {
	// Get the compiled resource file path from the asset's AssetMeta.
	std::string assetFilePath = metaFilePath.substr(0, metaFilePath.size() - 5); // Remove ".meta"
	bool failedToUnload = false;
	for (const auto& pair : assetMetaMap) {
		if (pair.second->sourceFilePath == assetFilePath) {
			std::string resourcePath = pair.second->compiledFilePath;
			GUID_128 guid = pair.first;

			if (ResourceManager::GetInstance().UnloadResource(guid, assetFilePath, resourcePath)) {
				std::cout << "[AssetManager] Successfully deleted resource file and its associated meta file: " << resourcePath << ", " << metaFilePath << std::endl;
				return true;
			}
			else {
				failedToUnload = true;
				break;
			}
		}
	}

	if (failedToUnload) {
		std::cerr << "[AssetManager] ERROR: Failed to delete resource file due to meta file deletion." << std::endl;
		return false;
	}

	return true;
}

bool AssetManager::HandleResourceFileDeletion(const std::string& resourcePath) {
	bool failedToUnload = false;
	for (const auto& pair : assetMetaMap) {
		if (pair.second->compiledFilePath == resourcePath) {
			std::string assetFilePath = pair.second->sourceFilePath;;
			std::string metaPath = assetFilePath + ".meta";
			GUID_128 guid = pair.first;

			if (ResourceManager::GetInstance().UnloadResource(guid, assetFilePath, resourcePath)) {
				std::cout << "[AssetManager] Successfully deleted resource file and its associated meta file: " << resourcePath << ", " << metaPath << std::endl;
				return true;
			}
			else {
				failedToUnload = true;
				break;
			}
		}
	}

	if (failedToUnload) {
		std::cerr << "[AssetManager] ERROR: Failed to delete resource file due to meta file deletion." << std::endl;
		return false;
	}

	return true;
}
