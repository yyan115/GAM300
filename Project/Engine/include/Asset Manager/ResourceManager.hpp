#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>
#include "GUID.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Graphics/TextRendering/Font.hpp"
#include "Asset Manager/FileUtilities.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif

class ResourceManager {
public:
	static ResourceManager& GetInstance() {
		static ResourceManager instance;
		return instance;
	}

	template <typename T>
	std::shared_ptr<T> GetResource(const std::string& assetPath) {
		static_assert(!std::is_same_v<T, Font>,
			"Calling ResourceManager::GetInstance().GetResource() to get a font is forbidden. Use GetFontResource() instead.");

		auto& resourceMap = GetResourceMap<T>();
		std::filesystem::path filePathObj(assetPath);
		std::string filePath = filePathObj.generic_string();

		GUID_128 guid{};
		if (!MetaFilesManager::MetaFileExists(filePath)) {
			GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
			guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
		}
		else {
			guid = MetaFilesManager::GetGUID128FromAssetFile(filePath);
		}
		
		// Return a shared pointer to the resource (Texture, Model, etc.)
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			return it->second;
		}
		else {
			return LoadResource<T>(guid, filePath);
		}
	}

	std::shared_ptr<Font> GetFontResource(const std::string& assetPath, unsigned int fontSize = 48) {
		auto& resourceMap = GetResourceMap<Font>();
		std::filesystem::path filePathObj(assetPath);
		std::string filePath = filePathObj.generic_string();
		std::string storedFilePath = filePathObj.generic_string() + std::to_string(fontSize);

		GUID_128 guid{};
		if (!MetaFilesManager::MetaFileExists(storedFilePath)) {
			GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
			guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
		}
		else {
			guid = MetaFilesManager::GetGUID128FromAssetFile(storedFilePath);
		}

		// Return a shared pointer to the resource (Font).
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			return it->second;
		}
		else {
			return LoadFontResource(guid, filePath, fontSize);
		}
	}

	template <typename T>
	bool UnloadResource(GUID_128 guid, const std::string& assetPath, const std::string& resourcePath) {
		// Implementation for unloading the resource
		auto& resourceMap = GetResourceMap<T>();
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			resourceMap.erase(it);
			std::cout << "[ResourceManager] Removed from resource map: " << resourcePath << std::endl;
		}

		if (FileUtilities::RemoveFile(resourcePath)) {
			std::cout << "[ResourceManager] Deleted resource file: " << resourcePath << std::endl;
			if (MetaFilesManager::DeleteMetaFile(assetPath)) {
				std::cout << "[ResourceManager] Deleted meta file for resource: " << resourcePath << std::endl;
				return true;
			}
			else {
				std::cerr << "[ResourceManager] ERROR: Failed to delete meta file for resource: " << resourcePath << std::endl;
				return false;
			}
		}

		std::cerr << "[ResourceManager] ERROR: Failed to unload resource: " << resourcePath << std::endl;
		return false;
	}

	bool UnloadResource(GUID_128 guid, const std::string& assetPath, const std::string& resourcePath) {
		std::filesystem::path p(resourcePath);
		std::string extension = p.extension().string();
		if (textureExtensions.find(extension) != textureExtensions.end()) {
			return UnloadResource<Texture>(guid, assetPath, resourcePath);
		}
		else if (fontExtensions.find(extension) != fontExtensions.end()) {
			return UnloadResource<Font>(guid, assetPath, resourcePath);
		}
		else if (modelExtensions.find(extension) != modelExtensions.end()) {
			return UnloadResource<Model>(guid, assetPath, resourcePath);
		}
		else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
			return UnloadResource<Shader>(guid, assetPath, resourcePath);
		}
		else {
			std::cerr << "[ResourceManager] ERROR: Trying to unload unsupported resource extension: " << extension << std::endl;
			return false;
		}
	}

	template <typename T>
	void UnloadAllResourcesOfType() {
		GetResourceMap<T>().clear();
	}

	bool IsResourceExtensionSupported(const std::string& extension) const {
		return supportedResourceExtensions.find(extension) != supportedResourceExtensions.end();
	}

private:
	// Supported resource extensions
	const std::unordered_set<std::string> textureExtensions = { ".dds"};
	const std::unordered_set<std::string> audioExtensions = {  };
	const std::unordered_set<std::string> fontExtensions = { ".font" };
	const std::unordered_set<std::string> modelExtensions = { ".mesh" };
	const std::unordered_set<std::string> shaderExtensions = { ".shader" };

	// Supported resource extensions
	std::unordered_set<std::string> supportedResourceExtensions;

	ResourceManager() {
		supportedResourceExtensions.insert(textureExtensions.begin(), textureExtensions.end());
		supportedResourceExtensions.insert(audioExtensions.begin(), audioExtensions.end());
		supportedResourceExtensions.insert(fontExtensions.begin(), fontExtensions.end());
		supportedResourceExtensions.insert(modelExtensions.begin(), modelExtensions.end());
		supportedResourceExtensions.insert(shaderExtensions.begin(), shaderExtensions.end());
	};

	/**
	 * \brief Returns a singleton container for the asset type T.
	 *
	 * \tparam T The type of the assets.
	 * \return A reference to the map of assets for the specified type.
	 */
	template <typename T>
	std::unordered_map<GUID_128, std::shared_ptr<T>>& GetResourceMap() {
		static std::unordered_map<GUID_128, std::shared_ptr<T>> resourceMap;
		return resourceMap;
	}


	template <typename T>
	std::shared_ptr<T> LoadResource(const GUID_128& guid, const std::string& assetPath) {
		std::shared_ptr<T> resource = std::make_shared<T>();

#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[ResourceManager] Attempting to load resource: %s", assetPath.c_str());
#endif
		// Use the regular LoadResource method - the Android asset loading
		// is now handled in get_file_contents() at a lower level
		if (resource->LoadResource(assetPath)) {
			auto& resourceMap = GetResourceMap<T>();
			resourceMap[guid] = resource;
			std::cout << "[ResourceManager] Loaded resource for: " << assetPath << std::endl;
#ifdef ANDROID
			__android_log_print(ANDROID_LOG_INFO, "GAM300", "[ResourceManager] Successfully loaded resource: %s", assetPath.c_str());
#endif
			return resource;
		}

		std::cerr << "[ResourceManager] ERROR: Failed to load resource: " << assetPath << std::endl;
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ResourceManager] ERROR: Failed to load resource: %s", assetPath.c_str());
#endif
		return nullptr;
	}

	std::shared_ptr<Font> LoadFontResource(const GUID_128& guid, const std::string& assetPath, unsigned int fontSize) {
		std::shared_ptr<Font> font = std::make_shared<Font>();
		if (font->LoadResource(assetPath, fontSize)) {
			auto& resourceMap = GetResourceMap<Font>();
			resourceMap[guid] = font;
			std::cout << "[ResourceManager] Loaded resource for: " << assetPath << std::endl;
			return font;
		}

		std::cerr << "[ResourceManager] ERROR: Failed to load resource: " << assetPath << std::endl;
		return nullptr;
	}
};