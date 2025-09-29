#pragma once
#include <string>
#include "Utilities/GUID.hpp"
#include "Asset Manager/AssetMeta.hpp"
#include "../Engine.h"

class ENGINE_API IAsset {
public:
	virtual ~IAsset() = default;
	virtual std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) = 0;
	virtual bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") { resourcePath, assetPath;  return true; }
	virtual bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") { resourcePath, assetPath;  return true; }
	std::shared_ptr<AssetMeta> GenerateBaseMetaFile(GUID_128 guid128, const std::string& assetPath, const std::string& resourcePath, const std::string& androidResourcePath = "", bool forAndroid = false);
	virtual std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) = 0;
};