#pragma once
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

typedef struct FMOD_SOUND FMOD_SOUND;

class Audio : public IAsset {
public:
	FMOD_SOUND* sound{};
	std::string assetPath{};

	Audio() = default;
	Audio(std::shared_ptr<AssetMeta> assetMeta);

	// Match IAsset interface
	std::string ENGINE_API CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	bool ENGINE_API LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	bool ENGINE_API ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ENGINE_API ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;
};