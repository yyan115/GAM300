#include "Engine.h"
#include "GameManager.h"
#include <iostream>
#include "Logging.hpp"

int main() {
    ENGINE_PRINT("=== GAME BUILD ===\n");
    //std::cout << "=== GAME BUILD ===" << std::endl;

    Engine::Initialize();
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
    //std::cout << "=== Game ended ===" << std::endl;
    return 0;
}