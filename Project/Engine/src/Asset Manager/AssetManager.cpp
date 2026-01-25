#include "pch.h"
#include "Asset Manager/AssetManager.hpp"
#include "Sound/Audio.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>

AssetManager& AssetManager::GetInstance() {
    static AssetManager instance; // lives only in the DLL
    return instance;
}

void AssetManager::AddToEventQueue(AssetManager::Event event, const std::filesystem::path& assetPath) {
	if (androidCompilationStatus.isCompiling) {
		return;
	}
	assetEventQueue.emplace_back(std::make_pair(event, assetPath));
}

void AssetManager::RunEventQueue() {
    if (!assetEventQueue.empty()) {
        auto currentEvent = assetEventQueue.front();
        assetEventQueue.pop_front();

		switch (currentEvent.first) {
		case Event::added: {
			if (previousEvent.first == currentEvent.first && previousEvent.second == currentEvent.second) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "[AssetManager] Running event queue... Asset ADDED: " << currentEvent.second.generic_string() << ". Compiling asset..." << std::endl;
			CompileAsset(currentEvent.second.generic_string(), true);
			break;
		}
		case Event::modified: {
			if (previousEvent.first == currentEvent.first && previousEvent.second == currentEvent.second) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "[AssetManager] Running event queue... Asset MODIFIED: " << currentEvent.second.generic_string() << ". Re-compiling asset..." << std::endl;
			CompileAsset(currentEvent.second.generic_string(), true);
			break;
		}
		case Event::removed: {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "[AssetManager] Running event queue... Asset REMOVED: " << currentEvent.second.generic_string() << ". Checking for any add event for the same asset ahead..." << std::endl;
			bool unloadAsset = true;
			for (const auto& nextEvent : assetEventQueue) {
				// If the next event is adding back the same asset (e.g. replacement of assets).
				if (nextEvent.second == currentEvent.second && nextEvent.first == Event::added) {
					// Don't unload the current asset as it will be replaced next.
					std::cout << "[AssetManager] Running event queue... FOUND A SUBSEQUENT ADD EVENT: " << nextEvent.second.generic_string() << ". Asset won't be unloaded." << std::endl;
					unloadAsset = false;
					break;
				}
			}

			if (unloadAsset) {
				UnloadAsset(currentEvent.second.generic_string());
			}
			break;
		}
		case Event::renamed_old:
			std::cout << "[AssetManager] Running event queue... Asset RENAMED (OLD): " << currentEvent.second.generic_string() << ". Unloading asset..." << std::endl;
			UnloadAsset(currentEvent.second.generic_string());
			break;
		case Event::renamed_new:
			std::cout << "[AssetManager] Running event queue... Asset RENAMED (NEW): " << currentEvent.second.generic_string() << ". Compiling asset..." << std::endl;
			CompileAsset(currentEvent.second.generic_string(), true);
			break;
		default:
			break;
		}

		previousEvent = currentEvent;
		previousEventTime = std::chrono::steady_clock::now();
    }

	if (std::chrono::steady_clock::now() - previousEventTime > std::chrono::milliseconds(500)) {
		previousEvent.second = std::filesystem::path{};
	}
}
const std::filesystem::path& AssetManager::GetAndroidResourcesPath() {
	if (canonicalAndroidResourcesPath.empty()) {
		canonicalAndroidResourcesPath = std::filesystem::absolute(androidResourcesPath);
	}

	return canonicalAndroidResourcesPath;
}

std::string AssetManager::ExtractRelativeAndroidPath(const std::string& fullAndroidPath) {
	std::string relativePath{};
	return fullAndroidPath.substr(canonicalAndroidResourcesPath.generic_string().length() + 1);
}

int AssetManager::GetAssetMetaMapSize() {
	return static_cast<int>(assetMetaMap.size());
}

