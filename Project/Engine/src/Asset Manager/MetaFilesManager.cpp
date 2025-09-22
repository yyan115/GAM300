#include "pch.h"

#include <filesystem>
#include "Asset Manager/MetaFilesManager.hpp"
<<<<<<< Updated upstream
=======
#include "Asset Manager/AssetManager.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <Asset Manager/FileUtilities.hpp>
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
>>>>>>> Stashed changes

std::unordered_map<std::string, GUID_128> MetaFilesManager::assetPathToGUID128;

GUID_128 MetaFilesManager::GenerateMetaFile(const std::string& assetPath) {
	std::string metaFilePath = assetPath + ".meta";
	if (!MetaFileExists(assetPath)) {
		GUID_string guidStr = GUIDUtilities::GenerateGUIDString();

		// Write and save the .meta file to disk.
		std::ofstream metaFile(metaFilePath);
		if (metaFile.is_open()) {
			metaFile << "GUID: " << guidStr << std::endl;
			metaFile << "Version: " << CURRENT_METADATA_VERSION << std::endl;
			metaFile.close();
			std::cout << "[MetaFilesManager] Generated .meta for: " << assetPath << std::endl;
		} else {
			std::cerr << "[MetaFilesManager] ERROR: Unable to create .meta file: " << metaFilePath << std::endl;
		}

		GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
		AddGUID128Mapping(assetPath, guid128);
		return guid128;
	}

	return GUID_128{};
}

bool MetaFilesManager::MetaFileExists(const std::string& assetPath) {
	std::filesystem::path metaPath = std::filesystem::path(assetPath + ".meta");
	return std::filesystem::exists(metaPath);
}

GUID_string MetaFilesManager::GetGUIDFromMetaFile(const std::string& metaFilePath) {
	if (std::filesystem::exists(metaFilePath)) {
		std::ifstream metaFS(metaFilePath);
		std::string line{};
		while (std::getline(metaFS, line)) {
			size_t pos = line.find("GUID:");
			if (pos != std::string::npos) {
				return line.substr(pos + 6);
			}
		}
	}

	return "";
}

GUID_string MetaFilesManager::GetGUIDFromAssetFile(const std::string& assetPath) {
	std::string metaFilePath = assetPath + ".meta";
	return GetGUIDFromMetaFile(metaFilePath);
}

