#pragma once
#include "Utilities/GUID.hpp"
#include <string>
#include <chrono>

class AssetMeta {
public:
	GUID_128 guid{};
	std::string sourceFilePath;
	std::string compiledFilePath;
	std::string androidCompiledFilePath;
	std::chrono::system_clock::time_point lastCompileTime;
	int version{};

	void PopulateAssetMeta(GUID_128 _guid, const std::string& _sourcePath, const std::string& _compiledPath, int _ver);
	virtual void PopulateAssetMetaFromFile(const std::string& metaFilePath);
};

class TextureMeta : public AssetMeta {
public:
	uint32_t ID{};
	std::string type;
	uint32_t unit{};

	void PopulateTextureMeta(uint32_t _ID, const std::string& _type, uint32_t _unit);
	void PopulateAssetMetaFromFile(const std::string& metaFilePath) override;
};