#include "Engine.h"
#include "GameManager.h"
#include <iostream>
#include "Logging.hpp"

int main() {
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