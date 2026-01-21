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



//Called once 
bool VideoComponent::ProcessMetaData(std::string resourcePath) {
	if (resourcePath.empty()) return false;

	Asset::Cutscene data(resourcePath);

	if (data.cutscenes.empty()) {
		return false;
	}
	auto it = data.cutscenes.begin();
	const Asset::CutsceneInfo& info = it->second;

	//it->first		//"Board"
	////cutSceneName -> it->first + "_info.framestart pad with 0s" 

	//std::string numResult = "_" + PadNumber(info.frameStart);

	//std::string cutSceneName = it->first + numResult + ".png";

	// Populate component fields
	this->cutSceneName	= it->first;
	this->frameStart	= info.frameStart;
	this->frameEnd		= info.frameEnd;
	this->activeFrame	= info.frameStart;
	this->preTime		= info.preTime;
	this->duration		= info.duration;
	this->postTime		= info.postTime;
	this->videoPath		= resourcePath;

	return true;
}

std::string VideoComponent::PadNumber(int num)
{
	std::ostringstream oss;
	oss << std::setw(5) << std::setfill('0') << num;

	return oss.str();
}