std::shared_ptr<AssetMeta> AssetManager::AddAssetMetaToMap(const std::string& assetPath) {
	ENGINE_LOG_INFO("AddAssetMetaToMap");
	std::filesystem::path p(assetPath);
	std::string extension = p.extension().string();
	std::string metaFilePath = assetPath + ".meta";
	std::shared_ptr<AssetMeta> assetMeta;
	if (textureExtensions.find(extension) != textureExtensions.end()) {
		assetMeta = std::make_shared<TextureMeta>();
		assetMeta->PopulateAssetMetaFromFile(metaFilePath);
	}
	else if (modelExtensions.find(extension) != modelExtensions.end()) {
		assetMeta = std::make_shared<ModelMeta>();
		assetMeta->PopulateAssetMetaFromFile(metaFilePath);
	}
	else {
		assetMeta = std::make_shared<AssetMeta>();
		assetMeta->PopulateAssetMetaFromFile(metaFilePath);
	}

//#ifndef ANDROID
//	// Check if the compiled resource file exists. If not, we need to recompile the asset.
//	if (!std::filesystem::exists(assetMeta->compiledFilePath)) {
//		std::cout << "[AssetManager] WARNING: Compiled resource file missing for asset: " << assetPath << ". Recompiling..." << std::endl;
//		CompileAsset(assetPath);
//	}
//#endif

	assetMetaMap[assetMeta->guid] = assetMeta;
	return assetMeta;
}

bool AssetManager::CompileAsset(const std::string& filePathStr, bool forceCompile, bool forAndroid) {
	std::filesystem::path filePathObj(filePathStr);
	std::string extension = filePathObj.extension().string();
	if (textureExtensions.find(extension) != textureExtensions.end()) {
		return CompileTexture(filePathStr, "diffuse", -1, true, forceCompile, forAndroid);
	}
	else if (audioExtensions.find(extension) != audioExtensions.end()) {
		return CompileAsset<Audio>(filePathStr, forceCompile, forAndroid);
	}
	else if (fontExtensions.find(extension) != fontExtensions.end()) {
		return CompileAsset<Font>(filePathStr, forceCompile, forAndroid);
	}
	else if (modelExtensions.find(extension) != modelExtensions.end()) {
		return CompileAsset<Model>(filePathStr, forceCompile, forAndroid);
	}
	else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
		return CompileAsset<Shader>(filePathStr, forceCompile, forAndroid);
	}
	else if (materialExtensions.find(extension) != materialExtensions.end()) {
		return CompileAsset<Material>(filePathStr, forceCompile, forAndroid);
	}
	else if (scriptExtensions.find(extension) != scriptExtensions.end()) {
		return CompileAsset<Script>(filePathStr, forceCompile, forAndroid);
	}
	else {
		std::cerr << "[AssetManager] ERROR: Attempting to compile unsupported asset extension: " << extension << std::endl;
		return false;
	}
}

bool AssetManager::CompileAsset(std::shared_ptr<AssetMeta> assetMeta, bool forceCompile, bool forAndroid) {
	std::filesystem::path filePathObj(assetMeta->sourceFilePath);
	std::string extension = filePathObj.extension().string();
	if (textureExtensions.find(extension) != textureExtensions.end()) {
		std::shared_ptr<TextureMeta> textureMeta = std::static_pointer_cast<TextureMeta>(assetMeta);
		return CompileTexture(assetMeta->sourceFilePath, textureMeta, forceCompile, forAndroid);
	}
	else if (audioExtensions.find(extension) != audioExtensions.end()) {
		return CompileAsset<Audio>(assetMeta->sourceFilePath, forceCompile, forAndroid, assetMeta);
	}
	else if (fontExtensions.find(extension) != fontExtensions.end()) {
		return CompileAsset<Font>(assetMeta->sourceFilePath, forceCompile, forAndroid, assetMeta);
	}
	else if (modelExtensions.find(extension) != modelExtensions.end()) {
		return CompileAsset<Model>(assetMeta->sourceFilePath, forceCompile, forAndroid, assetMeta);
	}
	else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
		return CompileAsset<Shader>(assetMeta->sourceFilePath, forceCompile, forAndroid, assetMeta);
	}
	else if (materialExtensions.find(extension) != materialExtensions.end()) {
		return CompileAsset<Material>(assetMeta->sourceFilePath, forceCompile, forAndroid, assetMeta);
	}
	else if (scriptExtensions.find(extension) != scriptExtensions.end()) {
		return CompileAsset<Script>(assetMeta->sourceFilePath, forceCompile, forAndroid, assetMeta);
	}
	else {
		std::cerr << "[AssetManager] ERROR: Attempting to compile unsupported asset extension: " << extension << std::endl;
		return false;
	}
}

