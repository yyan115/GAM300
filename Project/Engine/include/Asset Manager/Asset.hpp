#pragma once
#include <string>
#include "Utilities/GUID.hpp"
#include "Asset Manager/AssetMeta.hpp"
#include "../Engine.h"

class IAsset {
public:
	virtual ~IAsset() = default;
	virtual std::string CompileToResource(const std::string& assetPath) = 0;
	virtual bool LoadResource(const std::string& assetPath) { assetPath;  return true; }
	virtual bool ReloadResource(const std::string& assetPath) { assetPath;  return true; }
	ENGINE_API std::shared_ptr<AssetMeta> GenerateBaseMetaFile(GUID_128 guid128, const std::string& assetPath, const std::string& resourcePath);
	virtual std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) = 0;
};