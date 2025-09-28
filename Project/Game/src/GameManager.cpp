#include "GameManager.h"

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "Logging.hpp"

// Define the static member
bool GameManager::initialized = false;

void GameManager::Initialize() {
    if (!initialized) {
        ENGINE_PRINT("GameManager initialized!\n");
        //std::cout << "GameManager initialized!" << std::endl;

        initialized = true;
    }
}

void GameManager::Update() {
    // Game logic here
}

void GameManager::Shutdown() {
    if (initialized) {
        ENGINE_PRINT("GameManager shut down!\n");
        //std::cout << "GameManager shut down!" << std::endl;
        initialized = false;
    }
}
