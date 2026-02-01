#include "pch.h"
#include "TimeManager.hpp"
#include "RunTimeVar.hpp"

#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

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
    RunTimeVar::unscaledDeltaTime = RunTimeVar::deltaTime; //Store for pause usage

    //IF PAUSED, SET DELTATIME TO 0
    if (isPaused)
    {
        RunTimeVar::deltaTime = 0;
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
double TimeManager::GetUnscaledDeltaTime()
{
    return RunTimeVar::unscaledDeltaTime;
}
