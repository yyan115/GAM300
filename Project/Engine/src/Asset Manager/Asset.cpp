#include "pch.h"

#include "Asset Manager/Asset.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#ifdef __ANDROID__
#include <sstream>
#include <iomanip>
#endif
#include <Utilities/FileUtilities.hpp>
#include <Asset Manager/AssetManager.hpp>
#include "Logging.hpp"

std::shared_ptr<AssetMeta> IAsset::GenerateBaseMetaFile(GUID_128 guid128, const std::string& assetPath, const std::string& resourcePath, const std::string& androidResourcePath, bool forAndroid) {
	std::string metaFilePath{};
	if (!forAndroid) {
		metaFilePath = assetPath + ".meta";
	}
	else {
		std::string relativePath = assetPath.substr(assetPath.find("Resources"));
		metaFilePath = (AssetManager::GetInstance().GetAndroidResourcesPath() / relativePath).generic_string() + ".meta";
	}
	GUID_string guidStr = GUIDUtilities::ConvertGUID128ToString(guid128);

	// Write and save the .meta file to disk.
	rapidjson::Document doc;
	doc.SetObject();
	auto& allocator = doc.GetAllocator();

	rapidjson::Value assetMetaData(rapidjson::kObjectType);

	// Add meta file version
	assetMetaData.AddMember("version", MetaFilesManager::CURRENT_METADATA_VERSION, allocator);
	// Add GUID
	assetMetaData.AddMember("guid", rapidjson::Value().SetString(guidStr.c_str(), allocator), allocator);
	// Add source asset path
	assetMetaData.AddMember("source", rapidjson::Value().SetString(assetPath.c_str(), allocator), allocator);
	// Add compiled resource path
	assetMetaData.AddMember("compiled", rapidjson::Value().SetString(resourcePath.c_str(), allocator), allocator);
	if (forAndroid)
		assetMetaData.AddMember("android_compiled", rapidjson::Value().SetString(AssetManager::GetInstance().ExtractRelativeAndroidPath(androidResourcePath).c_str(), allocator), allocator);
	// Add last compiled timestamp
	auto tp = std::chrono::system_clock::now();
#ifdef __ANDROID__
	auto time_t = std::chrono::system_clock::to_time_t(tp);
	auto tm = *std::localtime(&time_t);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	std::string timestamp = oss.str();
#else
	std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", tp);
#endif
	//assetMetaData.AddMember("last_compiled", rapidjson::Value().SetString(timestamp.c_str(), allocator), allocator);

	doc.AddMember("AssetMetaData", assetMetaData, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	std::ofstream metaFile(metaFilePath);
	metaFile << buffer.GetString();
	metaFile.close();

	//if (!forAndroid) {
	//	// Save the meta file in the root project directory as well.
	//	try {
	//		std::filesystem::copy_file(metaFilePath, (FileUtilities::GetSolutionRootDir() / metaFilePath).generic_string(),
	//			std::filesystem::copy_options::overwrite_existing);
	//	}
	//	catch (const std::filesystem::filesystem_error& e) {
	//		std::cerr << "[Asset] Copy failed: " << e.what() << std::endl;
	//	}
	//}
	//else {
	//	// Save the meta file to the build and root directory as well.
	//	try {
	//		std::string buildMetaPath = assetPath + ".meta";
	//		std::filesystem::copy_file(metaFilePath, buildMetaPath,
	//			std::filesystem::copy_options::overwrite_existing);
	//		std::filesystem::copy_file(metaFilePath, (FileUtilities::GetSolutionRootDir() / buildMetaPath).generic_string(),
	//			std::filesystem::copy_options::overwrite_existing);
	//	}
	//	catch (const std::filesystem::filesystem_error& e) {
	//		std::cerr << "[Asset] Copy failed: " << e.what() << std::endl;
	//	}
	//}

	ENGINE_PRINT("[IAsset] Generated base meta file ", metaFilePath, "\n");

	MetaFilesManager::AddGUID128Mapping(assetPath, guid128);

	std::shared_ptr<AssetMeta> metaData = std::make_shared<AssetMeta>();
	metaData->PopulateAssetMeta(guid128, assetPath, resourcePath, MetaFilesManager::CURRENT_METADATA_VERSION);
	return metaData;
}
