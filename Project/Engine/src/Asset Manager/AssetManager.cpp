#include "pch.h"
#include "Asset Manager/AssetManager.hpp"

AssetManager& AssetManager::GetInstance() {
    static AssetManager instance; // lives only in the DLL
    return instance;
}

void AssetManager::AddToEventQueue(AssetManager::Event event, const std::filesystem::path& assetPath) {
	assetEventQueue.emplace_back(std::make_pair(event, assetPath));
	//using namespace std::chrono;
	//auto now = steady_clock::now();
	//auto it = compilationEvents.find(assetPath.generic_string());
	//if (it != compilationEvents.end()) {
	//	// If the duration between the previous compilation event for the same asset is too soon, don't add to the queue.
	//	if (duration_cast<milliseconds>(now - it->second).count() >= 300) {
	//	}
	//}
	//else {
	//	compilationQueue.push(assetPath);	
	//}
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

bool AssetManager::CompileAsset(const std::string& filePathStr, bool forceCompile, bool forAndroid) {
	std::filesystem::path filePathObj(filePathStr);
	std::string extension = filePathObj.extension().string();
	if (textureExtensions.find(extension) != textureExtensions.end()) {
		return CompileTexture(filePathStr, "diffuse", -1, forceCompile, forAndroid);
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
	else {
		std::cerr << "[AssetManager] ERROR: Attempting to compile unsupported asset extension: " << extension << std::endl;
		return false;
	}
}

bool AssetManager::CompileTexture(std::string filePath, std::string texType, GLint slot, bool forceCompile, bool forAndroid) {
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
			return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot, forceCompile, forAndroid);
		}
	}
	else {
		return CompileTextureToResource(guid, filePath.c_str(), texType.c_str(), slot, forceCompile, forAndroid);
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
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (audioExtensions.find(extension) != audioExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Audio>(guid, assetPath, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (fontExtensions.find(extension) != fontExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Font>(guid, assetPath, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (modelExtensions.find(extension) != modelExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Model>(guid, assetPath, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
		}
		else if (shaderExtensions.find(extension) != shaderExtensions.end()) {
			ResourceManager::GetInstance().UnloadResource<Shader>(guid, assetPath, it->second->compiledFilePath);
			//MetaFilesManager::DeleteMetaFile(assetPath);
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
	supportedAssetExtensions.insert(audioExtensions.begin(), audioExtensions.end());
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

bool AssetManager::IsExtensionShaderVertFrag(const std::string& extension) const {
	return shaderExtensions.find(extension) != shaderExtensions.end();
}

bool AssetManager::IsExtensionTexture(const std::string& extension) const {
	return textureExtensions.find(extension) != textureExtensions.end();
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

std::string AssetManager::GetAssetPathFromGUID(const GUID_128 guid) {
	auto it = assetMetaMap.find(guid);
	if (it != assetMetaMap.end()) {
		return it->second->sourceFilePath;
	}

	std::cerr << "[AssetManager] ERROR: Asset meta with GUID not found." << std::endl;
}

void AssetManager::CompileAllAssetsForAndroid() {
	// SHOULD RUN ON ANOTHER THREAD, TBD
	std::cout << "[AssetManager] Starting compile all assets for Android..." << std::endl;
	for (const auto& pair : assetMetaMap) {
		std::filesystem::path p(pair.second->compiledFilePath);
		std::string assetPath{};
		if (ResourceManager::GetInstance().IsExtensionShader(p.extension().generic_string())) {
			assetPath = pair.second->sourceFilePath + ".vert";
		}
		else {
			assetPath = pair.second->sourceFilePath;
		}
		CompileAsset(assetPath, true, true);
	}

	// Copy scenes to Android resources.
	for (auto p : std::filesystem::recursive_directory_iterator("Resources/Scenes")) {
		if (std::filesystem::is_regular_file(p)) {
			if (FileUtilities::CopyFile(p.path().generic_string(), (AssetManager::GetInstance().GetAndroidResourcesPath() / p.path()).generic_string())) {
				ENGINE_LOG_INFO("Copied scene file to Android Resources: " + p.path().generic_string());
			}
		}
	}

	// Output the asset manifest for Android.
	std::filesystem::path manifestFileP(GetAndroidResourcesPath() / "asset_manifest.txt");
	std::ofstream out(manifestFileP.generic_string());
	if (!out.is_open()) {
		std::cerr << "[AssetManager] Failed to open manifest file for writing\n";
		return;
	}
	for (auto& p : std::filesystem::recursive_directory_iterator("Resources")) {
		if (std::filesystem::is_regular_file(p)) {
			out << p.path().generic_string() << "\n";
		}
	}

	std::cout << "[AssetManager] Asset manifest written to " << manifestFileP.generic_string() << std::endl;
	std::cout << "[AssetManager] Finished compiling assets for Android. Android Resources folder is in GAM300/AndroidProject/app/src/main/assets/Resources" << std::endl << std::endl;
}

void AssetManager::CompileAllAssetsForDesktop() {
	// SHOULD RUN ON ANOTHER THREAD, TBD
	std::cout << "[AssetManager] Starting compile all assets for Desktop..." << std::endl;
	for (const auto& pair : assetMetaMap) {
		std::filesystem::path p(pair.second->compiledFilePath);
		std::string assetPath{};
		if (ResourceManager::GetInstance().IsExtensionShader(p.extension().generic_string())) {
			assetPath = pair.second->sourceFilePath + ".vert";
		}
		else {
			assetPath = pair.second->sourceFilePath;
		}
		CompileAsset(assetPath, true);
	}

	// Copy scenes to resources.
	for (auto p : std::filesystem::recursive_directory_iterator("Resources/Scenes")) {
		if (std::filesystem::is_regular_file(p)) {
			if (FileUtilities::CopyFile(p.path().generic_string(), (FileUtilities::GetSolutionRootDir() / p.path()).generic_string())) {
				ENGINE_LOG_INFO("Copied scene file to Project/Resources: " + p.path().generic_string());
			}
		}
	}

	std::cout << "[AssetManager] Finished compiling assets for Desktop." << std::endl << std::endl;
}
