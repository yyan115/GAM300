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
#include "HotReloadManager.h"
#include "ScriptFileSystem.h"
#include "ScriptSerializer.h"
#include "ScriptInspector.h"
#include "StatePreserver.h"
//UTILS NEED TO HANDLE THIS ALL OF THIS INSTANCE CREATION IS TO BE DONE IN SCRIPTUTILS NOT HERE
#include <lua.h>
#include <lauxlib.h>

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
    {
        const std::string testScriptPath = "Resources/Scripts/sample_mono_behaviour.lua";

        // Write the Lua script to disk (overwrites existing)
        const std::string luaText =
            "position = { x = 1.0, y = 2.0, z = 3.0 }\n"
            "health = 42\n"
            "name = \"PlayerOne\"\n"
            "enabled = true\n"
            "inventory = { \"sword\", \"potion\" }\n"
            "asset = { type = \"AssetRef\", data = { id = \"asset_hero_mesh\" } }\n"
            "__editor = {\n"
            "  health = { displayName = \"Health\", tooltip = \"Hit points (0..100)\", editorHint = \"slider:0,100\" },\n"
            "  position = { displayName = \"Position\", tooltip = \"World position\" },\n"
            "  name = { displayName = \"Character Name\" },\n"
            "  inventory = { displayName = \"Inventory\" },\n"
            "  asset = { displayName = \"Mesh Asset\", tooltip = \"Linked mesh asset (ID)\", editorHint = \"asset\" }\n"
            "}\n"
            "_co = nil\n"
            "local function log(fmt, ...) if cpp_log then cpp_log(string.format(fmt, ...)) else print(string.format(fmt, ...)) end end\n"
            "function Awake() log('[lua] Awake: name=%s health=%d', name, health) end\n"
            "function Start() log('[lua] Start: pos=(%.2f,%.2f,%.2f)', position.x, position.y, position.z)\n"
            "  if not _co then _co = coroutine.create(function() for i=1,3 do log('[lua] coroutine step %d', i); coroutine.yield() end log('[lua] coroutine finished') end) end\n"
            "end\n"
            "function Update(dt)\n"
            "  if enabled then position.x = position.x + (dt * 0.1) end\n"
            "  if _co and coroutine.status(_co) ~= 'dead' then local ok,err = coroutine.resume(_co); if not ok then log('[lua] coroutine error: %s', tostring(err)) end end\n"
            "  log('[lua] Update: dt=%.3f pos.x=%.3f health=%d', dt, position.x, health)\n"
            "end\n"
            "function OnDisable() log('[lua] OnDisable called') end\n"
            "function on_reload() log('[lua] on_reload invoked — script reloaded; current name=%s', name) end\n";

        if (!WriteFile(testScriptPath, luaText)) {
            std::cerr << "[test] Failed to write test script file: " << testScriptPath << "\n";
        }
        else {
            std::cout << "[test] Wrote test script: " << testScriptPath << "\n";
        }

        // Acquire Lua state
        lua_State* L = Scripting::GetLuaState();
        if (!L) {
            std::cerr << "[test] No Lua state available\n";
        }
        else {
            // --- Helper: create an instance from a script file (same logic used by ScriptComponent::AttachScript)
            auto CreateInstanceFromFile = [&](const std::string& path) -> int {
                int tmpRef = LUA_NOREF;
                int loadStatus = luaL_loadfile(L, path.c_str());
                if (loadStatus != LUA_OK) {
                    const char* msg = lua_tostring(L, -1);
                    std::cerr << "[test] luaL_loadfile error: " << (msg ? msg : "(no msg)") << "\n";
                    lua_pop(L, 1);
                    return LUA_NOREF;
                }
                // pcall with simple message handler
                lua_pushcfunction(L, [](lua_State* L)->int {
                    const char* m = lua_tostring(L, 1);
                    if (m) luaL_traceback(L, L, m, 1);
                    else lua_pushliteral(L, "(error object not string)");
                    return 1;
                    });
                int msgh = lua_gettop(L);
                int p = lua_pcall(L, 0, 1, msgh);
                if (p != LUA_OK) {
                    const char* msg = lua_tostring(L, -1);
                    std::cerr << "[test] Runtime error loading script: " << (msg ? msg : "(no msg)") << "\n";
                    lua_pop(L, 1);
                    lua_remove(L, msgh);
                    return LUA_NOREF;
                }
                // success: returned value in stack top (the module/instance)
                lua_remove(L, msgh); // remove message handler
                if (!lua_istable(L, -1)) {
                    // wrap into table { _returned = <value> }
                    lua_newtable(L);
                    lua_pushliteral(L, "_returned");
                    lua_pushvalue(L, -2); // original value
                    lua_settable(L, -3);
                    lua_remove(L, -2);
                }
                tmpRef = luaL_ref(L, LUA_REGISTRYINDEX); // pops table
                return tmpRef;
                };

            // Create instance A from the written script
            int instanceA = CreateInstanceFromFile(testScriptPath);
            if (instanceA == LUA_NOREF) {
                std::cerr << "[test] Failed to create instanceA\n";
            }
            else {
                std::cout << "[test] Created instanceA ref=" << instanceA << "\n";
            }

            // Create helpers
            Scripting::ScriptSerializer serializer;
            Scripting::ScriptInspector inspector;
            Scripting::StatePreserver preserver;

            // -- Test 1: Serialize instance to JSON and print
            if (instanceA != LUA_NOREF) {
                std::string jsonA = serializer.SerializeInstanceToJson(L, instanceA);
                std::cout << "[test] Serialize(instanceA) =>\n" << jsonA << "\n";
            }

            // -- Test 2: Inspect fields via ScriptInspector
            if (instanceA != LUA_NOREF) {
                auto fields = inspector.InspectInstance(L, instanceA, testScriptPath, /*ttl*/0.5);
                std::cout << "[test] InspectInstance fields:\n";
                for (const auto& f : fields) {
                    std::cout << "  - " << f.name << " type=" << static_cast<int>(f.type)
                        << " display=\"" << f.meta.displayName << "\" default=" << f.defaultValueSerialized << "\n";
                }
                // pick a field and change it via inspector (e.g., health -> 73)
                for (const auto& f : fields) {
                    if (f.name == "health") {
                        bool ok = inspector.SetFieldFromString(L, instanceA, f, "73");
                        std::cout << "[test] SetFieldFromString(health=73) => " << (ok ? "ok" : "fail") << "\n";
                        break;
                    }
                    if (f.name == "name") {
                        // also test changing a string
                        bool ok2 = inspector.SetFieldFromString(L, instanceA, f, "Hero007");
                        std::cout << "[test] SetFieldFromString(name=Hero007) => " << (ok2 ? "ok" : "fail") << "\n";
                    }
                }
                // reserialize and show change
                std::string jsonA_after = serializer.SerializeInstanceToJson(L, instanceA);
                std::cout << "[test] After edits Serialize(instanceA) =>\n" << jsonA_after << "\n";
            }

            // -- Test 3: StatePreserver - register keys, extract JSON, then re-inject into a new instance (simulate reload)
            //if (instanceA != LUA_NOREF) {
            //    // register keys to preserve for this instance
            //    preserver.RegisterInstanceKeys(instanceA, std::vector<std::string>{"position", "health", "name", "asset"});
            //    std::string extracted = preserver.ExtractState(L, instanceA);
            //    std::cout << "[test] ExtractState JSON =>\n" << extracted << "\n";

            //    // Create a new instance (simulate script reload / new instance B)
            //    int instanceB = CreateInstanceFromFile(testScriptPath);
            //    if (instanceB == LUA_NOREF) {
            //        std::cerr << "[test] Failed to create instanceB\n";
            //    }
            //    else {
            //        std::cout << "[test] Created instanceB ref=" << instanceB << "\n";
            //    }

            //    // Example userdata reconciler:
            //    // maps a serialized AssetRef (temp table) -> a new runtime "resolved asset" table (simulated userdata)
            //    Scripting::StatePreserver::UserdataReconcileFn reconciler = [](lua_State* L, int targetInstanceRef, const std::string& key, int tempIndex) -> bool {
            //        // tempIndex is absolute index where the parsed temp value resides (in ReinjectState it is at top)
            //        if (!lua_istable(L, tempIndex)) return false;
            //        lua_getfield(L, tempIndex, "type");
            //        bool handled = false;
            //        if (lua_isstring(L, -1)) {
            //            const char* typed = lua_tostring(L, -1);
            //            if (typed && strcmp(typed, "AssetRef") == 0) {
            //                // Extract id from data.id if present (optional)
            //                lua_pop(L, 1); // pop type
            //                lua_getfield(L, tempIndex, "data"); // push data
            //                const char* id = nullptr;
            //                if (lua_istable(L, -1)) {
            //                    lua_getfield(L, -1, "id");
            //                    if (lua_isstring(L, -1)) id = lua_tostring(L, -1);
            //                    lua_pop(L, 1); // pop id
            //                }
            //                // pop data table
            //                lua_pop(L, 1);

            //                // Create a new "resolved asset" table to simulate a new userdata instance
            //                lua_newtable(L);
            //                lua_pushstring(L, "resolved_asset_id");
            //                if (id) lua_pushstring(L, id); else lua_pushstring(L, "unknown");
            //                lua_settable(L, -3);
            //                lua_pushstring(L, "is_resolved");
            //                lua_pushboolean(L, 1);
            //                lua_settable(L, -3);

            //                // Set into target instance: get target table, set field = new table
            //                lua_rawgeti(L, LUA_REGISTRYINDEX, targetInstanceRef); // push target
            //                int absTarget = lua_absindex(L, lua_gettop(L));
            //                lua_pushvalue(L, -2); // push new asset table
            //                lua_setfield(L, absTarget, key.c_str()); // target[key] = newAsset
            //                // pop target and new asset
            //                lua_pop(L, 1); // pop target (new asset popped by setfield)
            //                // note: new asset still present? we used value copy/pop semantics so it's fine.

            //                handled = true;
            //            }
            //            else {
            //                lua_pop(L, 1); // pop type
            //            }
            //        }
            //        else {
            //            lua_pop(L, 1); // pop type if present
            //        }
            //        return handled;
            //        };

            //    // Reinject into instanceB using reconciler
            //    bool reinjected = preserver.ReinjectState(L, instanceB, extracted, reconciler);
            //    std::cout << "[test] ReinjectState into instanceB => " << (reinjected ? "ok" : "fail") << "\n";

            //    // serialize instanceB after reinjection
            //    std::string jsonB_after = serializer.SerializeInstanceToJson(L, instanceB);
            //    std::cout << "[test] Json(instanceB) after reinjection =>\n" << jsonB_after << "\n";

            //    // cleanup instanceB
            //    luaL_unref(L, LUA_REGISTRYINDEX, instanceB);
            //}

            // -- Test 4: Execute Update(dt) manually a few times to see coroutine and update messages
            //if (instanceA != LUA_NOREF) {
            //    // push update function and call it 3 times
            //    for (int i = 0; i < 3; ++i) {
            //        // retrieve update function from instance table
            //        lua_rawgeti(L, LUA_REGISTRYINDEX, instanceA); // push table
            //        lua_getfield(L, -1, "Update");               // push function or nil
            //        if (lua_isfunction(L, -1)) {
            //            lua_pushvalue(L, -2);                   // push self
            //            lua_pushnumber(L, 1.0f / 60.0f);          // dt
            //            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            //                const char* e = lua_tostring(L, -1);
            //                std::cerr << "[test] Update() error: " << (e ? e : "(no msg)") << "\n";
            //                lua_pop(L, 1);
            //            }
            //        }
            //        else {
            //            lua_pop(L, 1); // pop non-function
            //        }
            //        lua_pop(L, 1); // pop instance table
            //    }
            //}

            // cleanup instanceA
            if (instanceA != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, instanceA);

        } // end if L
    } // end test block
