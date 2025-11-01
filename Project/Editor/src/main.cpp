#include "Engine.h"
#include "GameManager.h"
#include "GUIManager.hpp"
#include <iostream>
#include "imgui.h"
#include "WindowManager.hpp"
#include "TimeManager.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Logging.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "Scripting.h"

int main() {
    ENGINE_PRINT("=== EDITOR BUILD ===");

    if (!glfwInit()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to initialize GLFW!");
        return -1;
    }

    Engine::Initialize();
    Engine::InitializeGraphicsResources(); // Load scenes and setup graphics

    GLFWwindow* window = WindowManager::getWindow();
    if (!window) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to create GLFW window!\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    GameManager::Initialize();
	GUIManager::Initialize();

    Scripting::Initialize("scripts/test.lua");

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (Engine::IsRunning()) {
        // Begin frame profiling
        PerformanceProfiler::GetInstance().BeginFrame();
        
        //Update deltaTime at start of Frame
        //TimeManager::UpdateDeltaTime();

            Engine::Update();
            GameManager::Update();
            Engine::StartDraw();
            //Engine::Draw();
            GUIManager::Render();
            Engine::EndDraw();
		
        // WindowManager handles buffer swapping for editor
        //WindowManager::SwapBuffers();
        

            auto now = clock::now();
            std::chrono::duration<float> delta = now - last;
            last = now;
            float dt = delta.count();

            // Update engine/editor/game state...

            Scripting::Tick(dt); // hot-reload check + call update(dt)

            // break condition for demo
            std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // End frame profiling
        PerformanceProfiler::GetInstance().EndFrame();
    }
    Scripting::Shutdown();
	GUIManager::Exit();
    GameManager::Shutdown();
    Engine::Shutdown();
    MetaFilesManager::CleanupUnusedMetaFiles();

    ENGINE_PRINT("=== Editor ended ===\n");
    return 0;
}