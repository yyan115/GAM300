#include "pch.h"
#include "Asset Manager/AssetMeta.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include <rapidjson/document.h>
#include "Logging.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>

const std::array<std::string, 8> TextureMeta::textureTypes = { "diffuse", "specular", "normal", "emissive", "metallic", "roughness", "ao", "height" };
const std::array<std::string, 2> TextureMeta::textureWrapModes = { "Clamp", "Repeat" };

void AssetMeta::PopulateAssetMeta(GUID_128 _guid, const std::string& _sourcePath, const std::string& _compiledPath, int _ver, const std::string& _androidCompiledPath)
{
	guid = _guid;
	sourceFilePath = _sourcePath;
	compiledFilePath = _compiledPath;
	androidCompiledFilePath = _androidCompiledPath;
	version = _ver;
	//lastCompileTime = std::chrono::system_clock::now();
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

	if (!doc.IsObject()) return;

	if (!doc.HasMember("AssetMetaData")) return;

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

	ENGINE_LOG_DEBUG("Populated AssetMeta from file: " + metaFilePath);

	//lastCompileTime = MetaFilesManager::GetLastCompileTimeFromMetaFile(metaFilePath);
}

void TextureMeta::PopulateTextureMeta(const std::string& _type, bool _flipUVs, bool _generateMipmaps, const TextureWrapMode& _textureWrapMode, int _maxSize)
{
	type = _type;
	flipUVs = _flipUVs;
	generateMipmaps = _generateMipmaps;
	textureWrapMode = _textureWrapMode;
	textureWrapModeStr = textureWrapModes[static_cast<int>(_textureWrapMode)];
	maxSize = _maxSize;
}

void TextureMeta::PopulateAssetMetaFromFile(const std::string& metaFilePath) {
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
	if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("TextureMetaData")) {
		ENGINE_LOG_WARN("[AssetMeta]: Rapidjson parse error: " + metaFilePath);
		return;
	}

	const auto& assetMetaData = doc["TextureMetaData"];
	if (assetMetaData.HasMember("type")) {
		type = assetMetaData["type"].GetString();
	}
	if (assetMetaData.HasMember("flipUVs")) {
		flipUVs = assetMetaData["flipUVs"].GetBool();
	}
	if (assetMetaData.HasMember("generateMipmaps")) {
		generateMipmaps = assetMetaData["generateMipmaps"].GetBool();
	}
	if (assetMetaData.HasMember("textureWrapMode")) {
		textureWrapModeStr = assetMetaData["textureWrapMode"].GetString();
		for (size_t i = 0; i < textureWrapModes.size(); ++i) {
			if (textureWrapModes[i] == textureWrapModeStr) {
				textureWrapMode = static_cast<TextureWrapMode>(i);
				break;
			}
		}
	}
	if (assetMetaData.HasMember("maxSize")) {
		maxSize = assetMetaData["maxSize"].GetInt();
	}
}

ENGINE_API void ModelMeta::PopulateModelMeta(bool _optimizeMesh) {
	optimizeMeshes = _optimizeMesh;
}

ENGINE_API void ModelMeta::PopulateAssetMetaFromFile(const std::string& metaFilePath) {
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

	if (!doc.HasMember("ModelMetaData")) {
		return;
	}

	const auto& assetMetaData = doc["ModelMetaData"];
	if (assetMetaData.HasMember("optimizeMeshes")) {
		optimizeMeshes = assetMetaData["optimizeMeshes"].GetBool();
	}
}
