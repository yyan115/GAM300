#pragma once
#include "Engine.h"


class ENGINE_API TimeManager {
public:
	static void UpdateDeltaTime();
	static double GetDeltaTime();
	static double GetFps();
};
