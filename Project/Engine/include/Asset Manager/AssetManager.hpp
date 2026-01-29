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
#include "Asset Manager/ResourceManager.hpp"
#include "Sound/Audio.hpp"
#include "Script/Script.hpp"
#include <future>


#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
// Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

class AssetManager {
public:
	ENGINE_API static AssetManager& GetInstance();

	ENGINE_API bool CompileAsset(const std::string& filePathStr, bool forceCompile = false, bool forAndroid = false);
	ENGINE_API bool CompileAsset(std::shared_ptr<AssetMeta> assetMeta, bool forceCompile = false, bool forAndroid = false);
	std::shared_ptr<AssetMeta> AddAssetMetaToMap(const std::string& assetPath);

	/**
	 * \brief Compiles an asset of type T from the specified file path into a resource.
	 *	Handles the initial generation or retrieval of the asset's GUID and checks if compilation is necessary.
	 *	Note: This function is not to be used for compiling Texture assets. Use CompileTexture() instead.
	 *
	 * \tparam T The type of the asset to load.
	 * \param filePathStr The file path of the asset to load.
	 * \param forceCompile If true, forces recompilation of the asset even if it is already compiled.
	 * \param forAndroid If true, compiles the asset for Android platform.
	 * \param assetMeta Optional custom asset metadata to use during compilation.
	 * \return True if the asset was successfully compiled or already exists; false otherwise.
	 */
	template <typename T>
	bool CompileAsset(const std::string& filePathStr, bool forceCompile = false, bool forAndroid = false, std::shared_ptr<AssetMeta> assetMeta = nullptr) {
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
				if (!assetMeta)
					return CompileAssetToResource<T>(guid, filePath, forceCompile, forAndroid);
				else
					return CompileAssetToResource<T>(guid, filePath, forceCompile, forAndroid, assetMeta);
			}
		}
		else {
			if (!assetMeta)
				return CompileAssetToResource<T>(guid, filePath, forceCompile, forAndroid);
			else
				return CompileAssetToResource<T>(guid, filePath, forceCompile, forAndroid, assetMeta);
		}
	}

	bool CompileTexture(const std::string& filePath, const std::string& texType, GLint slot, bool flipUVs, bool forceCompile = false, bool forAndroid = false);
	ENGINE_API bool CompileTexture(const std::string& filePath, std::shared_ptr<TextureMeta> textureMeta, bool forceCompile = false, bool forAndroid = false);
	ENGINE_API bool CompileUpdatedMaterial(const std::string& filePath, std::shared_ptr<Material> material, bool forceCompile = false, bool forAndroid = false);

	bool IsAssetCompiled(GUID_128 guid);
	bool IsAssetCompiled(const std::string& assetPath);
	void ENGINE_API UnloadAsset(const std::string& assetPath);

	//void UnloadAllAssets() {
	//	assetMetaMap.clear();
	//}

	GUID_128 ENGINE_API GetGUID128FromAssetMeta(const std::string& assetPath);
	
	template <typename T>
	std::shared_ptr<T> LoadByGUID(const GUID_128& guid)
	{
		auto meta = GetAssetMeta(guid);
		if (!meta) return nullptr;

		// Ensure compiled resource exists (first touch)
		if (!std::filesystem::exists(meta->compiledFilePath)) {
			CompileAsset(meta->sourceFilePath, /*forceCompile=*/true);
		}

		// Load the resource using the paths from meta
		return ResourceManager::GetInstance().GetResourceFromMeta<T>(guid, meta->compiledFilePath, meta->sourceFilePath);
	}

	std::shared_ptr<AssetMeta> ENGINE_API GetAssetMeta(GUID_128 guid);

	void InitializeSupportedExtensions();
	std::unordered_set<std::string>& GetSupportedExtensions();
	const std::unordered_set<std::string>& GetShaderExtensions() const;
	bool ENGINE_API IsAssetExtensionSupported(const std::string& extension) const;
	bool ENGINE_API IsExtensionMetaFile(const std::string& extension) const;
	bool IsExtensionShaderVertFrag(const std::string& extension) const;
	bool ENGINE_API IsExtensionTexture(const std::string& extension) const;
	bool ENGINE_API IsExtensionMaterial(const std::string& extension) const;

	bool ENGINE_API HandleMetaFileDeletion(const std::string& metaFilePath);
	bool ENGINE_API HandleResourceFileDeletion(const std::string& resourcePath);

	bool ReadTextFile(const std::string& path, std::string& outContent);

	std::string ENGINE_API GetAssetPathFromGUID(const GUID_128 guid);
	std::vector<std::string> ENGINE_API CompileAllAssetsForAndroid();
	std::vector<std::string> CompileAllAssetsForDesktop();

	void SetRootAssetDirectory(const std::string& _rootAssetsFolder);
	std::string ENGINE_API GetRootAssetDirectory() const;
	std::string ENGINE_API GetAssetPathFromAssetName(const std::string& assetName);

	enum class Event {
		added,
		removed,
		modified,
		renamed_old,
		renamed_new
	};

	void ENGINE_API AddToEventQueue(AssetManager::Event event, const std::filesystem::path& assetPath);
	void ENGINE_API RunEventQueue();

	const std::filesystem::path& GetAndroidResourcesPath();
	std::string ExtractRelativeAndroidPath(const std::string& fullAndroidPath);

	// Handle 'Compile All Assets'
	std::future<std::vector<std::string>> desktopAssetCompilationFuture;
	struct AndroidCompilationStatus {
		bool isCompiling = false;
		bool finishedCompiling = false;
		std::future<std::vector<std::string>> assetCompilationFuture;
		int numCompiledAssets = 0;
	} androidCompilationStatus;

	int ENGINE_API GetAssetMetaMapSize();

