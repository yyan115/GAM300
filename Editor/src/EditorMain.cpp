#include "EditorMain.hpp"
#include "Game.hpp"
#include "Engine.hpp"
#include <iostream>

int main()
{
    std::cout << "[Editor] Starting up Editor...\n";

    // Possibly initialize Engine and Game for editing:
    Engine::Init();
    Game::Initialize();

    // Example usage:
    Editor::LaunchEditor();

    // Shut down
    Game::Shutdown();
    Engine::Shutdown();
    return 0;
}
