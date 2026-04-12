#include "Engine.h"
#include "GameManager.h"
#include <iostream>
#include "Logging.hpp"

// Comment out to hide console window
// #define SHOW_CONSOLE

#ifdef _WIN32
#ifdef SHOW_CONSOLE
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

// Force discrete GPU on laptops with hybrid graphics (NVIDIA Optimus / AMD PowerXpress).
// Without this, Windows graphics drivers default to the integrated GPU when the game is
// launched from an "unknown" install path, capping the game at ~30fps due to VSync half-rate.
// These exported symbols are looked up by the NVIDIA / AMD drivers at process start.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main() {
#ifdef _WIN32
#ifdef SHOW_CONSOLE
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
#endif
#endif
    ENGINE_PRINT("=== GAME BUILD ===\n");

    Engine::Initialize();
    Engine::InitializeGraphicsResources(); // Load scenes and setup graphics
    GameManager::Initialize();

    while (Engine::IsRunning()) {

        Engine::Update();
        GameManager::Update();

        Engine::StartDraw();
        Engine::Draw();
        Engine::EndDraw();
    }

    GameManager::Shutdown();
    Engine::Shutdown();
    ENGINE_PRINT("=== Game ended ===\n");
    return 0;
}