#include "pch.h"
#include "TimeManager.hpp"
#include "RunTimeVar.hpp"


void TimeManager::UpdateDeltaTime() {
    const double targetDeltaTime = 1.0 / 60.0; // cap at 60fps

    //double currentTime = platform ? platform->GetTime() : 0.0;
    double currentTime = glfwGetTime();
    double frameTime = currentTime - RunTimeVar::lastFrameTime;

    double remainingTime = targetDeltaTime - frameTime;

    //Limit to 60 FPS?
    //// Sleep only if we have at least 5 ms remaining
    //if (remainingTime > 0.005) {
    //    std::this_thread::sleep_for(std::chrono::milliseconds((int)((remainingTime - 0.001) * 1000)));
    //}
    //// Busy-wait the last few milliseconds - now handled by platform
    //while ((platform->GetTime() - lastFrameTime) < targetDeltaTime) {}

    // Update deltaTime
    //currentTime = platform ? platform->GetTime() : 0.0;
    currentTime = glfwGetTime();
    RunTimeVar::deltaTime = currentTime - RunTimeVar::lastFrameTime;
    RunTimeVar::lastFrameTime = currentTime;
    // Swap interval handled by platform internally
}

double TimeManager::GetDeltaTime() {
    return RunTimeVar::deltaTime;
}
double TimeManager::GetFps() {
    return RunTimeVar::deltaTime > 0.0 ? 1.0 / RunTimeVar::deltaTime : 0.0;
}
