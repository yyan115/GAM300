#include "Game.hpp"
#include <iostream>
#include "Engine.hpp"

namespace Game
{
    void Initialize()
    {
        Engine::Init();
        std::cout << "[Game] Initialized.\n";
    }

    void RunLoop()
    {
        std::cout << "[Game] Running main loop...\n";
        // Put your game loop logic here...
        Engine::Update();
    }

    void Shutdown()
    {
        std::cout << "[Game] Shutdown.\n";
        Engine::Shutdown();
    }
}
