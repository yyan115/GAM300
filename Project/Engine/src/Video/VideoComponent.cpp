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