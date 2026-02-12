#include "pch.h"

#include <filesystem>
#include "Asset Manager/MetaFilesManager.hpp"
#include "Asset Manager/AssetManager.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <Utilities/FileUtilities.hpp>
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#include "Logging.hpp"

std::unordered_map<std::string, GUID_128> MetaFilesManager::assetPathToGUID128;

bool MetaFilesManager::MetaFileExists(const std::string& assetPath) {
	std::filesystem::path metaFilePath(assetPath);
#ifndef ANDROID
	//std::filesystem::path rootMetaFilePath(FileUtilities::GetSolutionRootDir() / assetPath);
	std::string extension = metaFilePath.extension().string();
	if (AssetManager::GetInstance().GetShaderExtensions().find(extension) != AssetManager::GetInstance().GetShaderExtensions().end() ||
		ResourceManager::GetInstance().IsExtensionShader(extension)) {
		metaFilePath = (metaFilePath.parent_path() / metaFilePath.stem()).generic_string() + ".meta";
		//rootMetaFilePath = (rootMetaFilePath.parent_path() / rootMetaFilePath.stem()).generic_string() + ".meta";
	}
	else {
		metaFilePath = std::filesystem::path(assetPath + ".meta");
		//rootMetaFilePath = std::filesystem::path(rootMetaFilePath.generic_string() + ".meta");
	}

	//return std::filesystem::exists(metaFilePath.generic_string()); && std::filesystem::exists(rootMetaFilePath.generic_string());
	ENGINE_LOG_INFO("Meta file path: " + metaFilePath.generic_string());
	return std::filesystem::exists(metaFilePath.generic_string());
#endif

#ifdef ANDROID
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: Platform not available for asset discovery!", "\n");
		return false;
	}

	metaFilePath = assetPath + ".meta";
	ENGINE_LOG_INFO("Meta file path: " + metaFilePath.generic_string());
	return platform->FileExists(metaFilePath);

	//std::vector<std::string> assetFiles = platform->ListAssets("Resources", true);
	//ENGINE_LOG_INFO("assetFiles.size(): " + std::to_string(assetFiles.size()));
	//for (const auto& asset : assetFiles) {
	//	ENGINE_LOG_INFO(asset);
	//}
	//auto it = std::find(assetFiles.begin(), assetFiles.end(), metaFilePath);
	//return it != assetFiles.end();
#endif
}

//std::chrono::system_clock::time_point MetaFilesManager::GetLastCompileTimeFromMetaFile(const std::string& metaFilePath) {
//	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
//	IPlatform* platform = WindowManager::GetPlatform();
//	if (!platform) {
//		ENGINE_LOG_DEBUG("[MetaFilesManager] ERROR: Platform not available for asset discovery!");
//		return std::chrono::system_clock::time_point{};
//	}
//	std::vector<uint8_t> metaFileData = platform->ReadAsset(metaFilePath);
//	rapidjson::Document doc;
//	if (!metaFileData.empty()) {
//		rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
//		doc.ParseStream(ms);
//	}
//	if (doc.HasParseError()) {
//		ENGINE_LOG_DEBUG("[MetaFilesManager]: Rapidjson parse error: " + metaFilePath);
//	}
//
//	const auto& assetMetaData = doc["AssetMetaData"];
//	if (assetMetaData.HasMember("last_compiled")) {
//		std::string timestampStr = assetMetaData["last_compiled"].GetString();
//		std::istringstream iss(timestampStr);
//		std::chrono::sys_time<std::chrono::milliseconds> tp;
//
//		// Parse using the same format string you used for formatting
//#ifdef ANDROID
//		// std::chrono::parse not available on Android NDK yet - use epoch time so assets always recompile
//		tp = std::chrono::sys_time<std::chrono::milliseconds>{};
//#else
//		iss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", tp);
//#endif
//
//		if (iss.fail()) {
//			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: Failed to parse timestamp for .meta file: ", metaFilePath, "\n");
//			return std::chrono::system_clock::time_point{};
//		}
//		else {
//			// Convert sys_time<seconds> to system_clock::time_point
//			return tp;
//		}
//	}
//	else {
//		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: last_compiled not found in meta file: ", metaFilePath, "\n");
//		return std::chrono::system_clock::time_point{};
//	}
//}

