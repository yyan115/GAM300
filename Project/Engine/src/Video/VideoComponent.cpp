#include "pch.h"
#include "Video/VideoComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(VideoComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(isPlaying)
	REFL_REGISTER_PROPERTY(loop)
	REFL_REGISTER_PROPERTY(playbackSpeed)
	REFL_REGISTER_PROPERTY(currentTime)
	REFL_REGISTER_PROPERTY(videoPath)
REFL_REGISTER_END
#pragma endregion

bool VideoComponent::ProcessMetaData(std::string resourcePath) {
	if (resourcePath.empty()) return false;

	Asset::Cutscene data(resourcePath);

	if (data.cutscenes.empty()) {
		return false;
	}
	auto it = data.cutscenes.begin();
	const Asset::CutsceneInfo& info = it->second;

	// Populate component fields
	this->cutSceneName = it->first;
	this->frameStart = info.frameStart;
	this->frameEnd = info.frameEnd;
	this->preTime = info.preTime;
	this->duration = info.duration;
	this->postTime = info.postTime;
	this->videoPath = resourcePath;

	return true;
}
