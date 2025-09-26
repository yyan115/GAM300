#pragma once
#include "Asset Manager/Asset.hpp"
#include "fmod.h"

class Audio : public IAsset {
public:
	FMOD_SOUND* sound;
	std::string assetPath;

	std::string CompileToResource(const std::string& assetPath) override;
	bool LoadResource(const std::string& assetPath) override;
	bool ReloadResource(const std::string& assetPath) override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) override;
};