GUID_string MetaFilesManager::GetGUIDFromMetaFile(const std::string& metaFilePath) {
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_LOG_DEBUG("[MetaFilesManager] ERROR: Platform not available for asset discovery!");
		return "";
	}
	if (!platform->FileExists(metaFilePath)) {
		ENGINE_LOG_DEBUG("[MetaFilesManager]: Meta file not found: " + metaFilePath);
		return "";
	}

	std::vector<uint8_t> metaFileData = platform->ReadAsset(metaFilePath);
	rapidjson::Document doc;
	if (!metaFileData.empty()) {
		rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
		doc.ParseStream(ms);
	}
	if (doc.HasParseError()) {
		ENGINE_LOG_DEBUG("[MetaFilesManager]: Rapidjson parse error: " + metaFilePath);
	}

	const auto& assetMetaData = doc["AssetMetaData"];

	if (assetMetaData.HasMember("guid")) {
		GUID_string guid = assetMetaData["guid"].GetString();
		return guid;
	}
	else {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: GUID not found in meta file: ", metaFilePath, "\n");
		return "";
	}
}

GUID_string MetaFilesManager::GetGUIDFromAssetFile(const std::string& assetPath) {
	std::string metaFilePath = assetPath + ".meta";
	ENGINE_LOG_DEBUG("[MetaFilesManager]: GetGUIDFromMetaFile: " + metaFilePath);
	return GetGUIDFromMetaFile(metaFilePath);
}

void MetaFilesManager::InitializeAssetMetaFiles(const std::string& rootAssetFolder) {
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: Platform not available for asset discovery!", "\n");
		return;
	}

	AssetManager::GetInstance().SetRootAssetDirectory(rootAssetFolder);
	std::vector<std::string> assetFiles = platform->ListAssets(rootAssetFolder, true);
	//ENGINE_LOG_INFO("platform->ListAssets size: " + std::to_string(assetFiles.size()));

	for (std::string assetPath : assetFiles) {
		std::filesystem::path filePath(assetPath);
		std::string extension = filePath.extension().string();
		extension.erase(std::remove_if(extension.begin(), extension.end(), ::isspace), extension.end());
		assetPath = (filePath.parent_path() / filePath.stem()).generic_string() + extension;
		//ENGINE_LOG_INFO("extension: " + extension);

		if (AssetManager::GetInstance().IsAssetExtensionSupported(extension)) {
#ifdef EDITOR
		if (MetaFileExists(assetPath)) {
			if (AssetManager::GetInstance().IsExtensionShaderVertFrag(extension)) {
				std::string shaderAssetPath = (filePath.parent_path() / filePath.stem()).generic_string();

				GUID_128 guid128 = GetGUID128FromAssetFile(shaderAssetPath);
				AddGUID128Mapping(shaderAssetPath, guid128);
				auto assetMeta = AssetManager::GetInstance().AddAssetMetaToMap(shaderAssetPath);
				AssetManager::GetInstance().CompileAsset(assetPath, false);
				continue;
			}
			GUID_128 guid128 = GetGUID128FromAssetFile(assetPath);
			AddGUID128Mapping(assetPath, guid128);
			auto assetMeta = AssetManager::GetInstance().AddAssetMetaToMap(assetPath);
			AssetManager::GetInstance().CompileAsset(assetMeta, false);
		}
		else {
			// .meta missing — must compile for the first time
			AssetManager::GetInstance().CompileAsset(assetPath, true);
		}
//#if !defined(EDITOR) && !defined(ANDROID) 
//			if (!MetaFileExists(assetPath)) {
//				ENGINE_PRINT("[MetaFilesManager] .meta missing for: ", assetPath, ". Compiling and generating...", "\n");
//				AssetManager::GetInstance().CompileAsset(assetPath);
//			}
//			else if (!MetaFileUpdated(assetPath)) {
//				ENGINE_PRINT("[MetaFilesManager] .meta outdated for: ", assetPath, ". Re-compiling and regenerating...", "\n");
//				AssetManager::GetInstance().CompileAsset(assetPath);
//			}
//			else {
//				if (AssetFileUpdated(assetPath)) {
//					ENGINE_PRINT("[MetaFilesManager] Asset file was updated: ", assetPath, ". Re-compiling...", "\n");
//					AssetManager::GetInstance().CompileAsset(assetPath, true);
//				}
//				else {
//					if (AssetManager::GetInstance().IsExtensionShaderVertFrag(extension)) {
//						assetPath = (filePath.parent_path() / filePath.stem()).generic_string();
//					}
//
//					GUID_128 guid128 = GetGUID128FromAssetFile(assetPath);
//					AddGUID128Mapping(assetPath, guid128);
//					AssetManager::GetInstance().AddAssetMetaToMap(assetPath);
//				}
//			}
#else
			ENGINE_LOG_INFO("IsAssetExtensionSupported");
			if (AssetManager::GetInstance().IsExtensionShaderVertFrag(extension)) {
				assetPath = (filePath.parent_path() / filePath.stem()).generic_string();
			}

			GUID_128 guid128 = GetGUID128FromAssetFile(assetPath);
			AddGUID128Mapping(assetPath, guid128);
			AssetManager::GetInstance().AddAssetMetaToMap(assetPath);
#endif
		}
		else {
			//ENGINE_LOG_INFO("no");
		}
	}
}

