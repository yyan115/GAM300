#include "pch.h"

#include <filesystem>
#include "Asset Manager/MetaFilesManager.hpp"
#include "Asset Manager/AssetManager.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <Asset Manager/FileUtilities.hpp>
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

std::unordered_map<std::string, GUID_128> MetaFilesManager::assetPathToGUID128;

//GUID_128 MetaFilesManager::GenerateMetaFile(const std::string& assetPath, const std::string& resourcePath) {
//	std::string metaFilePath = assetPath + ".meta";
//	GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
//
//	// Write and save the .meta file to disk.
//	rapidjson::Document doc;
//	doc.SetObject();
//	auto& allocator = doc.GetAllocator();
//
//	rapidjson::Value assetMetaData(rapidjson::kObjectType);
//
//	// Add GUID
//	assetMetaData.AddMember("guid", rapidjson::Value().SetString(guidStr.c_str(), allocator), allocator);
//	// Add source asset path
//	assetMetaData.AddMember("source", rapidjson::Value().SetString(assetPath.c_str(), allocator), allocator);
//	// Add compiled resource path
//	assetMetaData.AddMember("resource", rapidjson::Value().SetString(resourcePath.c_str(), allocator), allocator);
//	// Add last compiled timestamp
//	auto tp = std::chrono::system_clock::now();
//	std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", tp);
//	assetMetaData.AddMember("last_compiled", rapidjson::Value().SetString(timestamp.c_str(), allocator), allocator);
//	// Add meta file version
//	assetMetaData.AddMember("version", CURRENT_METADATA_VERSION, allocator);
//
//	doc.AddMember("AssetMetaData", assetMetaData, allocator);
//
//	rapidjson::StringBuffer buffer;
//	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
//	doc.Accept(writer);
//
//	std::ofstream metaFile(metaFilePath);
//	metaFile << buffer.GetString();
//	metaFile.close();
//
//	GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
//	AddGUID128Mapping(assetPath, guid128);
//	return guid128;
//}

bool MetaFilesManager::MetaFileExists(const std::string& assetPath) {
	std::filesystem::path metaFilePath(assetPath);
	std::string extension = metaFilePath.extension().string();
	if (AssetManager::GetInstance().GetShaderExtensions().find(extension) != AssetManager::GetInstance().GetShaderExtensions().end()) {
		metaFilePath = (metaFilePath.parent_path() / metaFilePath.stem()).generic_string() + ".meta";
	}
	else metaFilePath = std::filesystem::path(assetPath + ".meta");
	return std::filesystem::exists(metaFilePath.generic_string());
}

GUID_string MetaFilesManager::GetGUIDFromMetaFile(const std::string& metaFilePath) {
	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());

	const auto& assetMetaData = doc["AssetMetaData"];

	if (assetMetaData.HasMember("guid")) {
		return assetMetaData["guid"].GetString();
	}
	else {
		std::cerr << "[MetaFilesManager] ERROR: GUID not found in meta file: " << metaFilePath << std::endl;
		return "";
	}
}

GUID_string MetaFilesManager::GetGUIDFromAssetFile(const std::string& assetPath) {
	std::string metaFilePath = assetPath + ".meta";
	return GetGUIDFromMetaFile(metaFilePath);
}

void MetaFilesManager::InitializeAssetMetaFiles(const std::string& rootAssetFolder) {
	std::unordered_set<std::string> compiledShaderNames; // To avoid compiling the same shader multiple times.

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
	}
}

GUID_128 MetaFilesManager::GetGUID128FromAssetFile(const std::string& assetPath) {
	if (assetPathToGUID128.find(assetPath) == assetPathToGUID128.end()) {
		GUID_string guidStr = GetGUIDFromAssetFile(assetPath);
		GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
		assetPathToGUID128[assetPath] = guid128;
		return guid128;
	}

	return assetPathToGUID128[assetPath];
}

bool MetaFilesManager::MetaFileUpdated(const std::string& assetPath) {
	std::filesystem::path metaFilePath(assetPath);
	std::string extension = metaFilePath.extension().string();
	if (AssetManager::GetInstance().GetShaderExtensions().find(extension) != AssetManager::GetInstance().GetShaderExtensions().end()) {
		metaFilePath = (metaFilePath.parent_path() / metaFilePath.stem()).generic_string() + ".meta";
	}
	else metaFilePath = std::filesystem::path(assetPath + ".meta");
	
	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());
	if (!doc.IsObject()) return false;
	if (!doc.HasMember("AssetMetaData")) return false;

	const auto& assetMetaData = doc["AssetMetaData"];

	if (assetMetaData.HasMember("version")) {
		return assetMetaData["version"].GetInt() == CURRENT_METADATA_VERSION;
	}
	else {
		std::cerr << "[MetaFilesManager] ERROR: version not found in meta file: " << metaFilePath << std::endl;
		return "";
	}
}

//GUID_128 MetaFilesManager::UpdateMetaFile(const std::string& assetPath) {
//	std::filesystem::path metaPath = std::filesystem::path(assetPath + ".meta");
//	if (std::filesystem::exists(metaPath)) {
//		std::ofstream metaFile(metaPath, std::ios::out | std::ios::trunc);
//		if (metaFile.is_open()) {
//			GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
//			metaFile << "GUID: " << guidStr << std::endl;
//			metaFile << "Version: " << CURRENT_METADATA_VERSION << std::endl;
//			metaFile.close();
//
//			std::cout << "[MetaFilesManager] Updated .meta for: " << assetPath << std::endl;
//
//			GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
//			AddGUID128Mapping(assetPath, guid128);
//			return guid128;
//		}
//		else {
//			std::cerr << "[MetaFilesManager] ERROR: Unable to update .meta file: " << metaPath << std::endl;
//		}
//	}
//
//	return GUID_128{};
//}

void MetaFilesManager::AddGUID128Mapping(const std::string& assetPath, const GUID_128& guid) {
	assetPathToGUID128[assetPath] = guid;
}

bool MetaFilesManager::DeleteMetaFile(const std::string& assetPath) {
	std::filesystem::path p(assetPath + ".meta");
	return FileUtilities::RemoveFile(p.generic_string());
}
