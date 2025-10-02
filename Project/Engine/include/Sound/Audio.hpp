#pragma once
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

typedef struct FMOD_SOUND FMOD_SOUND;

class ENGINE_API Audio : public IAsset {
public:
	FMOD_SOUND* sound;
	std::string assetPath;

	// Match IAsset interface
	std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;
};