GUID_128 MetaFilesManager::GetGUID128FromAssetFile(const std::string& assetPath) {
	ENGINE_LOG_DEBUG("[MetaFilesManager]: GetGUID128FromAssetFile: " + assetPath);
	if (assetPathToGUID128.find(assetPath) == assetPathToGUID128.end()) {
		ENGINE_LOG_DEBUG("[MetaFilesManager]: GetGUIDFromAssetFile: " + assetPath);
		GUID_string guidStr = GetGUIDFromAssetFile(assetPath);
		if (guidStr == "") {
			guidStr = GUIDUtilities::GenerateGUIDString();
			return GUIDUtilities::ConvertStringToGUID128(guidStr);
		}
		GUID_128 guid128 = GUIDUtilities::ConvertStringToGUID128(guidStr);
		assetPathToGUID128[assetPath] = guid128;
		return guid128;
	}

	return assetPathToGUID128[assetPath];
}

std::string MetaFilesManager::GetResourceNameFromAssetFile(const std::string& assetPath) {
	std::string metaFilePath = assetPath + ".meta";
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_LOG_DEBUG("[MetaFilesManager] ERROR: Platform not available for asset discovery!");
		return "";
	}
	if (!platform->FileExists(metaFilePath)) {
		ENGINE_LOG_DEBUG("[MetaFilesManager]: Meta file not found: " + metaFilePath);
		return "";
	}

	std::vector<uint8_t> metaFileData = platform->ReadAsset(metaFilePath);
	rapidjson::Document doc;
	if (!metaFileData.empty()) {
		rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
		doc.ParseStream(ms);
	}
	if (doc.HasParseError()) {
		ENGINE_LOG_DEBUG("[MetaFilesManager]: Rapidjson parse error: " + metaFilePath);
	}

	const auto& assetMetaData = doc["AssetMetaData"];

#ifdef ANDROID
	if (assetMetaData.HasMember("android_compiled")) {
		std::string androidResourcePath = assetMetaData["android_compiled"].GetString();
		return androidResourcePath;
	}
#else
	if (assetMetaData.HasMember("compiled")) {
		std::string resourcePath = assetMetaData["compiled"].GetString();
#ifndef EDITOR
		resourcePath = resourcePath.substr(resourcePath.find("Resources"));
#endif
		return resourcePath;
	}
#endif
	return "";
}

bool MetaFilesManager::MetaFileUpdated(const std::string& assetPath) {
	std::filesystem::path metaFilePath(assetPath);
	//std::filesystem::path rootMetaFilePath(FileUtilities::GetSolutionRootDir() / assetPath);
	std::string extension = metaFilePath.extension().string();
	if (AssetManager::GetInstance().IsExtensionShaderVertFrag(extension)) {
		metaFilePath = (metaFilePath.parent_path() / metaFilePath.stem()).generic_string() + ".meta";
		//rootMetaFilePath = (rootMetaFilePath.parent_path() / rootMetaFilePath.stem()).generic_string() + ".meta";
	}
	else {
		metaFilePath = std::filesystem::path(assetPath + ".meta");
		//rootMetaFilePath = std::filesystem::path(rootMetaFilePath.generic_string() + ".meta");
	}

	std::ifstream ifs(metaFilePath);
	//std::ifstream ifsRoot(rootMetaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	//std::string jsonContentRoot((std::istreambuf_iterator<char>(ifsRoot)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());
	if (!doc.IsObject()) {
		ifs.close();
		//ifsRoot.close();
		return false;
	}
	if (!doc.HasMember("AssetMetaData")) {
		ifs.close();
		//ifsRoot.close();
		return false;
	}
	//rapidjson::Document docRoot;
	//docRoot.Parse(jsonContentRoot.c_str());
	//if (!docRoot.IsObject()) {
	//	ifs.close();
	//	ifsRoot.close();
	//	return false;
	//}
	//if (!docRoot.HasMember("AssetMetaData")) {
	//	ifs.close();
	//	return false;
	//}
	
	if (AssetManager::GetInstance().IsExtensionTexture(extension)) {
		if (!doc.HasMember("TextureMetaData")) { //|| !docRoot.HasMember("TextureMetaData")) {
			ifs.close();
			//ifsRoot.close();
			return false;
		}
	}

	const auto& assetMetaData = doc["AssetMetaData"];
	//const auto& assetMetaDataRoot = docRoot["AssetMetaData"];

	if (assetMetaData.HasMember("version")) {
		if (assetMetaData["version"].GetInt() == CURRENT_METADATA_VERSION) {
			ifs.close();
			return true;
		}
		else {
			ifs.close();
			return false;
		}
	}
	else {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: version not found in meta file: ", metaFilePath, "\n");
		ifs.close();
		return false;
	}
}

