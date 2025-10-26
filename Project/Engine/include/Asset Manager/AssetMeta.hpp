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
		Texture,
		Model
	};

	GUID_128 guid{};
	std::string sourceFilePath;
	std::string compiledFilePath;
	std::string androidCompiledFilePath;
	std::chrono::system_clock::time_point lastCompileTime;
	int version{};

	ENGINE_API void PopulateAssetMeta(GUID_128 _guid, const std::string& _sourcePath, const std::string& _compiledPath, int _ver, const std::string& _androidCompiledPath = "");
	ENGINE_API virtual void PopulateAssetMetaFromFile(const std::string& metaFilePath);
	virtual Type GetType() const { return Type::Base; }
};

class TextureMeta : public AssetMeta {
public:
	ENGINE_API static const std::array<std::string, 3> textureTypes;
	std::string type;
	bool flipUVs = true;
	bool generateMipmaps = true;

	ENGINE_API void PopulateTextureMeta(const std::string& _type, bool _flipUVs, bool _generateMipmaps);
	ENGINE_API void PopulateAssetMetaFromFile(const std::string& metaFilePath) override;
	Type GetType() const override { return Type::Texture; }
};

class ModelMeta : public AssetMeta {
public:
	bool optimizeMeshes = true;

	ENGINE_API void PopulateModelMeta(bool _optimizeMesh);
	ENGINE_API void PopulateAssetMetaFromFile(const std::string& metaFilePath) override;
	Type GetType() const override { return Type::Model; }
};