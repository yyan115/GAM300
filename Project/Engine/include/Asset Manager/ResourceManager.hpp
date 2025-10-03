#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>
#include "Utilities/GUID.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Graphics/TextRendering/Font.hpp"
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
	std::shared_ptr<T> GetResourceFromGUID(const GUID_128& guid, const std::string& assetPath) {
		auto& resourceMap = GetResourceMap<T>();
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			return it->second;
		}
		else {
			return GetResource<T>(assetPath);
		}
	}

	std::shared_ptr<Font> GetFontResourceFromGUID(const GUID_128& guid, const std::string& assetPath, unsigned int fontSize) {
		auto& resourceMap = GetResourceMap<Font>();
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			return it->second;
		}
		else {
			return GetFontResource(assetPath, fontSize);
		}
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
		std::string resourcePath = MetaFilesManager::GetResourceNameFromAssetFile(assetPath);
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
		std::string resourcePath = MetaFilesManager::GetResourceNameFromAssetFile(assetPath);
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
	bool UnloadResource(GUID_128 guid, const std::string& resourcePath) {
		// Implementation for unloading the resource
		auto& resourceMap = GetResourceMap<T>();
		auto it = resourceMap.find(guid);
		if (it != resourceMap.end()) {
			resourceMap.erase(it);
			ENGINE_PRINT("[ResourceManager] Removed from resource map: ", resourcePath, "\n");
		}

		if (FileUtilities::RemoveFile(resourcePath)) {
			ENGINE_PRINT("[ResourceManager] Deleted resource file: ", resourcePath, "\n");
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
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ResourceManager] ERROR: Failed to unload resource: ", resourcePath, "\n");
		return false;
	}

	bool UnloadResource(GUID_128 guid, const std::string& resourcePath) {
		std::filesystem::path p(resourcePath);
		std::string extension = p.extension().string();
		if (textureExtensions.find(extension) != textureExtensions.end()) {
			return UnloadResource<Texture>(guid, resourcePath);
		}
		else if (fontExtensions.find(extension) != fontExtensions.end()) {
			return UnloadResource<Font>(guid, resourcePath);
		}
		else if (modelExtensions.find(extension) != modelExtensions.end()) {
			return UnloadResource<Model>(guid, resourcePath);
		}
		else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
			return UnloadResource<Shader>(guid, resourcePath);
		}
		else if (audioExtensions.find(extension) != audioExtensions.end()) {
			return UnloadResource<Audio>(guid, resourcePath);
		}
		else {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ResourceManager] ERROR: Trying to unload unsupported resource extension: ", extension, "\n");
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

	template <typename T>
	std::shared_ptr<T> LoadFromMeta(const GUID_128& guid,
		const std::string& resourcePath,
		const std::string& assetPath,
		bool reload = false)
	{
		return LoadResource<T>(guid, resourcePath, assetPath, reload);
	}

private:
	// Supported resource extensions
	const std::unordered_set<std::string> textureExtensions = { ".dds"};
	const std::unordered_set<std::string> audioExtensions = { ".wav", ".ogg" };
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
	std::shared_ptr<T> LoadResource(const GUID_128& guid, const std::string& resourcePath, const std::string& assetPath, bool reload = false) {
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[ResourceManager] Attempting to load resource: %s", assetPath.c_str());
#endif
		std::shared_ptr<T> resource;
		if (!reload) {
			resource = std::make_shared<T>();
			ENGINE_LOG_DEBUG("resource->LoadResource(resourcePath, assetPath)");
			if (resource->LoadResource(resourcePath, assetPath)) {
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
			if (resource->ReloadResource(resourcePath, assetPath)) {
				auto& resourceMap = GetResourceMap<T>();
				resourceMap[guid] = resource;
				ENGINE_PRINT("[ResourceManager] Reloaded resource for: ", resourcePath, "\n");
				return resource;
			}
		}

		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ResourceManager] ERROR: Failed to load resource: ", resourcePath, "\n");
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ResourceManager] ERROR: Failed to load resource: %s", assetPath.c_str());
#endif
		return nullptr;
	}

	std::shared_ptr<Font> LoadFontResource(const GUID_128& guid, const std::string& resourcePath, const std::string& assetPath, unsigned int fontSize, bool reload = false) {
		std::shared_ptr<Font> font;
		if (!reload) {
			font = std::make_shared<Font>();
			if (font->LoadResource(resourcePath, assetPath, fontSize)) {
				auto& resourceMap = GetResourceMap<Font>();
				resourceMap[guid] = font;
				ENGINE_PRINT("[ResourceManager] Loaded resource for: ", resourcePath, "\n");
				return font;
			}
		}
		else {
			font = GetFontResource(assetPath);
			if (font->ReloadResource(resourcePath, assetPath)) {
				auto& resourceMap = GetResourceMap<Font>();
				resourceMap[guid] = font;
				ENGINE_PRINT("[ResourceManager] Reloaded resource for: ", resourcePath, "\n");
				return font;
			}
		}
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ResourceManager] ERROR: Failed to load resource: ", resourcePath, "\n");
		return nullptr;
	}
};