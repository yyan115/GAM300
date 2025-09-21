#pragma once
#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

class ENGINE_API TimeManager {
public:
	static void UpdateDeltaTime();
	static double GetDeltaTime();
	static double GetFps();
};
