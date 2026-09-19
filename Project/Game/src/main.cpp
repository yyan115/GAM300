#include "Engine.h"
#include "GameManager.h"
#include <iostream>
#include <string_view>
#include "Logging.hpp"

// The console is a build option, not a code edit. GAME_CONSOLE is set by
// Project/Game/CMakeLists.txt, which also picks the linker subsystem. The two
// have to agree: the subsystem decides whether the process starts attached to
// a console, this decides whether it creates one, and a GUI-subsystem build
// that still calls AllocConsole gets its console straight back.
//
// Defaulted ON so a build that does not go through that option behaves as it
// always did.
#ifndef GAME_CONSOLE
#define GAME_CONSOLE 1
#endif
#if GAME_CONSOLE
#define SHOW_CONSOLE
#endif

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

int main(int argc, char* argv[]) {
#ifdef _WIN32
#ifdef SHOW_CONSOLE
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
#endif
#endif
    ENGINE_PRINT("=== GAME BUILD ===\n");

    bool startWindowed = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--windowed") startWindowed = true;
    }
    Engine::Initialize(startWindowed);
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