#endif

    while (Engine::IsRunning()) {
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
//#if LUA_TEST
//        // 6a) Call the scripting runtime tick so Lua's `update(dt)` executes.
//            Scripting::Tick(dt);
//
//            // 6b) Poll hot-reload manager for file-change events (non-blocking).
//            // Poll() will also invoke the change callback (SetChangeCallback) on the caller thread.
//            auto events = hrm.Poll();
//            for (const auto& ev : events) {
//                std::cout << "[main] Poll got event: path=" << ev.path << " ts=" << ev.timestamp << "\n";
//                // If you want to trigger reload immediately (instead of using RequestReload in the callback)
//                // you could call Scripting::RequestReload() here as well.
//            }
//
//            // 6c) Simulate an edit to the script file after `simulateEditAfterSeconds` seconds.
//            // This overwrites the file on disk; the watcher will detect it and trigger a reload.
//            double elapsed = std::chrono::duration<double>(now - startTime).count();
//            if (!didWriteUpdatedScript && elapsed >= simulateEditAfterSeconds) {
//                std::cout << "[main] Simulating script edit (writing updated script)\n";
//                const std::string updatedScript =
//                    "counter = 0\n"
//                    "function update(dt)\n"
//                    "  -- changed behavior: increment by 10 to show reload took effect\n"
//                    "  counter = counter + 10\n"
//                    "  cpp_log(string.format('[lua] UPDATED tick: counter=%d dt=%.3f', counter, dt))\n"
//                    "end\n"
//                    "function on_reload()\n"
//                    "  cpp_log('[lua] UPDATED on_reload invoked — new script active')\n"
//                    "end\n";
//                if (WriteFile(scriptPath, updatedScript)) {
//                    // best-effort: notify the HotReloadManager and the runtime
//                    hrm.RequestReload("file_modified_programmatically");
//                    Scripting::RequestReload();
//                    didWriteUpdatedScript = true;
//                }
//                else {
//                    std::cerr << "[main] Failed to write updated script\n";
//                }
//            }
//
//            // 6d) Demonstrate a manual reload request after a bit more time.
//            if (!didManualReload && elapsed >= manualReloadAfterSeconds) {
//                std::cout << "[main] Manual RequestReload()\n";
//                Scripting::RequestReload();
//                didManualReload = true;
//            }
//#endif
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