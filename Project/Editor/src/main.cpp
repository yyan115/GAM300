#include "Engine.h"
#include "GameManager.h"
#include "GUIManager.hpp"
#include <iostream>
#include "imgui.h"
#include "WindowManager.hpp"
#include "TimeManager.hpp"
#include "Logging.hpp"


int main() {
    ENGINE_PRINT("=== EDITOR BUILD ===", EngineLogging::LogLevel::Info, false);

    if (!glfwInit()) {
        ENGINE_PRINT("Failed to initialize GLFW!", EngineLogging::LogLevel::Error, false);
        return -1;
    }

    Engine::Initialize();

    GLFWwindow* window = WindowManager::getWindow();
    if (!window) {
        std::cerr << "Faileasdd to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    GameManager::Initialize();
	GUIManager::Initialize();

    while (Engine::IsRunning()) {
        //Update deltaTime at start of Frame
        TimeManager::UpdateDeltaTime();

        Engine::Update();
        GameManager::Update();

        // Render 3D content to FBO
        Engine::StartDraw();
        //Engine::Draw();
        GUIManager::Render();
        Engine::EndDraw();
		
        // WindowManager handles buffer swapping for editor
        //WindowManager::SwapBuffers();
    }
	GUIManager::Exit();
    GameManager::Shutdown();
    Engine::Shutdown();

    std::cout << "=== Editor ended ===" << std::endl;
    return 0;
}