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
	REFL_REGISTER_PROPERTY(dialoguePath)
REFL_REGISTER_END
#pragma endregion

//HELPER FUNCTIONS
std::string VideoComponent::PadNumber(int num)
{
	std::ostringstream oss;
	oss << std::setw(5) << std::setfill('0') << num;

	return oss.str();
}


//Called once 
bool VideoComponent::ProcessMetaData(std::string resourcePath) {
	if (resourcePath.empty()) return false;

	Asset::Cutscene data(resourcePath);

	if (data.cutscenes.empty()) {
		return false;
	}
	auto it = data.cutscenes.begin();
	const Asset::CutsceneInfo& info = it->second;

	// Populate component fields
	this->cutSceneName	= it->first;
	this->frameStart	= info.frameStart;
	this->frameEnd		= info.frameEnd;
	this->activeFrame	= info.frameStart;
	this->preTime		= info.preTime;
	this->duration		= info.duration;
	this->postTime		= info.postTime;
	return true;
}

//Called Once
bool VideoComponent::ProcessDialogueData(std::string dialoguePath)
{
	std::ifstream file(dialoguePath);
	if (!file.is_open()) return false;

	std::string line;
	while (std::getline(file, line))
	{
		// Skip empty lines or comments
		if (line.empty() || line.find("Frame") == std::string::npos)
			continue;

		// 1. Find the position of the colon ':'
		size_t colonPos = line.find(":");
		if (colonPos == std::string::npos) continue;

		// 2. Extract the Frame Number part (e.g., "Frame 1 ")
		// We look for the space after "Frame" and before the colon
		size_t firstSpace = line.find(" ");
		std::string frameNumStr = line.substr(firstSpace + 1, colonPos - firstSpace - 1);

		int frameNum = std::stoi(frameNumStr);

		// 3. Extract the Dialogue part (everything after the colon)
		std::string dialogueText = line.substr(colonPos + 1);

		// 4. Clean up leading/trailing whitespace from the text
		size_t firstChar = dialogueText.find_first_not_of(" ");
		if (firstChar != std::string::npos) {
			dialogueText = dialogueText.substr(firstChar);
		}
		dialogueMap[frameNum] = dialogueText;
	}
	return true;
}

