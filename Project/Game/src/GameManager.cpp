#include "GameManager.h"

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

// Define the static member
bool GameManager::initialized = false;

void GameManager::Initialize() {
    if (!initialized) {
        std::cout << "GameManager initialized!" << std::endl;

        initialized = true;
    }
}

void GameManager::Update() {
    // Game logic here
}

void GameManager::Shutdown() {
    if (initialized) {
        std::cout << "GameManager shut down!" << std::endl;
        initialized = false;
    }
}
