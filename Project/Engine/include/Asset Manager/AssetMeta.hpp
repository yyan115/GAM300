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
	static const std::array<std::string, 9> textureTypes;
	std::string type;
	bool flipUVs = false;
	bool generateMipmaps = true;
	int maxSize = 2048; // Max texture dimension at import time. Larger textures are downscaled.

	enum class TextureWrapMode {
		Clamp = 0,
		Repeat
	};

	static const std::array<std::string, 2> textureWrapModes;
	TextureWrapMode textureWrapMode = TextureWrapMode::Clamp; // Set to Clamp by default
	std::string textureWrapModeStr = "Clamp"; // String representation for serialization

	void PopulateTextureMeta(const std::string& _type, bool _flipUVs, bool _generateMipmaps, const TextureWrapMode& _textureWrapMode, int _maxSize = 2048);
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