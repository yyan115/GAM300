#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <type_traits>
#include <filesystem>
#include <queue>
//#include <FileWatch.hpp>
#include "../Engine.h"
#include "GUID.hpp"
#include "MetaFilesManager.hpp"
#include "Asset Manager/AssetMeta.hpp"
#include <Graphics/Model/Model.h>
#include "Graphics/TextRendering/Font.hpp"
#include "Asset Manager/ResourceManager.hpp"

class ENGINE_API AssetManager {
public:
	static AssetManager& GetInstance();

	void AddAssetMetaToMap(const std::string& assetPath) {
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

	bool CompileAsset(const std::string& filePathStr, bool forceCompile = false) {
		std::filesystem::path filePathObj(filePathStr);
		std::string extension = filePathObj.extension().string();
		if (textureExtensions.find(extension) != textureExtensions.end()) {
			return CompileTexture(filePathStr, "diffuse", -1, forceCompile);
		}
		//else if (audioExtensions.find(extension) != audioExtensions.end()) {
		//	return CompileAsset<Audio>(filePathStr);
		//}
		else if (fontExtensions.find(extension) != fontExtensions.end()) {
			return CompileAsset<Font>(filePathStr, forceCompile);
		}
		else if (modelExtensions.find(extension) != modelExtensions.end()) {
			return CompileAsset<Model>(filePathStr, forceCompile);
		}
		else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
			return CompileAsset<Shader>(filePathStr, forceCompile);
		}
		else {
			std::cerr << "[AssetManager] ERROR: Attempting to compile unsupported asset extension: " << extension << std::endl;
			return false;
		}
	}

	template <typename T>
	bool CompileAsset(const std::string& filePathStr, bool forceCompile = false) {
		static_assert(!std::is_same_v<T, Texture>,
			"Calling AssetManager::GetInstance().GetAsset() to compile a texture is forbidden. Use CompileTexture() instead.");

		std::filesystem::path filePathObj(filePathStr);
		std::string filePath;
		if (std::is_same_v<T, Shader>) {
			filePath = (filePathObj.parent_path() / filePathObj.stem()).generic_string();
		}
		else filePath = filePathObj.generic_string();

		GUID_128 guid{};
		if (!MetaFilesManager::MetaFileExists(filePath) || !MetaFilesManager::MetaFileUpdated(filePath)) {
			GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
			guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
		}
		else {
			guid = MetaFilesManager::GetGUID128FromAssetFile(filePath);
		}

		if (!forceCompile) {
			auto it = assetMetaMap.find(guid);
			if (it != assetMetaMap.end()) {
				return true;
			}
			else {
				return CompileAssetToResource<T>(guid, filePath, forceCompile);
			}
		}
		else {
			return CompileAssetToResource<T>(guid, filePath, forceCompile);
		}
	}

