#include "pch.h"
#include "Asset Manager/AssetMeta.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include <rapidjson/document.h>
#include "Logging.hpp"

void AssetMeta::PopulateAssetMeta(GUID_128 _guid, const std::string& _sourcePath, const std::string& _compiledPath, int _ver)
{
	guid = _guid;
	sourceFilePath = _sourcePath;
	compiledFilePath = _compiledPath;
	version = _ver;
	lastCompileTime = std::chrono::system_clock::now();
}

void AssetMeta::PopulateAssetMetaFromFile(const std::string& metaFilePath)
{
	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());

	const auto& assetMetaData = doc["AssetMetaData"];
	if (assetMetaData.HasMember("version")) {
		version = assetMetaData["version"].GetInt();
	}

	if (assetMetaData.HasMember("guid")) {
		GUID_string guidStr = assetMetaData["guid"].GetString();
		guid = GUIDUtilities::ConvertStringToGUID128(guidStr);
	}

	if (assetMetaData.HasMember("source")) {
		sourceFilePath = assetMetaData["source"].GetString();
	}

	if (assetMetaData.HasMember("compiled")) {
		compiledFilePath = assetMetaData["compiled"].GetString();
	}

	lastCompileTime = MetaFilesManager::GetLastCompileTimeFromMetaFile(metaFilePath);

	ifs.close();
}

void TextureMeta::PopulateTextureMeta(uint32_t _ID, const std::string& _type, uint32_t _unit)
{
	ID = _ID;
	type = _type;
	unit = _unit;
}

void TextureMeta::PopulateAssetMetaFromFile(const std::string& metaFilePath)
{
	AssetMeta::PopulateAssetMetaFromFile(metaFilePath);

	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());

	const auto& assetMetaData = doc["AssetMetaData"];
	if (assetMetaData.HasMember("id")) {
		ID = static_cast<uint32_t>(assetMetaData["id"].GetInt());
	}

	if (assetMetaData.HasMember("type")) {
		type = assetMetaData["type"].GetString();
	}

	if (assetMetaData.HasMember("unit")) {
		unit = assetMetaData["unit"].GetInt();
	}

	ifs.close();
}
