#include "pch.h"

#include "Sound/Audio.hpp"
#include "Sound/AudioManager.hpp"

std::string Audio::CompileToResource(const std::string& assetPath) {
	return assetPath; // fmod nothing to compile
}

bool Audio::LoadResource(const std::string& assetPath) {
	sound = AudioManager::GetInstance().LoadSound(assetPath);
	this->assetPath = assetPath;
	return sound != nullptr;
}

bool Audio::ReloadResource(const std::string& assetPath) {
	return LoadResource(assetPath);
}

std::shared_ptr<AssetMeta> Audio::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) {
	assetPath, currentMetaData;
	return std::shared_ptr<AssetMeta>();
}
