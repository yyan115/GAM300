#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>
#include "Utilities/GUID.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Graphics/TextRendering/Font.hpp"
#include "Graphics/Material.hpp"
#include "Graphics/Model/Model.h"
#include "Graphics/Texture.h"
#include "Graphics/ShaderClass.h"
#include "Utilities/FileUtilities.hpp"
#include "Sound/Audio.hpp"
#include "Logging.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif
// Forward declarations to avoid including heavy headers
class Audio;
class Texture;
class Model;
class Shader;

class ENGINE_API ResourceManager {
public:
	static ResourceManager& GetInstance() {
		static ResourceManager instance;
		return instance;
	}

	template <typename T>
	std::shared_ptr<T> GetResource(const std::string& assetPath, bool forceLoad = false) {
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
		std::string resourcePath = assetPath;
		if (!forceLoad) {
			auto it = resourceMap.find(guid);
			if (it != resourceMap.end()) {
				return it->second;
			}
			else {
				return LoadResource<T>(guid, resourcePath, assetPath, forceLoad);
			}
		}
		else {
			return LoadResource<T>(guid, resourcePath, assetPath, forceLoad);
		}
	}

	std::shared_ptr<Font> GetFontResource(const std::string& assetPath, unsigned int fontSize = 48, bool forceLoad = false) {
		auto& resourceMap = GetResourceMap<Font>();
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

		// Return a shared pointer to the resource (Font).
		std::string resourcePath = assetPath;
		if (!forceLoad) {
			auto it = resourceMap.find(guid);
			if (it != resourceMap.end()) {
				return it->second;
			}
			else {
				return LoadFontResource(guid, resourcePath, assetPath, fontSize, forceLoad);
			}
		}
		else {
			return LoadFontResource(guid, resourcePath, assetPath, fontSize, forceLoad);
		}
	}

	template <typename T>
	bool UnloadResource(GUID_128 guid, const std::string& assetPath, const std::string& resourcePath) {
		// Implementation for unloading the resource
		auto& resourceMap = GetResourceMap<T>();
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			resourceMap.erase(it);
			ENGINE_LOG_ERROR("[ResourceManager] Removed from resource map: " + resourcePath);
			//std::cout << "[ResourceManager] Removed from resource map: " << resourcePath << std::endl;
		}