//bool MetaFilesManager::AssetFileUpdated(const std::string& assetPath) {
//	// Get the last modified time for the asset file.
//	std::filesystem::path assetFile(assetPath);
//	auto ftime = std::filesystem::last_write_time(assetFile);
//#ifndef ANDROID
//	auto lastModifiedTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
//#endif
//#ifdef ANDROID
//	using namespace std::chrono;
//	auto lastModifiedTime = time_point_cast<system_clock::duration>(
//		ftime - decltype(ftime)::clock::now() + system_clock::now()
//	);
//#endif
//
//	// Compare the last modified time with the meta file's last compile time.
//	// If it was modified after the last compile time, the asset file was updated and requires re-compilation.
//	std::string extension = assetFile.extension().string();
//	std::string metaFilePath{};
//	//std::string metaFilePathRoot{};
//	if (AssetManager::GetInstance().IsExtensionShaderVertFrag(extension)) {
//		metaFilePath = (assetFile.parent_path() / assetFile.stem()).generic_string() + ".meta";
//		//metaFilePathRoot = (FileUtilities::GetSolutionRootDir() / assetFile.parent_path() / assetFile.stem()).generic_string() + ".meta";
//	}
//	else {
//		metaFilePath = assetPath + ".meta";
//		//metaFilePathRoot = (FileUtilities::GetSolutionRootDir() / assetPath).generic_string() + ".meta";
//	}
//
//	//auto lastCompileTime = GetLastCompileTimeFromMetaFile(metaFilePath);
//	//if (lastModifiedTime > lastCompileTime) {
//	//	return true;
//	//}
//
//	return false;
//}

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

void MetaFilesManager::CleanupUnusedMetaFiles() {
	ENGINE_PRINT("[MetaFilesManager] Cleaning up un-used meta files...", "\n");

	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MetaFilesManager] ERROR: Platform not available for asset discovery!", "\n");
		return;
	}

	std::vector<std::string> assetFiles = platform->ListAssets(AssetManager::GetInstance().GetRootAssetDirectory(), true);

	for (std::string metaPath : assetFiles) {
		std::filesystem::path metaPathObj(metaPath);
		std::filesystem::path assetPath(metaPathObj.parent_path() / metaPathObj.stem());
		std::string assetExtension = assetPath.extension().string();
		std::string extension = metaPathObj.extension().string();

		if (AssetManager::GetInstance().IsExtensionMetaFile(extension)) {
			//ENGINE_PRINT("Checking meta file: ", metaPath);
			// Check if the source file is still present. If not, the meta file is orphaned and should be deleted.
			std::ifstream ifs(metaPath);
			std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

			rapidjson::Document doc;
			doc.Parse(jsonContent.c_str());

			if (!doc.IsObject()) {
				FileUtilities::RemoveFile(metaPath);
				continue;
			}

			const auto& assetMetaData = doc["AssetMetaData"];
			if (assetMetaData.HasMember("source")) {
				std::string sourceFilePath = assetMetaData["source"].GetString();
				ifs.close();
				//ENGINE_PRINT("Checking source from meta file: ", sourceFilePath);
				std::string vertFilePath = sourceFilePath + ".vert";
				std::string fragFilePath = sourceFilePath + ".frag";
				bool keepMeta = FileUtilities::StrictExists(vertFilePath) || FileUtilities::StrictExists(fragFilePath) || FileUtilities::StrictExists(sourceFilePath);
				if (!keepMeta) {
					//ENGINE_PRINT("Source from meta file ", sourceFilePath, " doesn't exist. Deleting file...");
					FileUtilities::RemoveFile(metaPath);
				}
			}
		}
	}
}
