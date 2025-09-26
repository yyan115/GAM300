#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <type_traits>
#include <filesystem>
#include <thread>
//#include <FileWatch.hpp>
#include "../Engine.h"
#include "Utilities/GUID.hpp"
#include "MetaFilesManager.hpp"
#include "Asset Manager/AssetMeta.hpp"
#include <Graphics/Model/Model.h>
#include "Graphics/TextRendering/Font.hpp"
#include "Asset Manager/ResourceManager.hpp"

class ENGINE_API AssetManager {
public:
	static AssetManager& GetInstance();

	void AddAssetMetaToMap(const std::string& assetPath);
	bool CompileAsset(const std::string& filePathStr);

	template <typename T>
	bool CompileAsset(const std::string& filePathStr) {
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

		auto it = assetMetaMap.find(guid);
		if (it != assetMetaMap.end()) {
			return true;
		}
		else {
			return CompileAssetToResource<T>(guid, filePath);
		}
	}

	bool CompileTexture(std::string filePath, std::string texType, GLint slot);
	bool IsAssetCompiled(GUID_128 guid);
	void UnloadAsset(const std::string& assetPath);

	//void UnloadAllAssets() {
	//	assetMetaMap.clear();
	//}

	GUID_128 GetGUID128FromAssetMeta(const std::string& assetPath);
	std::shared_ptr<AssetMeta> GetAssetMeta(GUID_128 guid);

	void InitializeSupportedExtensions();
	std::unordered_set<std::string>& GetSupportedExtensions();
	const std::unordered_set<std::string>& GetShaderExtensions() const;
	bool IsAssetExtensionSupported(const std::string& extension) const;
	bool IsExtensionMetaFile(const std::string& extension) const;
	bool IsExtensionShaderVertFrag(const std::string& extension) const;

	bool HandleMetaFileDeletion(const std::string& metaFilePath);
	bool HandleResourceFileDeletion(const std::string& resourcePath);

	std::string GetAssetPathFromGUID(const GUID_128 guid);

private:
	std::unordered_map<GUID_128, std::shared_ptr<AssetMeta>> assetMetaMap;
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
	std::shared_ptr<T> GetAsset(const std::string& assetPath) {
		return ResourceManager::GetInstance().GetResource<T>(assetPath);
	}

	template <typename T>
	bool CompileAssetToResource(GUID_128 guid, const std::string& filePath) {
		// If the asset is not already loaded, load and store it using the GUID.
		if (assetMetaMap.find(guid) == assetMetaMap.end()) {
			std::shared_ptr<T> asset = std::make_shared<T>();
			std::string compiledPath = asset->CompileToResource(filePath);
			if (compiledPath.empty()) {
				std::cerr << "[AssetManager] ERROR: Failed to compile asset: " << filePath << std::endl;
				return false;
			}

			std::shared_ptr<AssetMeta> assetMeta = asset->GenerateBaseMetaFile(guid, filePath, compiledPath);
			assetMetaMap[guid] = assetMeta;
			std::cout << "[AssetManager] Compiled asset: " << filePath << " to " << compiledPath << std::endl << std::endl;
			return true;
		}

		return true;
	}

	bool CompileTextureToResource(GUID_128 guid, const char* filePath, const char* texType, GLint slot) {
		// If the asset is not already loaded, load and store it using the GUID.
		if (assetMetaMap.find(guid) == assetMetaMap.end()) {
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
			return true;
		}

		return true;
	}
};