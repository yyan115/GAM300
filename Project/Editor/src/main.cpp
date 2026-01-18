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
#include "ECS/TagsLayersSettings.hpp"

int main()
{
    ENGINE_PRINT("=== EDITOR BUILD ===");

    if (!glfwInit()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to initialize GLFW!");
        return -1;
    }

    Engine::Initialize();

    // Load project-wide tags, layers, and sorting layers
    ENGINE_PRINT("Loading project settings...");
    TagsLayersSettings::GetInstance().LoadSettings();

    Engine::InitializeGraphicsResources(); // Load scenes and setup graphics

    // Initialize Scripting subsystem for Lua support
    ENGINE_PRINT("Initializing Scripting runtime...");
    if (Scripting::Init())
    {
        ENGINE_PRINT("Scripting runtime initialized successfully");
    }
    else
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to initialize Scripting runtime!");
    }

    GLFWwindow* window = WindowManager::getWindow();
    if (!window) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Failed to create GLFW window!\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    GameManager::Initialize();
	GUIManager::Initialize();

    while (Engine::IsRunning()) 
    {
        // Begin frame profiling
        PerformanceProfiler::GetInstance().BeginFrame();
        
        //Update deltaTime at start of Frame
        //TimeManager::UpdateDeltaTime();

            Engine::Update();
            GameManager::Update();

            // --- end scripting/hot-reload work ---
            Engine::StartDraw();
            //Engine::Draw();
            GUIManager::Render();
            Engine::EndDraw();
		
        // WindowManager handles buffer swapping for editor
        //WindowManager::SwapBuffers();
     
        // End frame profiling
        PerformanceProfiler::GetInstance().EndFrame();
    }

	GUIManager::Exit();
    GameManager::Shutdown();

    // Shutdown Scripting runtime
    ENGINE_PRINT("Shutting down Scripting runtime...");
    Scripting::Shutdown();

    Engine::Shutdown();
    MetaFilesManager::CleanupUnusedMetaFiles();
    EngineLogging::Shutdown();

    // Add window cleanup before exit
    WindowManager::Exit();

    ENGINE_PRINT("=== Editor ended ===\n");
    return 0;
}