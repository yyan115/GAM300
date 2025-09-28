#pragma once
#include "Asset Manager/Asset.hpp"

typedef struct FMOD_SOUND FMOD_SOUND;

class ENGINE_API Audio : public IAsset
{
public:
	Audio();
	~Audio() = default;

	Audio(const Audio &) = delete;
	Audio &operator=(const Audio &) = delete;
	Audio(Audio &&) = default;
	Audio &operator=(Audio &&) = default;

	FMOD_SOUND *sound;
	std::string assetPath;

	std::string CompileToResource(const std::string &assetPath) override;
	bool LoadResource(const std::string &assetPath) override;
	bool ReloadResource(const std::string &assetPath) override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string &assetPath, std::shared_ptr<AssetMeta> currentMetaData) override;
};