void MetaFilesManager::InitializeAssetMetaFiles(const std::string& rootAssetFolder) {
	std::unordered_set<std::string> supportedExtensions = {
		".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG", ".bmp", ".BMP", // texture files
		".wav", ".ogg", // audio files
		".ttf", // font files
		".obj", ".fbx", // model files
		".vert", ".frag" // shader files
	};

<<<<<<< Updated upstream
	for (const auto& file : std::filesystem::recursive_directory_iterator(rootAssetFolder)) {
		// Check that it is a regular file (not a directory).
		if (file.is_regular_file()) {
			std::string extension = file.path().extension().string();

			if (supportedExtensions.find(extension) != supportedExtensions.end()) {
				std::string assetPath = file.path().generic_string();
				GUID_128 guid128{};
				if (!MetaFileExists(assetPath)) {
					guid128 = GenerateMetaFile(assetPath);
				}
				else if (!MetaFileUpdated(assetPath)) {
					std::cout << "[MetaFilesManager] .meta outdated for: " << assetPath << ". Regenerating..." << std::endl;
					guid128 = UpdateMetaFile(assetPath);
				}
				else {
					GUID_string guidStr = GetGUIDFromAssetFile(assetPath);
					guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
					std::cout << "[MetaFilesManager] .meta already exists for: " << assetPath << std::endl;
				}

				AddGUID128Mapping(assetPath, guid128);
			}
		}
=======
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		std::cerr << "[MetaFilesManager] ERROR: Platform not available for asset discovery!" << std::endl;
		return;
	}

	std::vector<std::string> assetFiles = platform->ListAssets(rootAssetFolder, true);

	for (const std::string& assetPath : assetFiles) {
		bool isShader = false;
		std::filesystem::path filePath(assetPath);
		std::string extension = filePath.extension().string();

		if (AssetManager::GetInstance().IsAssetExtensionSupported(extension)) {
			if (AssetManager::GetInstance().GetShaderExtensions().find(extension) != AssetManager::GetInstance().GetShaderExtensions().end()) {
				std::string shaderName = filePath.stem().string();
				if (compiledShaderNames.find(shaderName) != compiledShaderNames.end()) {
					continue; // Skip if this shader has already been compiled.
				}
				compiledShaderNames.insert(shaderName);
				isShader = true;
			}

			if (!MetaFileExists(assetPath)) {
				std::cout << "[MetaFilesManager] .meta missing for: " << assetPath << ". Compiling and generating..." << std::endl;
				AssetManager::GetInstance().CompileAsset(assetPath);
			}
			else if (!MetaFileUpdated(assetPath)) {
				std::cout << "[MetaFilesManager] .meta outdated for: " << assetPath << ". Re-compiling and regenerating..." << std::endl;
				AssetManager::GetInstance().CompileAsset(assetPath);
			}
			else {
				std::string finalAssetPath = assetPath;
				if (isShader) {
					finalAssetPath = (filePath.parent_path() / filePath.stem()).generic_string();
				}

				GUID_128 guid128 = GetGUID128FromAssetFile(finalAssetPath);
				AddGUID128Mapping(finalAssetPath, guid128);
				AssetManager::GetInstance().AddAssetMetaToMap(finalAssetPath);

				//std::cout << "[MetaFilesManager] .meta already exists for: " << assetPath << std::endl;
			}
		}
			//// fallback for shaders
			//else if (extension == ".meta") {
			//	std::string assetPath = file.path().generic_string();
			//	assetPath = assetPath.substr(0, assetPath.size() - 5); // Remove the .meta extension
			//	GUID_string guidStr = GetGUIDFromMetaFile(file.path().generic_string());
			//	GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
			//	AddGUID128Mapping(assetPath, guid128);
			//}
>>>>>>> Stashed changes
	}
}

GUID_128 MetaFilesManager::GetGUID128FromAssetFile(const std::string& assetPath) {
	return assetPathToGUID128[assetPath];
}

bool MetaFilesManager::MetaFileUpdated(const std::string& assetPath) {
	std::filesystem::path metaPath = std::filesystem::path(assetPath + ".meta");
	assert(std::filesystem::exists(metaPath) && "Meta file does not exist.");

	std::ifstream metaFS(metaPath);
	std::string line{};
	bool hasVersion = false;
	while (std::getline(metaFS, line)) {
		size_t pos = line.find("Version:");
		if (pos != std::string::npos) {
			hasVersion = true;
			int version = std::stoi(line.substr(pos + 9));
			if (version != CURRENT_METADATA_VERSION) {
				return false;
			}
			else return true;
		}
	}

	// If no version found, consider it outdated.
	return false;
}

GUID_128 MetaFilesManager::UpdateMetaFile(const std::string& assetPath) {
	std::filesystem::path metaPath = std::filesystem::path(assetPath + ".meta");
	if (std::filesystem::exists(metaPath)) {
		std::ofstream metaFile(metaPath, std::ios::out | std::ios::trunc);
		if (metaFile.is_open()) {
			GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
			metaFile << "GUID: " << guidStr << std::endl;
			metaFile << "Version: " << CURRENT_METADATA_VERSION << std::endl;
			metaFile.close();

			std::cout << "[MetaFilesManager] Updated .meta for: " << assetPath << std::endl;

			GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
			AddGUID128Mapping(assetPath, guid128);
			return guid128;
		}
		else {
			std::cerr << "[MetaFilesManager] ERROR: Unable to update .meta file: " << metaPath << std::endl;
		}
	}

	return GUID_128{};
}

void MetaFilesManager::AddGUID128Mapping(const std::string& assetPath, const GUID_128& guid) {
	assetPathToGUID128[assetPath] = guid;
}
