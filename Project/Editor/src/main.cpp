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

TODO REMOVE SCRIPTING LOGGING SYSTEM IF CAN REPLACE WITH ENGINE LOADING SYSTEM
*/
#pragma region LUA_TEST
#include "Scripting.h"
#include "HotReloadManager.h"
#include "ScriptFileSystem.h"

static bool WriteFile(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs << content;
    return true;
}
#define LUA_TEST 1
#pragma endregion

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

#if LUA_TEST
    const std::string scriptPath = "Resources/Scripts/test.lua";

    // 1) Write initial script to disk
    const std::string initialScript =
        "counter = 0\n"
        "coroutineExample = nil\n"
        "function update(dt)\n"
        "  counter = counter + 1\n"
        "  cpp_log(string.format(\"[lua] tick: counter=%d dt=%.3f\", counter, dt))\n"
        "  -- create a coroutine once to demonstrate coroutine-friendly behavior\n"
        "  if not coroutineExample then\n"
        "    coroutineExample = coroutine.create(function()\n"
        "      for i=1,3 do\n"
        "        cpp_log(\"[lua] coroutine step \" .. i)\n"
        "        coroutine.yield()\n"
        "      end\n"
        "      cpp_log(\"[lua] coroutine finished\")\n"
        "    end)\n"
        "  end\n"
        "  -- resume coroutine (safe if it's not finished)\n"
        "  if coroutine.status(coroutineExample) ~= 'dead' then\n"
        "    local ok, msg = coroutine.resume(coroutineExample)\n"
        "    if not ok then cpp_log('[lua] coroutine error: ' .. tostring(msg)) end\n"
        "  end\n"
        "end\n"
        "\n"
        "function on_reload()\n"
        "  cpp_log('[lua] on_reload invoked — script reloaded')\n"
        "end\n";

    if (!WriteFile(scriptPath, initialScript)) 
    {
        std::cerr << "Failed to write initial script file: " << scriptPath << "\n";
        return 1;
    }

    // 3) Initialize scripting runtime (main script path will be loaded at init)
    Scripting::ScriptingConfig cfg;
    cfg.mainScriptPath = scriptPath;
    cfg.gcIntervalMs = 200; // small GC step interval for demo

    if (!Scripting::Initialize(cfg)) {
        std::cerr << "Scripting::Initialize failed\n";
        return 2;
    }
    std::cout << "Scripting initialized.\n";

    // 4) Start hot-reload manager watching the script file.
    Scripting::HotReloadManager hrm;
    Scripting::HotReloadConfig hrCfg;
    hrCfg.paths.push_back(scriptPath);
    hrCfg.pollIntervalMs = 250;
    hrCfg.debounceMs = 150;
    if (!hrm.Start(hrCfg, nullptr)) {
        std::cerr << "HotReloadManager failed to start watcher (falling back to manual reloads)\n";
    }
    else {
        std::cout << "HotReloadManager started watching: " << scriptPath << "\n";
    }

    // Optional: Set a callback that runs on the main thread when Poll() returns events.
    hrm.SetChangeCallback([&](const Scripting::HotReloadEvent& ev) {
        std::cout << "[main] HotReloadManager change callback: " << ev.path
            << " ts=" << ev.timestamp << "\n";
        // This callback runs on main thread (because we'll call Poll on main thread below).
        // We schedule the actual reload to happen during the main loop.
        Scripting::RequestReload();
        });

    // 5) Create a per-script environment (demonstration)
    auto envId = Scripting::CreateEnvironment("demo_env");
    std::cout << "Created environment id = " << envId << "\n";

    // 6) Main loop integration: tick scripting, poll hot-reload, simulate changes
    // We'll use a simple steady_clock timer to simulate time & trigger file edits.
    auto startTime = std::chrono::steady_clock::now();
    auto lastFrameTime = startTime;
    bool didWriteUpdatedScript = false;
    bool didManualReload = false;

    // Configuration for our demo triggers (seconds after start)
    const double simulateEditAfterSeconds = 2.0;   // rewrite the lua file to simulate developer edit
    const double manualReloadAfterSeconds = 5.0;   // request a manual reload later
#endif

    while (Engine::IsRunning()) {
        // Begin frame profiling
        PerformanceProfiler::GetInstance().BeginFrame();
        
        //Update deltaTime at start of Frame
        //TimeManager::UpdateDeltaTime();
#if LUA_TEST
        auto now = std::chrono::steady_clock::now();
        float dt = 1.0f / 60.0f;
        dt = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;
#endif

            Engine::Update();
            GameManager::Update();
            // --- scripting + hot reload work (main-thread only) ---
#if LUA_TEST
        // 6a) Call the scripting runtime tick so Lua's `update(dt)` executes.
            Scripting::Tick(dt);

            // 6b) Poll hot-reload manager for file-change events (non-blocking).
            // Poll() will also invoke the change callback (SetChangeCallback) on the caller thread.
            auto events = hrm.Poll();
            for (const auto& ev : events) {
                std::cout << "[main] Poll got event: path=" << ev.path << " ts=" << ev.timestamp << "\n";
                // If you want to trigger reload immediately (instead of using RequestReload in the callback)
                // you could call Scripting::RequestReload() here as well.
            }

            // 6c) Simulate an edit to the script file after `simulateEditAfterSeconds` seconds.
            // This overwrites the file on disk; the watcher will detect it and trigger a reload.
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            if (!didWriteUpdatedScript && elapsed >= simulateEditAfterSeconds) {
                std::cout << "[main] Simulating script edit (writing updated script)\n";
                const std::string updatedScript =
                    "counter = 0\n"
                    "function update(dt)\n"
                    "  -- changed behavior: increment by 10 to show reload took effect\n"
                    "  counter = counter + 10\n"
                    "  cpp_log(string.format('[lua] UPDATED tick: counter=%d dt=%.3f', counter, dt))\n"
                    "end\n"
                    "function on_reload()\n"
                    "  cpp_log('[lua] UPDATED on_reload invoked — new script active')\n"
                    "end\n";
                if (WriteFile(scriptPath, updatedScript)) {
                    // best-effort: notify the HotReloadManager and the runtime
                    hrm.RequestReload("file_modified_programmatically");
                    Scripting::RequestReload();
                    didWriteUpdatedScript = true;
                }
                else {
                    std::cerr << "[main] Failed to write updated script\n";
                }
            }

            // 6d) Demonstrate a manual reload request after a bit more time.
            if (!didManualReload && elapsed >= manualReloadAfterSeconds) {
                std::cout << "[main] Manual RequestReload()\n";
                Scripting::RequestReload();
                didManualReload = true;
            }
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
    if (envId != 0) {
        Scripting::DestroyEnvironment(envId);
        std::cout << "Destroyed environment id = " << envId << "\n";
    }

    // Stop watcher and shutdown scripting runtime
    hrm.Stop(); //Got warning from vs TODO take note
    Scripting::Shutdown();
#endif

	GUIManager::Exit();
    GameManager::Shutdown();
    Engine::Shutdown();
    MetaFilesManager::CleanupUnusedMetaFiles();

    ENGINE_PRINT("=== Editor ended ===\n");
    return 0;
}