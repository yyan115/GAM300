#include "pch.h"

#include "Sound/Audio.hpp"
#include "Sound/AudioManager.hpp"
#include <Asset Manager/AssetManager.hpp>
#include "Sound/AudioSystem.hpp"

std::string Audio::CompileToResource(const std::string& assetPath, bool forAndroid) {
	if (!forAndroid)
		return assetPath; // fmod nothing to compile
	else {
		// Ensure parent directories exist.
		std::filesystem::path p(AssetManager::GetInstance().GetAndroidResourcesPath() / assetPath);
		std::filesystem::create_directories(p.parent_path());
		// Copy the audio asset to the Android Resources directory.
		try {
			std::filesystem::copy_file(assetPath, p.generic_string(),
				std::filesystem::copy_options::overwrite_existing);
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::cerr << "[AUDIO] Copy failed: " << e.what() << std::endl;
		}
		return (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPath).generic_string();
	}
}

bool Audio::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
	sound = AudioSystem::GetInstance().CreateSound(assetPath);
	this->assetPath = resourcePath;
	return sound != nullptr;
}

bool Audio::ReloadResource(const std::string& resourcePath, const std::string& assetPath) {
	// If we already have a sound loaded, unload it first
	if (sound && !this->assetPath.empty()) {
		AudioSystem::GetInstance().ReleaseSound(sound, this->assetPath);
		sound = nullptr;
	}

	// Load the new resource
	return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Audio::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid) {
	assetPath, currentMetaData, forAndroid;
	return std::shared_ptr<AssetMeta>();
}
