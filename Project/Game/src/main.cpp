#include "Engine.h"
#include "GameManager.h"
#include <iostream>
#include "Logging.hpp"

// Comment out to hide console window
#define SHOW_CONSOLE

#ifdef _WIN32
#ifdef SHOW_CONSOLE
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif
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