#pragma once
#include "Utilities/GUID.hpp"
#include <string>
#include <chrono>
#include <string>
#include <array>

class AssetMeta {
public:
	enum class Type { 
		Base, 
		Texture 
	};

	GUID_128 guid{};
	std::string sourceFilePath;
	std::string compiledFilePath;
	std::string androidCompiledFilePath;
	std::chrono::system_clock::time_point lastCompileTime;
	int version{};

	void PopulateAssetMeta(GUID_128 _guid, const std::string& _sourcePath, const std::string& _compiledPath, int _ver);
	virtual void PopulateAssetMetaFromFile(const std::string& metaFilePath);
	virtual Type GetType() const { return Type::Base; }
};

class TextureMeta : public AssetMeta {
public:
	ENGINE_API static const std::array<std::string, 3> textureTypes;
	std::string type;
	bool flipUVs = true;
	bool generateMipmaps = true;

	void PopulateTextureMeta(const std::string& _type, bool _flipUVs, bool _generateMipmaps);
	void PopulateAssetMetaFromFile(const std::string& metaFilePath) override;
	Type GetType() const override { return Type::Texture; }
};