#include "pch.h"
#include "Asset Manager/AssetMeta.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include <rapidjson/document.h>
#include "Logging.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>

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
	ENGINE_LOG_INFO("PopulateAssetMetaFromFile");
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_LOG_DEBUG("[AssetMeta] ERROR: Platform not available for asset discovery!");
		return;
	}
	std::vector<uint8_t> metaFileData = platform->ReadAsset(metaFilePath);
	rapidjson::Document doc;
	if (!metaFileData.empty()) {
		rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
		doc.ParseStream(ms);
	}
	if (doc.HasParseError()) {
		ENGINE_LOG_DEBUG("[AssetMeta]: Rapidjson parse error: " + metaFilePath);
	}

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

	if (assetMetaData.HasMember("android_compiled")) {
		androidCompiledFilePath = assetMetaData["android_compiled"].GetString();
	}

	lastCompileTime = MetaFilesManager::GetLastCompileTimeFromMetaFile(metaFilePath);
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

	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_LOG_DEBUG("[AssetMeta] ERROR: Platform not available for asset discovery!");
		return;
	}
	std::vector<uint8_t> metaFileData = platform->ReadAsset(metaFilePath);
	rapidjson::Document doc;
	if (!metaFileData.empty()) {
		rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
		doc.ParseStream(ms);
	}
	if (doc.HasParseError()) {
		ENGINE_LOG_DEBUG("[AssetMeta]: Rapidjson parse error: " + metaFilePath);
	}

	const auto& assetMetaData = doc["TextureMetaData"];
	if (assetMetaData.HasMember("id")) {
		ID = static_cast<uint32_t>(assetMetaData["id"].GetInt());
	}

	if (assetMetaData.HasMember("type")) {
		type = assetMetaData["type"].GetString();
	}

	if (assetMetaData.HasMember("unit")) {
		unit = assetMetaData["unit"].GetInt();
	}
}
