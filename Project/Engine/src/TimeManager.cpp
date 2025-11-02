#include "pch.h"
#include "TimeManager.hpp"
#include "RunTimeVar.hpp"

#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

// Static member initialization for fixed timestep
static double fixedDeltaTime = 1.0 / 60.0; // Default 60 FPS physics
static double accumulator = 0.0;
static int frameCount = 0;
static double fpsUpdateTimer = 0.0;
static double currentFps = 0.0;

void TimeManager::UpdateDeltaTime() {

    double currentTime = WindowManager::GetPlatform() ? WindowManager::GetPlatform()->GetTime() : 0.0;

    // Update deltaTime
    RunTimeVar::deltaTime = currentTime - RunTimeVar::lastFrameTime;
    RunTimeVar::lastFrameTime = currentTime;

    // Cap delta time to (4 FPS minimum)
    if (RunTimeVar::deltaTime > 0.25) {
        RunTimeVar::deltaTime = 0.25;
    }

    // Accumulate time for fixed updates
    accumulator += RunTimeVar::deltaTime;

    // Update FPS counter
    frameCount++;
    fpsUpdateTimer += RunTimeVar::deltaTime;
    if (fpsUpdateTimer >= 1.0) {
        currentFps = frameCount / fpsUpdateTimer;
        frameCount = 0;
        fpsUpdateTimer = 0.0;
    }
}

double TimeManager::GetDeltaTime() {
    return RunTimeVar::deltaTime;
}
double TimeManager::GetFps() {
    return currentFps;
}

double TimeManager::GetFixedDeltaTime() {
    return fixedDeltaTime;
}