bool AssetManager::CompileTexture(const std::string& filePath, const std::string& texType, GLint slot, bool flipUVs, bool forceCompile, bool forAndroid) {
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
			return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot, flipUVs, forceCompile, forAndroid);
		}
	}
	else {
		return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot, flipUVs, forceCompile, forAndroid);
	}
}

bool AssetManager::CompileTexture(const std::string& filePath, std::shared_ptr<TextureMeta> textureMeta, bool forceCompile, bool forAndroid) {
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
			return CompileTextureToResource(guid, filePath.c_str(), textureMeta, forceCompile, forAndroid);
		}
	}
	else {
		return CompileTextureToResource(guid, filePath.c_str(), textureMeta, forceCompile, forAndroid);
	}
}

bool AssetManager::CompileUpdatedMaterial(const std::string& filePath, std::shared_ptr<Material> material, bool forceCompile, bool forAndroid) {
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
			return CompileUpdatedMaterialToResource(guid, filePath, material, forceCompile, forAndroid);
		}
	}
	else {
		return CompileUpdatedMaterialToResource(guid, filePath, material, forceCompile, forAndroid);
	}
}

bool AssetManager::IsAssetCompiled(GUID_128 guid) {
	return assetMetaMap.find(guid) != assetMetaMap.end();
}