private:
	std::unordered_map<GUID_128, std::shared_ptr<AssetMeta>> assetMetaMap;
	std::list<std::pair<AssetManager::Event, std::filesystem::path>> assetEventQueue;
	std::pair<AssetManager::Event, std::filesystem::path> previousEvent;
	std::chrono::steady_clock::time_point previousEventTime;
	
	std::string rootAssetDirectory;
	std::filesystem::path androidResourcesPath{ "../../../AndroidProject/app/src/main/assets" };
	std::filesystem::path canonicalAndroidResourcesPath;

	// Supported asset extensions
	const std::unordered_set<std::string> textureExtensions = { ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG", ".bmp", ".BMP" };
	const std::unordered_set<std::string> audioExtensions = { ".wav", ".ogg", ".mp3", ".flac"};
	const std::unordered_set<std::string> fontExtensions = { ".ttf" };
	const std::unordered_set<std::string> modelExtensions = { ".obj", ".fbx" };
	const std::unordered_set<std::string> shaderExtensions = { ".vert", ".frag" };
	const std::unordered_set<std::string> materialExtensions = { ".mat" };
	const std::unordered_set<std::string> scriptExtensions = { ".lua" };
	const std::unordered_set<std::string> textExtensions = { ".txt" };
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

	/**
	 * \brief Calls the asset's CompileToResource function to compile it into a resource file.
	 *	Then generates and stores the asset metadata in the assetMetaMap.
	 *	Also handles hot-reloading of the resource if it is already loaded.
	 *
	 * \tparam T The type of the asset to load.
	 * \param guid The generated GUID of the asset.
	 * \param filePath The file path of the asset to load.
	 * \param forceCompile If true, forces recompilation of the asset even if it is already compiled.
	 * \param forAndroid If true, compiles the asset for Android platform.
	 * \param assetMeta Optional custom asset metadata to use during compilation.
	 * \return True if the asset was successfully compiled or already exists; false otherwise.
	 */
	template <typename T>
	bool CompileAssetToResource(GUID_128 guid,
		const std::string& filePath,
		bool forceCompile = false,
		bool forAndroid = false,
		std::shared_ptr<AssetMeta> assetMetaParam = nullptr)
	{
		// Short-circuit if not forced and already present
		if (!forceCompile && assetMetaMap.find(guid) != assetMetaMap.end()) {
			return true;
		}

		// Create asset (use provided meta if any)
		std::shared_ptr<T> asset;
		if (!assetMetaParam) asset = std::make_shared<T>();
		else asset = std::make_shared<T>(assetMetaParam);

		// Compile
		std::string compiledPath = asset->CompileToResource(filePath, forAndroid);
		if (compiledPath.empty()) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetManager] ERROR: Failed to compile asset: ", filePath);
			return false;
		}

		// Create/update meta safely
		std::shared_ptr<AssetMeta> metaPtr;
		if (!forAndroid) {
			metaPtr = asset->GenerateBaseMetaFile(guid, filePath, compiledPath);
		}
		else {
			auto it = assetMetaMap.find(guid);
			if (it != assetMetaMap.end()) {
				metaPtr = asset->GenerateBaseMetaFile(guid, filePath, it->second->compiledFilePath, compiledPath, true);
			}
			else {
				metaPtr = asset->GenerateBaseMetaFile(guid, filePath, compiledPath);
			}
		}

		metaPtr = asset->ExtendMetaFile(filePath, metaPtr, forAndroid);
		assetMetaMap[guid] = metaPtr;

		ENGINE_PRINT("[AssetManager] Compiled asset: ", filePath, " to ", compiledPath, "\n\n");

		// HOT-RELOAD (typed)
		if (!forAndroid) {
			if (ResourceManager::GetInstance().IsResourceLoaded(guid)) {
				ENGINE_PRINT("[AssetManager] Resource is already loaded - hot-reloading the resource: ", compiledPath);

				// Special-case APIs that require a different call
				if constexpr (std::is_same_v<T, Font>) {
					// If GetFontResource expects a resource path, pass compiledPath.
					ResourceManager::GetInstance().GetFontResource(compiledPath, 0, true);
				}
				else if constexpr (std::is_same_v<T, Shader>) {
					ResourceManager::GetInstance().GetResource<Shader>(filePath, true);
				}
				else {
					// For model/material/audio/script etc., call typed GetResource<T>.
					// Some projects store assets keyed by source path; if your ResourceManager expects
					// compiledPath adjust accordingly.
					ResourceManager::GetInstance().GetResource<T>(compiledPath, true);
				}
			}
		}

		return true;
	}



	bool CompileTextureToResource(GUID_128 guid, const char* filePath, const char* texType, GLint slot, bool flipUVs, bool forceCompile = false, bool forAndroid = false);
	bool CompileTextureToResource(GUID_128 guid, const char* filePath, std::shared_ptr<TextureMeta> textureMeta, bool forceCompile = false, bool forAndroid = false);
	
	bool CompileUpdatedMaterialToResource(GUID_128 guid, const std::string& filePath, std::shared_ptr<Material> material, bool forceCompile = false, bool forAndroid = false);
};