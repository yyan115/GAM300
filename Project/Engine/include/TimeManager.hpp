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

	// TIME SCALE (1.0 = normal, 0.0 = full freeze, 0.5 = half speed)
	static void SetTimeScale(float scale) { timeScale = scale; }
	static float GetTimeScale() { return timeScale; }

private:
	inline static bool isPaused = false;
	inline static float timeScale = 1.0f;
	inline static double fixedDeltaTime = 1.0 / 60.0; // Default 60 FPS physics
	inline static double accumulator = 0.0;
	inline static int frameCount = 0;
	inline static double fpsUpdateTimer = 0.0;
	inline static double currentFps = 0.0;

	// Delta time smoothing — averages over recent frames to prevent jitter
	static constexpr int DT_HISTORY_SIZE = 5;
	inline static double dtHistory[DT_HISTORY_SIZE] = {};
	inline static int dtHistoryIndex = 0;
	inline static bool dtHistoryFilled = false;


};