bool AssetManager::IsAssetCompiled(const std::string& assetPath) {
	for (const auto& pair : assetMetaMap) {
		if (pair.second->sourceFilePath == assetPath) {
			return true;
		}
	}
	return false;
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
			ResourceManager::GetInstance().UnloadResource<Texture>(guid, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (audioExtensions.find(extension) != audioExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Audio>(guid, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (fontExtensions.find(extension) != fontExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Font>(guid, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (modelExtensions.find(extension) != modelExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Model>(guid, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Shader>(guid, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (materialExtensions.find(extension) != materialExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Material>(guid, it->second->compiledFilePath);
		}
		else if (scriptExtensions.find(extension) != shaderExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Script>(guid, it->second->compiledFilePath);
		}
		else {
			std::cerr << "[AssetManager] ERROR: Trying to unload unsupported asset extension: " << extension << std::endl;
		}

		assetMetaMap.erase(it);
		std::cout << "[AssetManager] Unloaded asset: " << assetPath << std::endl << std::endl;
	}
}

GUID_128 AssetManager::GetGUID128FromAssetMeta(const std::string& assetPath) {
	// Helper lambda to normalize a path: extract from "Resources" onwards and use forward slashes
	auto normalizePath = [](const std::string& path) -> std::string {
		size_t resPos = path.find("Resources");
		if (resPos == std::string::npos) {
			return "";  // "Resources" not found
		}
		std::string relative = path.substr(resPos);
		// Normalize separators to forward slashes
		std::replace(relative.begin(), relative.end(), '\\', '/');
		return relative;
	};

	std::string normalizedAssetPath = normalizePath(assetPath);
	if (normalizedAssetPath.empty()) {
		std::cerr << "[AssetManager] ERROR: Path does not contain 'Resources': " << assetPath << std::endl;
		return GUID_128{};
	}

	for (const auto& pair : assetMetaMap) {
		std::string normalizedSourcePath = normalizePath(pair.second->sourceFilePath);
		if (!normalizedSourcePath.empty() && normalizedSourcePath == normalizedAssetPath) {
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
	return nullptr;
}

void AssetManager::InitializeSupportedExtensions() {
	supportedAssetExtensions.insert(textureExtensions.begin(), textureExtensions.end());
	supportedAssetExtensions.insert(audioExtensions.begin(), audioExtensions.end());
	supportedAssetExtensions.insert(fontExtensions.begin(), fontExtensions.end());
	supportedAssetExtensions.insert(modelExtensions.begin(), modelExtensions.end());
	supportedAssetExtensions.insert(shaderExtensions.begin(), shaderExtensions.end());
	supportedAssetExtensions.insert(materialExtensions.begin(), materialExtensions.end());
	supportedAssetExtensions.insert(scriptExtensions.begin(), scriptExtensions.end());
}

std::unordered_set<std::string>& AssetManager::GetSupportedExtensions() {
	return supportedAssetExtensions;
}

const std::unordered_set<std::string>& AssetManager::GetShaderExtensions() const {
	return shaderExtensions;
}

bool AssetManager::IsAssetExtensionSupported(const std::string& extension) const {
	//for (auto& supported : supportedAssetExtensions) {
	//	ENGINE_LOG_INFO("Extension: " + extension);
	//	ENGINE_LOG_INFO("Supported: " + supported);
	//}

	return supportedAssetExtensions.find(extension) != supportedAssetExtensions.end();
}

bool AssetManager::IsExtensionMetaFile(const std::string& extension) const {
	return extension == ".meta";
}

bool AssetManager::IsExtensionShaderVertFrag(const std::string& extension) const {
	return shaderExtensions.find(extension) != shaderExtensions.end();
}

bool AssetManager::IsExtensionTexture(const std::string& extension) const {
	return textureExtensions.find(extension) != textureExtensions.end();
}

bool AssetManager::IsExtensionMaterial(const std::string& extension) const {
	return materialExtensions.find(extension) != materialExtensions.end();
}

bool AssetManager::HandleMetaFileDeletion(const std::string& metaFilePath) {
	std::string assetFilePath = metaFilePath.substr(0, metaFilePath.size() - 5); // Remove ".meta"
	bool failedToUnload = false;
	for (const auto& pair : assetMetaMap) {
		if (pair.second->sourceFilePath == assetFilePath) {
			std::string resourcePath = pair.second->compiledFilePath;
			GUID_128 guid = pair.first;

			if (ResourceManager::GetInstance().UnloadResource(guid, resourcePath)) {
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
			GUID_128 guid = pair.first;

			if (ResourceManager::GetInstance().UnloadResource(guid, resourcePath)) {
				std::cout << "[AssetManager] Successfully deleted resource file: " << resourcePath << std::endl;
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

std::string AssetManager::GetAssetPathFromGUID(const GUID_128 guid) {
	if (guid == GUID_128{}) return "";

	auto it = assetMetaMap.find(guid);
	if (it != assetMetaMap.end()) {
#ifndef ANDROID
		return it->second->sourceFilePath;
#else
		std::string path = it->second->sourceFilePath;
		// Strip ../../ prefix and get path starting from "Resources"
		size_t resourcesPos = path.find("Resources");
		if (resourcesPos != std::string::npos) {
			path = path.substr(resourcesPos);
		}
		path = FileUtilities::SanitizePathForAndroid(std::filesystem::path(path)).generic_string();
		return path;
#endif
	}

	ENGINE_LOG_ERROR("[AssetManager] ERROR: Asset meta with GUID " + GUIDUtilities::ConvertGUID128ToString(guid) + " not found.");
	return "";
}

std::vector<std::string> AssetManager::CompileAllAssetsForAndroid() {
	// THIS RUNS ON A SEPARATE THREAD.
	std::cout << "[AssetManager] Starting compile all assets for Android..." << std::endl;
	androidCompilationStatus.numCompiledAssets = 0;
	androidCompilationStatus.isCompiling = true;
	std::vector<std::string> remainingPaths{};
	for (const auto& pair : assetMetaMap) {
		std::filesystem::path p(pair.second->compiledFilePath);
		std::string assetPath{};
		if (ResourceManager::GetInstance().IsExtensionShader(p.extension().generic_string())) {
			// Skip compiling shaders because they must be compiled on the main OpenGL thread.
			assetPath = pair.second->sourceFilePath + ".vert";
			remainingPaths.push_back(assetPath);
			continue;
		}
		else if (ResourceManager::GetInstance().IsExtensionMesh(p.extension().generic_string())) {
			// Skip compiling meshes because they must be compiled on the main OpenGL thread.
			assetPath = pair.second->sourceFilePath;
			remainingPaths.push_back(assetPath);
			continue;
		}
		else if (IsExtensionMaterial(p.extension().generic_string())) {
			assetPath = pair.second->sourceFilePath;
			auto material = ResourceManager::GetInstance().GetResource<Material>(assetPath);
			CompileUpdatedMaterial(assetPath, material, true, true);
		}
		else {
			CompileAsset(pair.second, true, true);
			++androidCompilationStatus.numCompiledAssets;
		}
	}

	// Copy scenes to Android resources.
	if (std::filesystem::exists("../../Resources/Scenes")) {
		for (auto p : std::filesystem::recursive_directory_iterator(rootAssetDirectory + "/Scenes")) {
			std::string extension = p.path().extension().generic_string();
			if (std::filesystem::is_regular_file(p)) {
				std::string path = p.path().generic_string();
				path = path.substr(path.find("Resources"));
				std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(path));
				path = newPath.generic_string();
				if (FileUtilities::CopyFile(p.path().generic_string(), (AssetManager::GetInstance().GetAndroidResourcesPath() / path).generic_string())) {
					ENGINE_LOG_INFO("Copied scene file to Android Resources: " + p.path().generic_string());
				}
			}
		}
	}

	// Copy config files to Android resources.
	if (std::filesystem::exists(rootAssetDirectory + "/Configs")) {
		for (auto p : std::filesystem::recursive_directory_iterator(rootAssetDirectory + "/Configs")) {
			if (std::filesystem::is_regular_file(p)) {
				std::string path = p.path().generic_string();
				path = path.substr(path.find("Resources"));
				std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(path));
				path = newPath.generic_string();
				if (FileUtilities::CopyFile(p.path().generic_string(), (AssetManager::GetInstance().GetAndroidResourcesPath() / path).generic_string())) {
					ENGINE_LOG_INFO("Copied config file to Android Resources: " + p.path().generic_string());
				}
			}
		}
	}

	// Output the asset manifest for Android.
	std::filesystem::path manifestFileP(GetAndroidResourcesPath() / "asset_manifest.txt");
	std::ofstream out(manifestFileP.generic_string());
	if (!out.is_open()) {
		std::cerr << "[AssetManager] Failed to open manifest file for writing\n";
		return std::vector<std::string>{};
	}
	for (auto& p : std::filesystem::recursive_directory_iterator(rootAssetDirectory)) {
		// Only copy the file if it is one of the recognised asset file extensions.
		std::string extension = p.path().extension().generic_string();
		if (std::filesystem::is_regular_file(p) && (IsAssetExtensionSupported(extension) ||
			ResourceManager::GetInstance().IsResourceExtensionSupported(extension) || IsExtensionMetaFile(extension)))
		{
			std::string path = p.path().generic_string();
			path = path.substr(path.find("Resources"));
			std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(path));
			path = newPath.generic_string();
			out << path << "\n";
		}
	}

	// Add config files to manifest
	if (std::filesystem::exists(rootAssetDirectory + "/Configs")) {
		for (auto& p : std::filesystem::recursive_directory_iterator(rootAssetDirectory + "/Configs")) {
			if (std::filesystem::is_regular_file(p)) {
				std::string path = p.path().generic_string();
				path = path.substr(path.find("Resources"));
				std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(path));
				path = newPath.generic_string();
				out << path << "\n";
			}
		}
	}

	std::cout << "[AssetManager] Asset manifest written to " << manifestFileP.generic_string() << std::endl;
	std::cout << "[AssetManager] Finished compiling assets except Shaders and Meshes for Android. Android Resources folder is in GAM300/AndroidProject/app/src/main/assets/Resources" << std::endl << std::endl;
	androidCompilationStatus.finishedCompiling = true;
	return remainingPaths;
}

std::vector<std::string> AssetManager::CompileAllAssetsForDesktop() {
	// THIS RUNS ON A SEPARATE THREAD.
	std::cout << "[AssetManager] Starting compile all assets for Desktop..." << std::endl;
	std::vector<std::string> remainingPaths{};
	for (const auto& pair : assetMetaMap) {
		std::filesystem::path p(pair.second->compiledFilePath);
		std::string assetPath{};
		if (ResourceManager::GetInstance().IsExtensionShader(p.extension().generic_string())) {
			// Skip compiling shaders because they must be compiled on the main OpenGL thread.
			assetPath = pair.second->sourceFilePath + ".vert";
			remainingPaths.push_back(assetPath);
			continue;
		}
		else if (ResourceManager::GetInstance().IsExtensionMesh(p.extension().generic_string())) {
			// SKip compiling meshes because they must be compiled on the main OpenGL thread.
			remainingPaths.push_back(assetPath);
			continue;
		}
		else {
			assetPath = pair.second->sourceFilePath;
			CompileAsset(assetPath, true);
		}
	}

	//// Copy scenes to resources.
	//if (std::filesystem::exists(rootAssetFolder + "/Scenes")) {
	//	for (auto p : std::filesystem::recursive_directory_iterator(rootAssetFolder + "/Scenes")) {
	//		if (std::filesystem::is_regular_file(p)) {
	//			if (FileUtilities::CopyFile(p.path().generic_string(), (FileUtilities::GetSolutionRootDir() / p.path()).generic_string())) {
	//				ENGINE_LOG_INFO("Copied scene file to Project/Resources: " + p.path().generic_string());
	//			}
	//		}
	//	}
	//}

	std::cout << "[AssetManager] Finished compiling assets except Shaders and Meshes for Desktop." << std::endl << std::endl;
	return remainingPaths;
}

void AssetManager::SetRootAssetDirectory(const std::string& _rootAssetsFolder) {
	rootAssetDirectory = _rootAssetsFolder;
}

std::string AssetManager::GetRootAssetDirectory() const {
	return rootAssetDirectory;
}

std::string AssetManager::GetAssetPathFromAssetName(const std::string& assetName) {
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: Platform not available for asset discovery!", "\n");
		return "";
	}
	std::vector<std::string> assetFiles = platform->ListAssets(rootAssetDirectory, true);
	for (std::string assetPath : assetFiles) {
		std::filesystem::path p(assetPath);
		std::string currentAssetName = (p.stem().generic_string() + p.extension().generic_string());
		if (currentAssetName == assetName) {
			return assetPath;
		}
	}

	return "";
}

bool AssetManager::CompileTextureToResource(GUID_128 guid, const char* filePath, const char* texType, GLint slot, bool flipUVs, bool forceCompile, bool forAndroid) {
	// If the asset is not already loaded, load and store it using the GUID.
	if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
		Texture texture{};
		texture.metaData->type = texType;
		texture.metaData->flipUVs = flipUVs;
		texture.unit = slot;

		std::string compiledPath = texture.CompileToResource(filePath, forAndroid);
		if (compiledPath.empty()) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetManager] ERROR: Failed to compile asset: ", filePath, "\n");
			return false;
		}

		std::shared_ptr<AssetMeta> assetMeta;
		if (!forAndroid) {
			assetMeta = texture.GenerateBaseMetaFile(guid, filePath, compiledPath);
		}
		else {
			assetMeta = assetMetaMap.find(guid)->second;
			assetMeta = texture.GenerateBaseMetaFile(guid, filePath, assetMeta->compiledFilePath, compiledPath, true);
		}
		assetMeta = texture.ExtendMetaFile(filePath, assetMeta, forAndroid);
		assetMetaMap[guid] = assetMeta;
		ENGINE_PRINT("[AssetManager] Compiled asset: ", filePath, " to ", compiledPath, "\n\n");

		if (!forAndroid) {
			// If the resource is already loaded, hot-reload the resource.
			if (ResourceManager::GetInstance().IsResourceLoaded(guid)) {
				ResourceManager::GetInstance().GetResource<Texture>(filePath, true);
			}
		}

		return true;
	}

	return true;
}

bool AssetManager::CompileTextureToResource(GUID_128 guid, const char* filePath, std::shared_ptr<TextureMeta> textureMeta, bool forceCompile, bool forAndroid) {
	// If the asset is not already loaded, load and store it using the GUID.
	if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
		Texture texture{ textureMeta };
		std::string compiledPath = texture.CompileToResource(filePath, forAndroid);
		if (compiledPath.empty()) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetManager] ERROR: Failed to compile asset: ", filePath, "\n");
			return false;
		}

		std::shared_ptr<AssetMeta> assetMeta;
		if (!forAndroid) {
			assetMeta = texture.GenerateBaseMetaFile(guid, filePath, compiledPath);
		}
		else {
			assetMeta = assetMetaMap.find(guid)->second;
			assetMeta = texture.GenerateBaseMetaFile(guid, filePath, assetMeta->compiledFilePath, compiledPath, true);
		}
		assetMeta = texture.ExtendMetaFile(filePath, assetMeta, forAndroid);
		assetMetaMap[guid] = assetMeta;
		ENGINE_PRINT("[AssetManager] Compiled asset: ", filePath, " to ", compiledPath, "\n\n");

		if (!forAndroid) {
			// If the resource is already loaded, hot-reload the resource.
			if (ResourceManager::GetInstance().IsResourceLoaded(guid)) {
				ResourceManager::GetInstance().GetResource<Texture>(filePath, true);
			}
		}

		return true;
	}

	return true;
}

bool AssetManager::CompileUpdatedMaterialToResource(GUID_128 guid, const std::string& filePath, std::shared_ptr<Material> material, bool forceCompile, bool forAndroid) {
	// If the asset is not already loaded, load and store it using the GUID.
	if (forceCompile || assetMetaMap.find(guid) == assetMetaMap.end()) {
		std::string compiledPath = material->CompileUpdatedAssetToResource(filePath, forAndroid);
		if (compiledPath.empty()) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AssetManager] ERROR: Failed to compile updated material: ", filePath, "\n");
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		std::shared_ptr<AssetMeta> assetMeta;
		if (!forAndroid) {
			assetMeta = material->GenerateBaseMetaFile(guid, filePath, compiledPath);
		}
		else {
			assetMeta = assetMetaMap.find(guid)->second;
			assetMeta = material->GenerateBaseMetaFile(guid, filePath, assetMeta->compiledFilePath, compiledPath, true);
		}
		assetMetaMap[guid] = assetMeta;
		ENGINE_PRINT("[AssetManager] Compiled updated material: ", filePath, " to ", compiledPath, "\n\n");

		// If the resource is already loaded, hot-reload the resource.
		if (ResourceManager::GetInstance().IsResourceLoaded(guid)) {
			ResourceManager::GetInstance().GetResource<Texture>(filePath, true);
		}

		return true;
	}

	return true;
}

//For Scripting
bool AssetManager::ReadTextFile(const std::string & path, std::string & outContent)
{
	std::ifstream file(path);
	if (!file.is_open())
		return false;

	std::ostringstream ss;
	ss << file.rdbuf();
	outContent = ss.str();
	return true;
}