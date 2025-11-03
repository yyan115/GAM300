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
/*
* REMOVE BEFORE SUBMISSION
// Demonstrates:
//  - Writing a test Lua file
//  - Initialize scripting runtime
//  - Start hot-reload watcher on the script file
//  - Tick script runtime repeatedly while polling the HotReloadManager
//  - Modify the script file to simulate a developer edit and trigger reload
//  - Create/destroy a per-script environment
//  - Inspect a Lua global value from C++
//  - Clean shutdown
*/
#pragma region LUA_TEST
#include "Scripting.h"

#define LUA_TEST 1
#pragma endregion

int main()
{
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

#if LUA_TEST
    
        // 1) initialize scripting
        Scripting::Init();
        Scripting::SetHostLogHandler([](const std::string& s) {
            // engine logging wrapper
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "%s", s.c_str());
            });

        // 3) optional: set custom filesystem read callback (editor can load scripts from different folders)
        Scripting::SetFileSystemReadAllText([](const std::string& path, std::string& out) -> bool {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) return false;
            std::ostringstream ss;
            ss << ifs.rdbuf();
            out = ss.str();
            return true;
            });

        // 4) Per-frame: Tick scripting
        float dt = 1.0f / 60.0f;

        // 5) create a script instance from file (path on disk)
        int inst = Scripting::CreateInstanceFromFile("Resources/Scripts/sample_mono_behavior.lua");
        if (Scripting::IsValidInstance(inst)) {
            Scripting::CallInstanceFunction(inst, "Awake");
            Scripting::CallInstanceFunction(inst, "Start");
            // serialize (editor UI)
            std::string json = Scripting::SerializeInstanceToJson(inst);
            std::cout << json << std::endl;
            // destroy when done
            Scripting::DestroyInstance(inst);
        }
#endif

    while (Engine::IsRunning()) 
    {
        // Begin frame profiling
        PerformanceProfiler::GetInstance().BeginFrame();
        
        //Update deltaTime at start of Frame
        //TimeManager::UpdateDeltaTime();
//#if LUA_TEST
//        auto now = std::chrono::steady_clock::now();
//        float dt = 1.0f / 60.0f;
//        dt = std::chrono::duration<float>(now - lastFrameTime).count();
//        lastFrameTime = now;
//#endif

            Engine::Update();
            GameManager::Update();
            // --- scripting + hot reload work (main-thread only) ---
#if LUA_TEST
        // 6a) Call the scripting runtime tick so Lua's `update(dt)` executes.
            Scripting::Tick(dt);

            //// 6b) Poll hot-reload manager for file-change events (non-blocking).
            //// Poll() will also invoke the change callback (SetChangeCallback) on the caller thread.
            //auto events = hrm.Poll();
            //for (const auto& ev : events) {
            //    std::cout << "[main] Poll got event: path=" << ev.path << " ts=" << ev.timestamp << "\n";
            //    // If you want to trigger reload immediately (instead of using RequestReload in the callback)
            //    // you could call Scripting::RequestReload() here as well.
            //}

            //// 6c) Simulate an edit to the script file after `simulateEditAfterSeconds` seconds.
            //// This overwrites the file on disk; the watcher will detect it and trigger a reload.
            //double elapsed = std::chrono::duration<double>(now - startTime).count();
            //if (!didWriteUpdatedScript && elapsed >= simulateEditAfterSeconds) {
            //    std::cout << "[main] Simulating script edit (writing updated script)\n";
            //    const std::string updatedScript =
            //        "counter = 0\n"
            //        "function update(dt)\n"
            //        "  -- changed behavior: increment by 10 to show reload took effect\n"
            //        "  counter = counter + 10\n"
            //        "  cpp_log(string.format('[lua] UPDATED tick: counter=%d dt=%.3f', counter, dt))\n"
            //        "end\n"
            //        "function on_reload()\n"
            //        "  cpp_log('[lua] UPDATED on_reload invoked — new script active')\n"
            //        "end\n";
            //    if (WriteFile(scriptPath, updatedScript)) {
            //        // best-effort: notify the HotReloadManager and the runtime
            //        hrm.RequestReload("file_modified_programmatically");
            //        Scripting::RequestReload();
            //        didWriteUpdatedScript = true;
            //    }
            //    else {
            //        std::cerr << "[main] Failed to write updated script\n";
            //    }
            //}

            //// 6d) Demonstrate a manual reload request after a bit more time.
            //if (!didManualReload && elapsed >= manualReloadAfterSeconds) {
            //    std::cout << "[main] Manual RequestReload()\n";
            //    Scripting::RequestReload();
            //    didManualReload = true;
            //}
#endif
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

#if LUA_TEST
    // After the main loop wind-down: inspect Lua global `counter` via GetLuaState (main-thread only)
    //lua_State* L = Scripting::GetLuaState();
    //if (L) {
    //    lua_getglobal(L, "counter");
    //    if (lua_isnumber(L, -1)) {
    //        int counter = static_cast<int>(lua_tointeger(L, -1));
    //        std::cout << "[main] Lua 'counter' final value = " << counter << "\n";
    //    }
    //    else {
    //        std::cout << "[main] Lua 'counter' not present or not a number\n";
    //    }
    //    lua_pop(L, 1);
    //}

    // Destroy the created environment
    //if (envId != 0) {
    //    Scripting::DestroyEnvironment(envId);
    //    std::cout << "Destroyed environment id = " << envId << "\n";
    //}

    // Stop watcher and shutdown scripting runtime
    //hrm.Stop(); //Got warning from vs TODO take note
    Scripting::Shutdown();
#endif

	GUIManager::Exit();
    GameManager::Shutdown();
    Engine::Shutdown();
    MetaFilesManager::CleanupUnusedMetaFiles();

    ENGINE_PRINT("=== Editor ended ===\n");
    return 0;
}