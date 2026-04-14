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

    // Smooth delta time over recent frames to prevent jitter from VSync fluctuations
    dtHistory[dtHistoryIndex] = RunTimeVar::deltaTime;
    dtHistoryIndex = (dtHistoryIndex + 1) % DT_HISTORY_SIZE;
    if (!dtHistoryFilled && dtHistoryIndex == 0) dtHistoryFilled = true;

    int count = dtHistoryFilled ? DT_HISTORY_SIZE : dtHistoryIndex;
    if (count > 0) {
        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += dtHistory[i];
        RunTimeVar::deltaTime = sum / count;
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
    else if (timeScale != 1.0f)
    {
        RunTimeVar::deltaTime *= timeScale;
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
