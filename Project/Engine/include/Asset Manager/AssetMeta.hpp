#pragma once
#include "Utilities/GUID.hpp"
#include <string>
#include <chrono>
#include <string>
#include <array>

class ENGINE_API AssetMeta {
public:
	enum class Type {
		Base,
		Texture,
		Model
	};

	GUID_128 guid{};
	std::string sourceFilePath;
	std::string compiledFilePath;
	std::string androidCompiledFilePath;
	//std::chrono::system_clock::time_point lastCompileTime;
	int version{};

	void PopulateAssetMeta(GUID_128 _guid, const std::string& _sourcePath, const std::string& _compiledPath, int _ver, const std::string& _androidCompiledPath = "");
	virtual void PopulateAssetMetaFromFile(const std::string& metaFilePath);
	virtual Type GetType() const { return Type::Base; }
};

class ENGINE_API TextureMeta : public AssetMeta {
public:
	static const std::array<std::string, 3> textureTypes;
	std::string type;
	bool flipUVs = true;
	bool generateMipmaps = true;

	void PopulateTextureMeta(const std::string& _type, bool _flipUVs, bool _generateMipmaps);
	void PopulateAssetMetaFromFile(const std::string& metaFilePath) override;
	Type GetType() const override { return Type::Texture; }
};

class ENGINE_API ModelMeta : public AssetMeta {
public:
	bool optimizeMeshes = true;
	bool generateLODs = false;

	void PopulateModelMeta(bool _optimizeMesh);
	void PopulateAssetMetaFromFile(const std::string& metaFilePath) override;
	Type GetType() const override { return Type::Model; }
};