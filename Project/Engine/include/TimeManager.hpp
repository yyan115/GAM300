#pragma once
#include "Engine.h"


class ENGINE_API TimeManager {
public:
	static void UpdateDeltaTime();
	static double GetDeltaTime();
	static double GetUnscaledDeltaTime();
	static double GetFps();
	static double GetFixedDeltaTime();

	//PAUSE CONTROLS
	static void SetPaused(bool paused) { isPaused = paused; }
	static bool IsPaused() { return isPaused; }

private:
	inline static bool isPaused = false; 
	inline static double fixedDeltaTime = 1.0 / 60.0; // Default 60 FPS physics
	inline static double accumulator = 0.0;
	inline static int frameCount = 0;
	inline static double fpsUpdateTimer = 0.0;
	inline static double currentFps = 0.0;


};