	bool CompileTexture(std::string filePath, std::string texType, GLint slot, bool forceCompile = false) {
		GUID_128 guid{};
		if (!MetaFilesManager::MetaFileExists(filePath) || !MetaFilesManager::MetaFileUpdated(filePath)) {
			GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
			guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
		}
		else {
			guid = MetaFilesManager::GetGUID128FromAssetFile(filePath);
		}

		if (!forceCompile) {
			auto it = assetMetaMap.find(guid);
			if (it != assetMetaMap.end()) {
				return true;
			}
			else {
				return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot, forceCompile);
			}
		}
		else {
			return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot, forceCompile);
		}
	}

	bool IsAssetCompiled(GUID_128 guid) {
		return assetMetaMap.find(guid) != assetMetaMap.end();
	}

	void UnloadAsset(const std::string& assetPath) {
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

	void UnloadAllAssets() {
		assetMetaMap.clear();
	}

	GUID_128 GetGUID128FromAssetMeta(const std::string& assetPath) {
		for (const auto& pair : assetMetaMap) {
			if (pair.second->sourceFilePath == assetPath) {
				return pair.first;
			}
		}

		std::cerr << "[AssetManager] ERROR: Failed to get GUID_128 from AssetMeta, asset not found in AssetMetaMap: " << assetPath << std::endl;
		return GUID_128{};
	}

	std::shared_ptr<AssetMeta> GetAssetMeta(GUID_128 guid) {
		auto it = assetMetaMap.find(guid);
		if (it != assetMetaMap.end()) {
			return it->second;
		}
		else {
			std::cerr << "[AssetManager] ERROR: AssetMeta not found for GUID_128." << std::endl;
			return nullptr;
		}
	}

	void InitializeSupportedExtensions() {
		supportedAssetExtensions.insert(textureExtensions.begin(), textureExtensions.end());
		//supportedExtensions.insert(audioExtensions.begin(), audioExtensions.end());
		supportedAssetExtensions.insert(fontExtensions.begin(), fontExtensions.end());
		supportedAssetExtensions.insert(modelExtensions.begin(), modelExtensions.end());
		supportedAssetExtensions.insert(shaderExtensions.begin(), shaderExtensions.end());
	}

	std::unordered_set<std::string>& GetSupportedExtensions() {
		return supportedAssetExtensions;
	}

	const std::unordered_set<std::string>& GetShaderExtensions() const {
		return shaderExtensions;
	}

	bool IsAssetExtensionSupported(const std::string& extension) const {
		return supportedAssetExtensions.find(extension) != supportedAssetExtensions.end();
	}

	bool IsExtensionMetaFile(const std::string& extension) const {
		return extension == ".meta";
	}

	bool IsExtensionShaderVertFrag(const std::string& extension) const {
		return shaderExtensions.find(extension) != shaderExtensions.end();
	}

	bool HandleMetaFileDeletion(const std::string& metaFilePath) {
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

	bool HandleResourceFileDeletion(const std::string& resourcePath) {
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

	void AddToCompilationQueue(const std::filesystem::path& assetPath);

	void RunCompilationQueue();

private:
	std::unordered_map<GUID_128, std::shared_ptr<AssetMeta>> assetMetaMap;
	std::queue<std::filesystem::path> compilationQueue;
	//std::unique_ptr<filewatch::FileWatch<std::string>> assetWatcher;

	// Supported asset extensions
	const std::unordered_set<std::string> textureExtensions = { ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG", ".bmp", ".BMP" };
	const std::unordered_set<std::string> audioExtensions = { ".wav", ".ogg" };
	const std::unordered_set<std::string> fontExtensions = { ".ttf" };
	const std::unordered_set<std::string> modelExtensions = { ".obj", ".fbx" };
	const std::unordered_set<std::string> shaderExtensions = { ".vert", ".frag" };
	std::unordered_set<std::string> supportedAssetExtensions;

	AssetManager() {
		InitializeSupportedExtensions();
		//assetWatcher = std::make_unique<filewatch::FileWatch<std::string>>(
		//	"Resources",
		//	[this](const std::string& path, const filewatch::Event event_type) {
		//		std::filesystem::path pathObj("Resources/" + path);
		//		std::string fullPath = pathObj.generic_string();
		//		std::string extension = pathObj.extension().generic_string();
		//		if (IsAssetExtensionSupported(extension)) {
		//			// Sleep this thread for a while to allow the OS to finish the file operation.
		//			std::this_thread::sleep_for(std::chrono::milliseconds(100));

		//			if (event_type == filewatch::Event::modified || event_type == filewatch::Event::added) {
		//				std::cout << "[AssetWatcher] Detected change in asset: " << fullPath << ". Recompiling..." << std::endl;
		//				AssetManager::GetInstance().CompileAsset(fullPath);
		//			}
		//			else if (event_type == filewatch::Event::removed) {
		//				std::cout << "[AssetWatcher] Detected removal of asset: " << fullPath << ". Unloading..." << std::endl;
		//				AssetManager::GetInstance().UnloadAsset(fullPath);
		//			}
		//			else if (event_type == filewatch::Event::renamed_old) {
		//				std::cout << "[AssetWatcher] Detected rename (old name) of asset: " << fullPath << ". Unloading..." << std::endl;
		//				AssetManager::GetInstance().UnloadAsset(fullPath);
		//			}
		//			else if (event_type == filewatch::Event::renamed_new) {
		//				std::cout << "[AssetWatcher] Detected rename (new name) of asset: " << fullPath << ". Recompiling..." << std::endl;
		//				AssetManager::GetInstance().CompileAsset(fullPath);
		//			}
		//		}
		//		else if (IsExtensionMetaFile(extension)) {
		//			if (event_type == filewatch::Event::removed) {
		//				std::cout << "[AssetWatcher] WARNING: Detected removal of .meta file: " << fullPath << ". Deleting associated resource..." << std::endl;
		//				HandleMetaFileDeletion(fullPath);
		//			}
		//		}
		//		else if (ResourceManager::GetInstance().IsResourceExtensionSupported(extension)) {
		//			if (event_type == filewatch::Event::removed) {
		//				std::cout << "[AssetWatcher] WARNING: Detected removal of resource file: " << fullPath << ". Deleting associated meta file..." << std::endl;
		//				HandleResourceFileDeletion(fullPath);
		//			}
		//		}
		//	}
		//);
	}

	///**
	// * \brief Returns a singleton container for the asset type T.
	// *
	// * \tparam T The type of the assets.
	// * \return A reference to the map of assets for the specified type.
	// */
	//template <typename T>
	//std::unordered_map<GUID_128, std::shared_ptr<T>>& GetAssetMap() {
	//	static std::unordered_map<GUID_128, std::shared_ptr<T>> assetMap;
	//	return assetMap;
	//}

	template <typename T>
	bool CompileAssetToResource(GUID_128 guid, const std::string& filePath, bool forceCompile = false) {
		// If the asset is not already loaded, load and store it using the GUID.
		if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
			std::shared_ptr<T> asset = std::make_shared<T>();
			std::string compiledPath = asset->CompileToResource(filePath);
			if (compiledPath.empty()) {
				std::cerr << "[AssetManager] ERROR: Failed to compile asset: " << filePath << std::endl;
				return false;
			}

			std::shared_ptr<AssetMeta> assetMeta = asset->GenerateBaseMetaFile(guid, filePath, compiledPath);
			assetMetaMap[guid] = assetMeta;
			std::cout << "[AssetManager] Compiled asset: " << filePath << " to " << compiledPath << std::endl;

			// If the resource is already loaded, hot-reload the resource.
			if (ResourceManager::GetInstance().IsResourceLoaded(filePath, guid)) {
				std::cout << "[AssetManager] Resource is already loaded - hot-reloading the resource: " << compiledPath << std::endl;
				if constexpr (std::is_same_v<T, Font>) {
					ResourceManager::GetInstance().GetFontResource(filePath, 0, true);
				}
				else if (std::is_same_v<T, Shader>) {
					ResourceManager::GetInstance().GetResource<Shader>(filePath, true);
				}
				else {
					std::filesystem::path p(filePath);
					std::string extension = p.extension().generic_string();
					//else if (audioExtensions.find(extension) != audioExtensions.end()) {
					//	return CompileAsset<Audio>(filePathStr);
					//}
					if (modelExtensions.find(extension) != modelExtensions.end()) {
						ResourceManager::GetInstance().GetResource<Model>(filePath, true);
					}
				}
			}

			std::cout << std::endl;
			return true;
		}

		return true;
	}

	bool CompileTextureToResource(GUID_128 guid, const char* filePath, const char* texType, GLint slot, bool forceCompile = false) {
		// If the asset is not already loaded, load and store it using the GUID.
		if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
			Texture texture{ texType, slot };
			std::string compiledPath = texture.CompileToResource(filePath);
			if (compiledPath.empty()) {
				std::cerr << "[AssetManager] ERROR: Failed to compile asset: " << filePath << std::endl;
				return false;
			}

			std::shared_ptr<AssetMeta> assetMeta = texture.GenerateBaseMetaFile(guid, filePath, compiledPath);
			assetMeta = texture.ExtendMetaFile(filePath, assetMeta);
			assetMetaMap[guid] = assetMeta;
			std::cout << "[AssetManager] Compiled asset: " << filePath << " to " << compiledPath << std::endl << std::endl;

			// If the resource is already loaded, hot-reload the resource.
			if (ResourceManager::GetInstance().IsResourceLoaded(filePath, guid)) {
				ResourceManager::GetInstance().GetResource<Texture>(filePath, true);
			}
			else {
				ResourceManager::GetInstance().GetResource<Texture>(filePath);
			}

			return true;
		}

		return true;
	}
};