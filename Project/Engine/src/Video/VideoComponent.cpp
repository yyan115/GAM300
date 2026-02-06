#include "pch.h"
#include "Video/VideoComponent.hpp"
#include <algorithm>
#include <sstream>

#ifdef ANDROID
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#endif

#pragma region Reflection
REFL_REGISTER_START(VideoComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(isPlaying)
	REFL_REGISTER_PROPERTY(loop)
	REFL_REGISTER_PROPERTY(playbackSpeed)
	REFL_REGISTER_PROPERTY(currentTime)
	REFL_REGISTER_PROPERTY(videoPath)
	REFL_REGISTER_PROPERTY(dialoguePath)
	REFL_REGISTER_PROPERTY(cutsceneEnded)
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
	ENGINE_PRINT("[VideoComponent] ProcessMetaData called with path: '", resourcePath, "'\n");
	if (resourcePath.empty()) {
		ENGINE_PRINT("[VideoComponent] ProcessMetaData: path is empty, returning false\n");
		return false;
	}

	Asset::Cutscene data(resourcePath);

	if (data.cutscenes.empty()) {
		ENGINE_PRINT("[VideoComponent] ProcessMetaData: cutscenes map is empty after parsing '", resourcePath, "'\n");
		return false;
	}
	auto it = data.cutscenes.begin();
	const Asset::CutsceneInfo& info = it->second;

	// Populate component fields
	this->cutSceneName	= it->first;
	this->frameStart	= info.frameStart;
	this->frameEnd		= info.frameEnd;
	this->activeFrame	= info.frameStart;

	// New timing system
	this->fadeDuration		= info.fadeDuration;
	this->boardDuration		= info.boardDuration;
	this->panelDuration		= info.panelDuration;
	this->skipFadeDuration	= info.skipFadeDuration;

	// Legacy timing (for backwards compatibility)
	this->preTime		= info.preTime;
	this->duration		= info.duration;
	this->postTime		= info.postTime;

	ENGINE_PRINT("[VideoComponent] ProcessMetaData SUCCESS: cutSceneName='", this->cutSceneName,
		"', frames=", this->frameStart, "-", this->frameEnd,
		", fadeDuration=", this->fadeDuration, ", boardDuration=", this->boardDuration, "\n");
	return true;
}

//Called Once
bool VideoComponent::ProcessDialogueData(std::string dialoguePath)
{
	std::string fileContent;

#ifdef ANDROID
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_PRINT("[VideoComponent] ERROR: Platform is null, cannot read dialogue '", dialoguePath, "'\n");
		return false;
	}
	std::string assetPath = dialoguePath;
	std::replace(assetPath.begin(), assetPath.end(), '\\', '/');
	while (assetPath.size() >= 3 && assetPath.substr(0, 3) == "../") {
		assetPath = assetPath.substr(3);
	}
	ENGINE_PRINT("[VideoComponent] Reading dialogue asset: '", assetPath, "' (original: '", dialoguePath, "')\n");
	std::vector<uint8_t> buffer = platform->ReadAsset(assetPath);
	if (buffer.empty()) {
		ENGINE_PRINT("[VideoComponent] ERROR: Failed to read dialogue '", assetPath, "' (buffer empty)\n");
		return false;
	}
	fileContent.assign(buffer.begin(), buffer.end());
	ENGINE_PRINT("[VideoComponent] Read ", fileContent.size(), " bytes of dialogue from '", assetPath, "'\n");
#else
	std::ifstream file(dialoguePath);
	if (!file.is_open()) return false;
	std::stringstream ss;
	ss << file.rdbuf();
	fileContent = ss.str();
#endif

	std::istringstream contentStream(fileContent);
	std::string line;
	while (std::getline(contentStream, line))
	{
		// Skip empty lines or comments
		if (line.empty() || line[0] == '#')
			continue;

		// Support "Board N :", "Panel N :", and "Frame N :" formats
		bool isBoard = line.find("Board") != std::string::npos;
		bool isPanel = line.find("Panel") != std::string::npos;
		bool isFrame = line.find("Frame") != std::string::npos;

		if (!isBoard && !isPanel && !isFrame)
			continue;

		// 1. Find the position of the colon ':'
		size_t colonPos = line.find(":");
		if (colonPos == std::string::npos) continue;

		// 2. Extract the number part
		size_t firstSpace = line.find(" ");
		std::string numStr = line.substr(firstSpace + 1, colonPos - firstSpace - 1);
		// Clean whitespace
		numStr.erase(std::remove_if(numStr.begin(), numStr.end(), ::isspace), numStr.end());

		int num = std::stoi(numStr);

		// 3. Extract the Dialogue part (everything after the colon)
		std::string dialogueText = line.substr(colonPos + 1);

		// 4. Clean up leading/trailing whitespace from the text
		size_t firstChar = dialogueText.find_first_not_of(" ");
		if (firstChar != std::string::npos) {
			dialogueText = dialogueText.substr(firstChar);
		}

		if (isPanel)
		{
			// Store panel text - same text for all frames in panel
			panelDialogueMap[num] = dialogueText;
		}
		else if (isBoard || isFrame)
		{
			// Board/Frame-based dialogue - text per individual board
			dialogueMap[num] = dialogueText;
		}
	}
	return true;
}

