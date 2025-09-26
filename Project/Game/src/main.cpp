#include "Engine.h"
#include "GameManager.h"
#include <iostream>

int main() {
    std::cout << "=== GAME BUILD ===" << std::endl;

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

    std::cout << "=== Game ended ===" << std::endl;
    return 0;
}