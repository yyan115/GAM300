#include "GameMain.hpp"
#include <iostream>

int main()
{
    std::cout << "[Game] Starting up...\n";
    Game::Initialize();
    Game::RunLoop();
    Game::Shutdown();
    return 0;
}
