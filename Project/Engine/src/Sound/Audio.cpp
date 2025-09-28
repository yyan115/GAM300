#include "pch.h"

#include "Sound/Audio.hpp"
#include "Sound/AudioSystem.hpp"

std::string Audio::CompileToResource(const std::string& assetPath) {
	return assetPath; // fmod nothing to compile
}

bool Audio::LoadResource(const std::string& assetPath) {
	// Use the AudioSystem to create a FMOD sound now that AudioManager is gone
	sound = AudioSystem::GetInstance().CreateSound(assetPath);
	this->assetPath = assetPath;
	return sound != nullptr;
}

bool Audio::ReloadResource(const std::string& assetPath) {
	// If we already have a sound loaded, unload it first
	if (sound && !this->assetPath.empty()) {
		AudioSystem::GetInstance().ReleaseSound(sound, this->assetPath);
		sound = nullptr;
	}

	// Load the new resource
	return LoadResource(assetPath);
}

std::shared_ptr<AssetMeta> Audio::ExtendMetaFile(const std::string& /*assetPath*/, std::shared_ptr<AssetMeta> currentMetaData) {
	// For audio files, we don't need extended metadata beyond the base
	return currentMetaData;
}