		if (FileUtilities::RemoveFile(resourcePath)) {
			ENGINE_LOG_ERROR("[ResourceManager] Deleted resource file: " + resourcePath);
			return true;
			//if (MetaFilesManager::DeleteMetaFile(assetPath)) {
			//	std::cout << "[ResourceManager] Deleted meta file for resource: " << resourcePath << std::endl;
			//	return true;
			//}
			//else {
			//	std::cerr << "[ResourceManager] ERROR: Failed to delete meta file for resource: " << resourcePath << std::endl;
			//	return false;
			//}
		}
		ENGINE_LOG_ERROR("[ResourceManager] ERROR: Failed to unload resource: " + resourcePath);
		//std::cerr << "[ResourceManager] ERROR: Failed to unload resource: " << resourcePath << std::endl;
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
		else if (audioExtensions.find(extension) != audioExtensions.end()) {
			return UnloadResource<Audio>(guid, assetPath, resourcePath);
		}
		else if (materialExtensions.find(extension) != materialExtensions.end()) {
			return UnloadResource<Material>(guid, assetPath, resourcePath);
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

	bool IsExtensionShader(const std::string& extension) const {
		return shaderExtensions.find(extension) != shaderExtensions.end();
	}

	bool IsResourceLoaded(const GUID_128& guid) {
		if (GetResourceMap<Texture>().find(guid) != GetResourceMap<Texture>().end()) return true;
		else if (GetResourceMap<Model>().find(guid) != GetResourceMap<Model>().end()) return true;
		else if (GetResourceMap<Shader>().find(guid) != GetResourceMap<Shader>().end()) return true;
		else if (GetResourceMap<Font>().find(guid) != GetResourceMap<Font>().end()) return true;
		else if (GetResourceMap<Material>().find(guid) != GetResourceMap<Material>().end()) return true;
		return false;
	}

	// Helper function to get platform-specific shader path
	static std::string GetPlatformShaderPath(const std::string& baseShaderName) {
#ifdef ANDROID
		return "Resources/Shaders/" + baseShaderName + "android";
#else
		return "Resources/Shaders/" + baseShaderName;
#endif
	}

private:
	// Supported resource extensions
	const std::unordered_set<std::string> textureExtensions = { ".dds"};
	const std::unordered_set<std::string> audioExtensions = { ".wav", ".ogg" };
	const std::unordered_set<std::string> fontExtensions = { ".font" };
	const std::unordered_set<std::string> modelExtensions = { ".mesh" };
	const std::unordered_set<std::string> shaderExtensions = { ".shader" };
	const std::unordered_set<std::string> materialExtensions = { ".mat" };

	// Supported resource extensions
	std::unordered_set<std::string> supportedResourceExtensions;

	ResourceManager() {
		supportedResourceExtensions.insert(textureExtensions.begin(), textureExtensions.end());
		supportedResourceExtensions.insert(audioExtensions.begin(), audioExtensions.end());
		supportedResourceExtensions.insert(fontExtensions.begin(), fontExtensions.end());
		supportedResourceExtensions.insert(modelExtensions.begin(), modelExtensions.end());
		supportedResourceExtensions.insert(shaderExtensions.begin(), shaderExtensions.end());
		supportedResourceExtensions.insert(materialExtensions.begin(), materialExtensions.end());
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
	std::shared_ptr<T> LoadResource(const GUID_128& guid, const std::string& resourcePath, const std::string& assetPath, bool reload = false) {
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[ResourceManager] Attempting to load resource: %s", assetPath.c_str());
#endif
		std::shared_ptr<T> resource;
		if (!reload) {
			resource = std::make_shared<T>();
			ENGINE_LOG_DEBUG("resource->LoadResource(resourcePath, assetPath)");
			if (resource->LoadResource(resourcePath)) {
				auto& resourceMap = GetResourceMap<T>();
				resourceMap[guid] = resource;
				ENGINE_LOG_DEBUG("[ResourceManager] Loaded resource for: " + resourcePath);
#ifdef ANDROID
			__android_log_print(ANDROID_LOG_INFO, "GAM300", "[ResourceManager] Successfully loaded resource: %s", assetPath.c_str());
#endif
				return resource;
			}
		}
		else {
			resource = GetResource<T>(assetPath);
			if (resource->ReloadResource(resourcePath)) {
				auto& resourceMap = GetResourceMap<T>();
				resourceMap[guid] = resource;
				std::cout << "[ResourceManager] Reloaded resource for: " << resourcePath << std::endl;
				return resource;
			}
		}

		std::cerr << "[ResourceManager] ERROR: Failed to load resource: " << resourcePath << std::endl;
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ResourceManager] ERROR: Failed to load resource: %s", assetPath.c_str());
#endif
		return nullptr;
	}

	std::shared_ptr<Font> LoadFontResource(const GUID_128& guid, const std::string& resourcePath, const std::string& assetPath, unsigned int fontSize, bool reload = false) {
		std::shared_ptr<Font> font;
		if (!reload) {
			font = std::make_shared<Font>();
			if (font->LoadResource(resourcePath, fontSize)) {
				auto& resourceMap = GetResourceMap<Font>();
				resourceMap[guid] = font;
				std::cout << "[ResourceManager] Loaded resource for: " << resourcePath << std::endl;
				return font;
			}
		}
		else {
			font = GetFontResource(assetPath);
			if (font->ReloadResource(resourcePath)) {
				auto& resourceMap = GetResourceMap<Font>();
				resourceMap[guid] = font;
				std::cout << "[ResourceManager] Reloaded resource for: " << resourcePath << std::endl;
				return font;
			}
		}

		std::cerr << "[ResourceManager] ERROR: Failed to load resource: " << resourcePath << std::endl;
		return nullptr;
	}
};