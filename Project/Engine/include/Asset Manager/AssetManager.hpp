#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <type_traits>
#include <filesystem>
#include <queue>
#include <list>
#include "Logging.hpp"
//#include <FileWatch.hpp>
#include "../Engine.h"
#include "Utilities/GUID.hpp"
#include "MetaFilesManager.hpp"
#include "Asset Manager/AssetMeta.hpp"
#include <Graphics/Model/Model.h>
#include "Graphics/TextRendering/Font.hpp"
#include "Graphics/Material.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Sound/Audio.hpp"

class ENGINE_API AssetManager {
public:
	static AssetManager& GetInstance();

	bool CompileAsset(const std::string& filePathStr, bool forceCompile = false, bool forAndroid = false);
	void AddAssetMetaToMap(const std::string& assetPath);

	template <typename T>
	bool CompileAsset(const std::string& filePathStr, bool forceCompile = false, bool forAndroid = false) {
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
				return CompileAssetToResource<T>(guid, filePath, forceCompile, forAndroid);
			}
		}
		else {
			return CompileAssetToResource<T>(guid, filePath, forceCompile, forAndroid);
		}
	}

	bool CompileTexture(std::string filePath, std::string texType, GLint slot, bool forceCompile = false, bool forAndroid = false);

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
	bool IsExtensionTexture(const std::string& extension) const;

	bool HandleMetaFileDeletion(const std::string& metaFilePath);
	bool HandleResourceFileDeletion(const std::string& resourcePath);

	void CompileAllAssetsForAndroid();
	void CompileAllAssetsForDesktop();

	enum class Event {
		added,
		removed,
		modified,
		renamed_old,
		renamed_new
	};

	void AddToEventQueue(AssetManager::Event event, const std::filesystem::path& assetPath);
	void RunEventQueue();

	const std::filesystem::path& GetAndroidResourcesPath();
	std::string ExtractRelativeAndroidPath(const std::string& fullAndroidPath);

private:
	std::unordered_map<GUID_128, std::shared_ptr<AssetMeta>> assetMetaMap;
	std::list<std::pair<AssetManager::Event, std::filesystem::path>> assetEventQueue;
	std::pair<AssetManager::Event, std::filesystem::path> previousEvent;
	std::chrono::steady_clock::time_point previousEventTime;
	
	std::filesystem::path androidResourcesPath{ "../../../AndroidProject/app/src/main/assets" };
	std::filesystem::path canonicalAndroidResourcesPath;

	// Supported asset extensions
	const std::unordered_set<std::string> textureExtensions = { ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG", ".bmp", ".BMP" };
	const std::unordered_set<std::string> audioExtensions = { ".wav", ".ogg" };
	const std::unordered_set<std::string> fontExtensions = { ".ttf" };
	const std::unordered_set<std::string> modelExtensions = { ".obj", ".fbx" };
	const std::unordered_set<std::string> shaderExtensions = { ".vert", ".frag" };
	const std::unordered_set<std::string> materialExtensions = { ".mat" };
	std::unordered_set<std::string> supportedAssetExtensions;

	AssetManager() {
		InitializeSupportedExtensions();
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
	bool CompileAssetToResource(GUID_128 guid, const std::string& filePath, bool forceCompile = false, bool forAndroid = false) {
		// If the asset is not already loaded, load and store it using the GUID.
		if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
			std::shared_ptr<T> asset = std::make_shared<T>();
			std::string compiledPath = asset->CompileToResource(filePath);
			if (compiledPath.empty()) {
				ENGINE_LOG_ERROR("[AssetManager] ERROR: Failed to compile asset: " + std::string(filePath));
				//std::cerr << "[AssetManager] ERROR: Failed to compile asset: " << filePath << std::endl;
				return false;
			}

			std::shared_ptr<AssetMeta> assetMeta;
			if (!forAndroid) {
				assetMeta = asset->GenerateBaseMetaFile(guid, filePath, compiledPath);
			}
			else {
				assetMeta = assetMetaMap.find(guid)->second;
				// For Android, update the compiled path in the existing meta
				assetMeta->compiledFilePath = compiledPath;
			}
			assetMetaMap[guid] = assetMeta;
			std::cout << "[AssetManager] Compiled asset: " << filePath << " to " << compiledPath << std::endl << std::endl;

			if (!forAndroid) {
				// If the resource is already loaded, hot-reload the resource.
				if (ResourceManager::GetInstance().IsResourceLoaded(guid)) {
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
						if (audioExtensions.find(extension) != audioExtensions.end()) {
							ResourceManager::GetInstance().GetResource<Audio>(filePath, true);
						}
						else if (modelExtensions.find(extension) != modelExtensions.end()) {
							ResourceManager::GetInstance().GetResource<Model>(filePath, true);
						}
					}
				}
			}

			std::cout << std::endl;
			return true;
		}

		return true;
	}

	bool CompileTextureToResource(GUID_128 guid, const char* filePath, const char* texType, GLint slot, bool forceCompile = false, bool forAndroid = false) {
		// If the asset is not already loaded, load and store it using the GUID.
		if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
			Texture texture{ texType, slot };
			std::string compiledPath = texture.CompileToResource(filePath);
			if (compiledPath.empty()) {
				ENGINE_LOG_ERROR("[AssetManager] ERROR: Failed to compile asset: " + std::string(filePath));
				//std::cerr << "[AssetManager] ERROR: Failed to compile asset: " << filePath << std::endl;
				return false;
			}

			std::shared_ptr<AssetMeta> assetMeta;
			if (!forAndroid) {
				assetMeta = texture.GenerateBaseMetaFile(guid, filePath, compiledPath);
			}
			else {
				assetMeta = assetMetaMap.find(guid)->second;
				// For Android, update the compiled path in the existing meta
				assetMeta->compiledFilePath = compiledPath;
			}
			assetMeta = texture.ExtendMetaFile(filePath, assetMeta);
			assetMetaMap[guid] = assetMeta;
			std::cout << "[AssetManager] Compiled asset: " << filePath << " to " << compiledPath << std::endl << std::endl;

			if (!forAndroid) {
				// If the resource is already loaded, hot-reload the resource.
				if (ResourceManager::GetInstance().IsResourceLoaded(guid)) {
					ResourceManager::GetInstance().GetResource<Texture>(filePath, true);
				}
				else {
					ResourceManager::GetInstance().GetResource<Texture>(filePath);
				}
			}

			return true;
		}

		return true;
	}
};