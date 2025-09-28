#pragma once
#include "Asset Manager/Asset.hpp"

typedef struct FMOD_SOUND FMOD_SOUND;

class Audio : public IAsset {
public:
	FMOD_SOUND* sound;
	std::string assetPath;

	ENGINE_API std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	ENGINE_API bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	ENGINE_API bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	ENGINE_API std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;
};