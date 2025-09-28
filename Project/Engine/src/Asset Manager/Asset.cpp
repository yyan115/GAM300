#include "pch.h"

#include "Asset Manager/Asset.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#ifdef __ANDROID__
#include <sstream>
#include <iomanip>
#endif

std::shared_ptr<AssetMeta> IAsset::GenerateBaseMetaFile(GUID_128 guid128, const std::string& assetPath, const std::string& resourcePath) {
	std::string metaFilePath = assetPath + ".meta";
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
	assetMetaData.AddMember("last_compiled", rapidjson::Value().SetString(timestamp.c_str(), allocator), allocator);

	doc.AddMember("AssetMetaData", assetMetaData, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	std::ofstream metaFile(metaFilePath);
	metaFile << buffer.GetString();
	metaFile.close();

	std::cout << "[IAsset] Generated base meta file " << metaFilePath << std::endl;

	MetaFilesManager::AddGUID128Mapping(assetPath, guid128);

	std::shared_ptr<AssetMeta> metaData = std::make_shared<AssetMeta>();
	metaData->PopulateAssetMeta(guid128, assetPath, resourcePath, MetaFilesManager::CURRENT_METADATA_VERSION);
	return metaData;
}
