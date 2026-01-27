#include "pch.h"

#include "Graphics/OpenGL.h"
#include "Platform/Platform.h"

#ifdef ANDROID
#include <EGL/egl.h>
#include <android/log.h>
#include "Platform/IPlatform.h"
#endif

#include "Engine.h"
#include "Logging.hpp"

#include <WindowManager.hpp>
#include <Input/InputManager.h>
#include <Input/DesktopInputManager.h>
#ifdef ANDROID
#include <Input/AndroidInputManager.h>
#endif
#include <Asset Manager/MetaFilesManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include "Game AI/BrainSystems.hpp"
#include <Scene/SceneManager.hpp>
#include "TimeManager.hpp"
#include "Sound/AudioManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "Settings/GameSettings.hpp"

#ifdef ANDROID
#endif
#include <Asset Manager/AssetManager.hpp>
#include "Graphics/PostProcessing/PostProcessingManager.hpp"

namespace TEMP {
	std::string windowTitle = "GAM300";
}

// Static member definition
GameState Engine::currentGameState = GameState::EDIT_MODE;

const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

bool Engine::Initialize() {
	// Initialize logging system first
	if (!EngineLogging::Initialize()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to initialize logging system!\n");
		return false;
	}
	
	SetGameState(GameState::PLAY_MODE);
	WindowManager::Initialize(SCR_WIDTH, SCR_HEIGHT, TEMP::windowTitle.c_str());

    ENGINE_PRINT("Engine initializing...");

	// WOON LI TEST CODE
	//InputManager::Initialize();

	// Initialize unified input system (NEW)
	ENGINE_PRINT("[Engine] Initializing unified input system...");
	#ifdef ANDROID
		g_inputManager = new AndroidInputManager();
		ENGINE_PRINT("[Engine] Created AndroidInputManager");
	#else
		// Desktop: Pass platform pointer for hardware queries
		IPlatform* platform = WindowManager::GetPlatform();
		g_inputManager = new DesktopInputManager(platform);
		ENGINE_PRINT("[Engine] Created DesktopInputManager");
	#endif

	// Load input configuration
	// On Android, this is deferred until after AssetManager is set (called from JNI)
	#ifndef ANDROID
	std::string configPath = "Resources/Configs/input_config.json";
	if (g_inputManager && !g_inputManager->LoadConfig(configPath)) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to load input config from: ", configPath);
	} else {
		ENGINE_PRINT("[Engine] Input system initialized successfully");
	}
	#endif

	// Initialize AudioManager on desktop now that platform assets are available
	if (!AudioManager::GetInstance().Initialise()) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to initialize AudioManager\n");
	} else {
		ENGINE_PRINT("[Engine] AudioManager initialized\n");
	}

	// Initialize GameSettings (loads saved settings and applies to audio/graphics)
	// Note: This is called early but ApplySettings() for graphics is deferred
	// until PostProcessingManager is initialized (in InitializeGraphicsResources)
	GameSettingsManager::GetInstance().Initialize();
	ENGINE_PRINT("[Engine] GameSettings initialized\n");

	// Android: Asset initialization happens in JNI after AssetManager is set

	//TEST ON ANDROID FOR REFLECTION - IF NOT WORKING, INFORM IMMEDIATELY
#if 0
    {
        bool reflection_ok = true;
        bool serialization_ok = true;

        ENGINE_PRINT("=== Running reflection + serialization single-main test for Matrix4x4 ===");

        // --- Reflection-only checks ---
        ENGINE_PRINT("[1] Reflection metadata + runtime access checks");

        using T = Transform;
        TypeDescriptor* td = nullptr;
        try {
            td = TypeResolver<T>::Get();
        }
        catch (const std::exception& ex) {
            ENGINE_PRINT("ERROR: exception while calling TypeResolver::Get(): ", ex.what());
        }
        catch (...) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "ERROR: unknown exception calling TypeResolver::Get()");
        }

        if (!td) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: TypeResolver<Matrix4x4>::Get() returned null. Ensure REFL_REGISTER_START(Matrix4x4) is compiled & linked.");
            reflection_ok = false;
        }
        else {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "Type name: ", td->ToString().c_str(), ", size: ", td->GetSize());

            auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
            if (!sdesc) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: descriptor is not TypeDescriptor_Struct");
                reflection_ok = false;
            }
            else {
                // copy member list out of the descriptor (GetMembers returns a std::vector copy allocated inside the DLL)
                auto members = sdesc->GetMembers();
                ENGINE_PRINT(EngineLogging::LogLevel::Debug, "Member count: ", members.size());

                // Basic metadata checks & printing
                for (size_t i = 0; i < members.size(); ++i) {
                    const auto& m = members[i];
                    std::string mname = m.name ? m.name : "<null>";
                    std::string tname = m.type ? m.type->ToString() : "<null-type>";
                    ENGINE_PRINT("  [", i, "] name='", mname, "' type='", tname, "'\n");

                    if (!m.type) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: member has null TypeDescriptor");
                        reflection_ok = false;
                    }
                    if (tname.find('&') != std::string::npos) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: member type contains ampersand. Strip references in macro.");
                        reflection_ok = false;
                    }
                    if (!m.get_ptr) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: member.get_ptr is null");
                        reflection_ok = false;
                    }
                }

                // If metadata appears okay, test runtime assign/read on a temporary object
                if (reflection_ok) {
                    try {
                        T v{}; // object to mutate and test
                        // Get primitive descriptors that we support for testing
                        TypeDescriptor* floatDesc = nullptr;
                        TypeDescriptor* doubleDesc = nullptr;
                        TypeDescriptor* intDesc = nullptr;
                        TypeDescriptor* uintDesc = nullptr;
                        TypeDescriptor* int64Desc = nullptr;
                        TypeDescriptor* uint64Desc = nullptr;
                        TypeDescriptor* boolDesc = nullptr;
                        TypeDescriptor* stringDesc = nullptr;

                        try { floatDesc = TypeResolver<float>::Get(); }
                        catch (...) {}
                        try { doubleDesc = TypeResolver<double>::Get(); }
                        catch (...) {}
                        try { intDesc = TypeResolver<int>::Get(); }
                        catch (...) {}
                        try { uintDesc = TypeResolver<unsigned int>::Get(); }
                        catch (...) {}
                        try { int64Desc = TypeResolver<long long>::Get(); }
                        catch (...) {}
                        try { uint64Desc = TypeResolver<unsigned long long>::Get(); }
                        catch (...) {}
                        try { boolDesc = TypeResolver<bool>::Get(); }
                        catch (...) {}
                        try { stringDesc = TypeResolver<std::string>::Get(); }
                        catch (...) {}

                        struct RuntimeTest {
                            size_t idx;
                            std::string type_name;
                            std::function<void(void*)> assign;         // assign sample into a single object slot
                            std::function<bool(void*)> verify_inplace; // verify sample exists at given pointer
                        };
                        std::vector<RuntimeTest> runtime_tests;

                        // Build runtime tests based on members
                        for (size_t i = 0; i < members.size(); ++i) {
                            const auto& m = members[i];
                            if (!m.type || !m.get_ptr) continue;

                            if (m.type == floatDesc) {
                                float sample = 1.2345f + static_cast<float>(i) * 0.5f;
                                runtime_tests.push_back({
                                    i, "float",
                                    [sample](void* p) { *reinterpret_cast<float*>(p) = sample; },
                                    [sample](void* p) { return std::fabs(static_cast<double>(*reinterpret_cast<float*>(p)) - static_cast<double>(sample)) < 1e-6; }
                                    });
                            }
                            else if (m.type == doubleDesc) {
                                double sample = 1.2345 + static_cast<double>(i) * 0.5;
                                runtime_tests.push_back({
                                    i, "double",
                                    [sample](void* p) { *reinterpret_cast<double*>(p) = sample; },
                                    [sample](void* p) { return std::fabs(*reinterpret_cast<double*>(p) - sample) < 1e-9; }
                                    });
                            }
                            else if (m.type == intDesc) {
                                int sample = static_cast<int>(i) * 7 + 1;
                                runtime_tests.push_back({
                                    i, "int",
                                    [sample](void* p) { *reinterpret_cast<int*>(p) = sample; },
                                    [sample](void* p) { return *reinterpret_cast<int*>(p) == sample; }
                                    });
                            }
                            else if (m.type == uintDesc) {
                                unsigned sample = static_cast<unsigned>(i) * 11u + 3u;
                                runtime_tests.push_back({
                                    i, "unsigned",
                                    [sample](void* p) { *reinterpret_cast<unsigned*>(p) = sample; },
                                    [sample](void* p) { return *reinterpret_cast<unsigned*>(p) == sample; }
                                    });
                            }
                            else if (m.type == int64Desc) {
                                long long sample = static_cast<long long>(i) * 100000000LL + 5LL;
                                runtime_tests.push_back({
                                    i, "long long",
                                    [sample](void* p) { *reinterpret_cast<long long*>(p) = sample; },
                                    [sample](void* p) { return *reinterpret_cast<long long*>(p) == sample; }
                                    });
                            }
                            else if (m.type == uint64Desc) {
                                unsigned long long sample = static_cast<unsigned long long>(i) * 100000000ULL + 9ULL;
                                runtime_tests.push_back({
                                    i, "unsigned long long",
                                    [sample](void* p) { *reinterpret_cast<unsigned long long*>(p) = sample; },
                                    [sample](void* p) { return *reinterpret_cast<unsigned long long*>(p) == sample; }
                                    });
                            }
                            else if (m.type == boolDesc) {
                                bool sample = (i % 2) == 0;
                                runtime_tests.push_back({
                                    i, "bool",
                                    [sample](void* p) { *reinterpret_cast<bool*>(p) = sample; },
                                    [sample](void* p) { return *reinterpret_cast<bool*>(p) == sample; }
                                    });
                            }
                            else if (stringDesc && m.type == stringDesc) {
                                std::string sample = std::string("test_str_") + std::to_string(i);
                                runtime_tests.push_back({
                                    i, "std::string",
                                    [sample](void* p) { *reinterpret_cast<std::string*>(p) = sample; },
                                    [sample](void* p) { return *reinterpret_cast<std::string*>(p) == sample; }
                                    });
                            }
                        } // end build runtime tests

                        // Run runtime assign/verify tests
                        bool runtime_ok = true;
                        if (runtime_tests.empty()) {
                            ENGINE_PRINT("    -> WARN: no supported primitive members found; cannot validate runtime read/write\n");
                            runtime_ok = false;
                        }
                        else {
                            for (const auto& rt : runtime_tests) {
                                void* addr = members[rt.idx].get_ptr(&v);
                                try {
                                    rt.assign(addr);                    // write sample
                                }
                                catch (...) {
                                    ENGINE_PRINT("    -> EXCEPTION assigning member[" , rt.idx, "]\n");
                                    runtime_ok = false;
                                    continue;
                                }
                                bool ok = false;
                                try { ok = rt.verify_inplace(addr); }   // read & compare
                                catch (...) { ok = false; }
                                ENGINE_PRINT("    -> runtime member[" , rt.idx , "] (" , rt.type_name , "): " , (ok ? "OK" : "MISMATCH\n"));
                                if (!ok) runtime_ok = false;
                            }
                        }

                        ENGINE_PRINT(EngineLogging::LogLevel::Info, "Runtime read/write via get_ptr status: ", (runtime_ok ? "OK" : "MISMATCH"));
                        if (!runtime_ok) reflection_ok = false;
                    }
                    catch (const std::exception& ex) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: exception during runtime read/write: ", ex.what());
                        reflection_ok = false;
                    }
                } // end if metadata ok
            } // end else sdesc
        } // end else td

        // --- Serialization checks (uses TypeDescriptor::Serialize / SerializeJson / Deserialize) ---
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "[2] Serialization + round-trip checks");

        if (!td) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "SKIP: serialization checks because TypeDescriptor was not available");
            serialization_ok = false;
        }
        else {
            try {
                // Create sample object and populate via reflection
                T src{};
                auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
                if (!sdesc) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: not a struct descriptor; cannot serialize");
                    serialization_ok = false;
                }
                else {
                    auto members = sdesc->GetMembers();

                    // get primitive descriptors
                    TypeDescriptor* floatDesc = nullptr; TypeDescriptor* doubleDesc = nullptr;
                    TypeDescriptor* intDesc = nullptr; TypeDescriptor* uintDesc = nullptr;
                    TypeDescriptor* int64Desc = nullptr; TypeDescriptor* uint64Desc = nullptr;
                    TypeDescriptor* boolDesc = nullptr; TypeDescriptor* stringDesc = nullptr;

                    try { floatDesc = TypeResolver<float>::Get(); }
                    catch (...) {}
                    try { doubleDesc = TypeResolver<double>::Get(); }
                    catch (...) {}
                    try { intDesc = TypeResolver<int>::Get(); }
                    catch (...) {}
                    try { uintDesc = TypeResolver<unsigned int>::Get(); }
                    catch (...) {}
                    try { int64Desc = TypeResolver<long long>::Get(); }
                    catch (...) {}
                    try { uint64Desc = TypeResolver<unsigned long long>::Get(); }
                    catch (...) {}
                    try { boolDesc = TypeResolver<bool>::Get(); }
                    catch (...) {}
                    try { stringDesc = TypeResolver<std::string>::Get(); }
                    catch (...) {}

                    struct SerTest {
                        size_t idx;
                        std::string type_name;
                        std::function<void(void*)> assign; // populate src
                        std::function<bool(const void*, const void*)> compare; // compare src slot vs dst slot
                    };
                    std::vector<SerTest> ser_tests;

                    // Build serialization tests
                    for (size_t i = 0; i < members.size(); ++i) {
                        const auto& m = members[i];
                        if (!m.type || !m.get_ptr) continue;

                        if (m.type == floatDesc) {
                            float sample = 1.2345f + static_cast<float>(i) * 0.5f;
                            ser_tests.push_back({
                                i, "float",
                                [sample](void* p) { *reinterpret_cast<float*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    float va = *reinterpret_cast<const float*>(a);
                                    float vb = *reinterpret_cast<const float*>(b);
                                    return std::fabs(static_cast<double>(va - vb)) < 1e-6;
                                }
                                });
                        }
                        else if (m.type == doubleDesc) {
                            double sample = 1.2345 + static_cast<double>(i) * 0.5;
                            ser_tests.push_back({
                                i, "double",
                                [sample](void* p) { *reinterpret_cast<double*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    double va = *reinterpret_cast<const double*>(a);
                                    double vb = *reinterpret_cast<const double*>(b);
                                    return std::fabs(va - vb) < 1e-9;
                                }
                                });
                        }
                        else if (m.type == intDesc) {
                            int sample = static_cast<int>(i) * 7 + 1;
                            ser_tests.push_back({
                                i, "int",
                                [sample](void* p) { *reinterpret_cast<int*>(p) = sample; },
                                [](const void* a, const void* b) {
                                    return *reinterpret_cast<const int*>(a) == *reinterpret_cast<const int*>(b);
                                }
                                });
                        }
                        else if (m.type == uintDesc) {
                            unsigned sample = static_cast<unsigned>(i) * 11u + 3u;
                            ser_tests.push_back({
                                i, "unsigned",
                                [sample](void* p) { *reinterpret_cast<unsigned*>(p) = sample; },
                                [](const void* a, const void* b) {
                                    return *reinterpret_cast<const unsigned*>(a) == *reinterpret_cast<const unsigned*>(b);
                                }
                                });
                        }
                        else if (m.type == int64Desc) {
                            long long sample = static_cast<long long>(i) * 1000000LL + 5LL;
                            ser_tests.push_back({
                                i, "long long",
                                [sample](void* p) { *reinterpret_cast<long long*>(p) = sample; },
                                [](const void* a, const void* b) {
                                    return *reinterpret_cast<const long long*>(a) == *reinterpret_cast<const long long*>(b);
                                }
                                });
                        }
                        else if (m.type == uint64Desc) {
                            unsigned long long sample = static_cast<unsigned long long>(i) * 1000000ULL + 9ULL;
                            ser_tests.push_back({
                                i, "unsigned long long",
                                [sample](void* p) { *reinterpret_cast<unsigned long long*>(p) = sample; },
                                [](const void* a, const void* b) {
                                    return *reinterpret_cast<const unsigned long long*>(a) == *reinterpret_cast<const unsigned long long*>(b);
                                }
                                });
                        }
                        else if (m.type == boolDesc) {
                            bool sample = (i % 2) == 0;
                            ser_tests.push_back({
                                i, "bool",
                                [sample](void* p) { *reinterpret_cast<bool*>(p) = sample; },
                                [](const void* a, const void* b) {
                                    return *reinterpret_cast<const bool*>(a) == *reinterpret_cast<const bool*>(b);
                                }
                                });
                        }
                        else if (stringDesc && m.type == stringDesc) {
                            std::string sample = std::string("test_str_") + std::to_string(i);
                            ser_tests.push_back({
                                i, "std::string",
                                [sample](void* p) { *reinterpret_cast<std::string*>(p) = sample; },
                                [](const void* a, const void* b) {
                                    return *reinterpret_cast<const std::string*>(a) == *reinterpret_cast<const std::string*>(b);
                                }
                                });
                        }
                    } // end build ser_tests

                    // Populate src using tests
                    for (const auto& st : ser_tests) {
                        void* addr = members[st.idx].get_ptr(&src);
                        try { st.assign(addr); }
                        catch (...) {ENGINE_PRINT("  -> exception assigning member[", st.idx, "]\n");}
                    }

                    // Optionally set canonical floats if available
                    if (members.size() >= 3) {
                        try {
                            *reinterpret_cast<float*>(members[0].get_ptr(&src)) = 10.0f;
                            *reinterpret_cast<float*>(members[1].get_ptr(&src)) = -3.5f;
                            *reinterpret_cast<float*>(members[2].get_ptr(&src)) = 0.25f;
                        }
                        catch (...) {}
                    }
                    else {
                        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "WARN: not enough members to populate canonical values");
                    }

                    // 1) Text Serialize
                    std::stringstream ss;
                    td->Serialize(&src, ss);
                    std::string text_out = ss.str();
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "Text Serialize output: ", text_out.c_str());

                    // 2) rapidjson SerializeJson -> string
                    rapidjson::Document dout;
                    td->SerializeJson(&src, dout);
                    rapidjson::StringBuffer sb;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
                    dout.Accept(writer);
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "rapidjson Serialize output: ", sb.GetString());

                    // 3) Round-trip deserialize into dst
                    T dst{};
                    rapidjson::Document din;
                    din.Parse(sb.GetString());
                    td->Deserialize(&dst, din);

                    // Compare tested members src vs dst
                    size_t matched = 0;
                    if (ser_tests.empty()) {
                        ENGINE_PRINT("WARN: no supported members were tested; ensure primitive descriptors are registered.\n");
                    }
                    else {
                        for (const auto& st : ser_tests) {
                            const void* a = members[st.idx].get_ptr(&src);
                            const void* b = members[st.idx].get_ptr(&dst);
                            bool ok = false;
                            try { ok = st.compare(a, b); }
                            catch (...) { ok = false; }
                            ENGINE_PRINT("  member[", st.idx, "] (", st.type_name, "): ", (ok ? "MATCH" : "MISMATCH\n"));
                            if (ok) ++matched;
                        }
                    }

                    bool match_all = (!ser_tests.empty() && matched == ser_tests.size());
                    ENGINE_PRINT("Round-trip equality: ", (match_all ? "OK" : "MISMATCH"));
                    if (!match_all) serialization_ok = false;
                }
            }
            catch (...) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: unknown error during serialization tests");
                serialization_ok = false;
            }
        }

        // --- Registry introspection (optional) ---
        ENGINE_PRINT("[3] Registry contents (keys):");
        for (const auto& kv : TypeDescriptor::type_descriptor_lookup()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "  ", kv.first.c_str());
        }

        // --- Summary & exit code ---
        ENGINE_PRINT("=== SUMMARY ===");
        ENGINE_PRINT(reflection_ok ? EngineLogging::LogLevel::Info : EngineLogging::LogLevel::Error, "Reflection: ", (reflection_ok ? "PASS" : "FAIL"));
        ENGINE_PRINT(serialization_ok ? EngineLogging::LogLevel::Info : EngineLogging::LogLevel::Error, "Serialization: ", (serialization_ok ? "PASS" : "FAIL"));

        if (!reflection_ok) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "NOTE: if you hit a linker error mentioning GetPrimitiveDescriptor<float&>() or you see member types printed with ampersand, apply the macro fix to strip references when resolving member types in the macro: TypeResolver<std::remove_reference_t<decltype(std::declval<T>().VARIABLE)>>::Get()");
        }
    }
#endif
#if LUA_TEST
    Scripting::Init();

    
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
    //float dt = 1.0f / 60.0f;

    // 5) create a script instance from file (path on disk)
    //int inst = Scripting::CreateInstanceFromFile("Resources/Scripts/sample_mono_behavior_NEW.lua"); //Awake is called
    //if (Scripting::IsValidInstance(inst)) {
    //    Scripting::CallInstanceFunction(inst, "Start");
    //    // serialize (editor UI)
    //    std::string json = Scripting::SerializeInstanceToJson(inst);
    //    std::cout << json << std::endl;
    //}

#endif

	// Note: Scene loading and lighting setup moved to InitializeGraphicsResources()
	// This will be called after the graphics context is ready

	//lightManager.printLightStats();

	// Test Audio
	/*{
		if (!AudioManager::GetInstance().Initialise())
		{
			ENGINE_LOG_ERROR("Failed to initialize AudioManager");
		}
		else
		{
			AudioHandle h = AudioManager::GetInstance().LoadAudio("Resources/Audio/sfx/Test_duck.wav");
			if (h != 0) {
				AudioManager::GetInstance().Play(h, false, 0.5f);
			}
		}
	}*/

	ENGINE_LOG_INFO("Engine initialization completed successfully");
	
	// Add some test logging messages
	ENGINE_LOG_DEBUG("This is a test debug message");
	ENGINE_LOG_WARN("This is a test warning message");
	ENGINE_LOG_ERROR("This is a test error message");
	    
	return true;
}

bool Engine::InitializeGraphicsResources() {
#ifdef EDITOR
    MetaFilesManager::InitializeAssetMetaFiles("../../Resources"); // Root project resources folder for editor
#else
    MetaFilesManager::InitializeAssetMetaFiles("Resources"); // Root project resources folder for Android/Desktop
#endif
	ENGINE_LOG_INFO("Initializing graphics resources...");

#ifdef ANDROID
    if (auto* platform = WindowManager::GetPlatform()) {
        platform->MakeContextCurrent();
        // Check if OpenGL context is current
        EGLDisplay display = eglGetCurrentDisplay();
        EGLContext context = eglGetCurrentContext();
        EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);

        // __android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL State - Display: %p, Context: %p, Surface: %p",
        //                    display, context, surface);

        if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE) {
            __android_log_print(ANDROID_LOG_ERROR, "GAM300", "EGL CONTEXT NOT CURRENT!");
            return false;
        }
    }
#endif

	// Load last opened scene (editor) or default scene (game)
#ifdef EDITOR
	std::string lastScenePath = SceneManager::LoadLastOpenedScenePath();
	if (lastScenePath.empty()) {
		// No last scene, load default
		lastScenePath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Scenes/Joe_MainMenuTest.scene";
		ENGINE_LOG_INFO("No previous scene found, loading default scene");
	}
	else {
		ENGINE_LOG_INFO("Loading last opened scene: " + lastScenePath);
	}
	SceneManager::GetInstance().LoadScene(lastScenePath);
#else
	// Game build loads default scene
#ifdef ANDROID
	// Android: Load main menu
	SceneManager::GetInstance().LoadScene(AssetManager::GetInstance().GetRootAssetDirectory() + "/Scenes/01_MainMenu.scene");
	ENGINE_LOG_INFO("Loaded main menu scene (Android)");
#else
	// Desktop: Load main menu
	SceneManager::GetInstance().LoadScene(AssetManager::GetInstance().GetRootAssetDirectory() + "/Scenes/Joe_MainMenuTest.scene");
	ENGINE_LOG_INFO("Loaded main menu scene");
#endif
#endif

#ifdef ANDROID
    // Virtual controls are now handled by AndroidInputManager (no separate initialization needed)
    ENGINE_LOG_INFO("Android input system initialized (virtual controls integrated)");
#endif

	// Re-apply GameSettings now that PostProcessingManager is initialized
	// This ensures saved gamma/exposure values are applied to the HDR effect
	GameSettingsManager::GetInstance().ApplySettings();

	ENGINE_LOG_INFO("Graphics resources initialized successfully");
	return true;
}

void Engine::LoadInputConfig() {
    // Called from JNI on Android after AssetManager is set
    std::string configPath = "Resources/Configs/input_config.json";
    if (g_inputManager && !g_inputManager->LoadConfig(configPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to load input config from: ", configPath);
    } else {
        ENGINE_PRINT("[Engine] Input system initialized successfully");
    }
}

bool Engine::InitializeAssets() {
    // Initialize asset meta files - called after platform is ready (e.g., Android AssetManager set)
    // MetaFilesManager::InitializeAssetMetaFiles("Resources");  // Uncomment if needed
//#ifdef ANDROID
//    if (auto* platform = WindowManager::GetPlatform()) {
//        platform->MakeContextCurrent();
//        ENGINE_LOG_INFO("Android->MakeContextCurrent success");
//    }
//#endif
    return true;
}

void Engine::Update() {
    TimeManager::UpdateDeltaTime();

    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    RunBrainInitSystem(ecs);

    RunBrainUpdateSystem(ecs, static_cast<float>(TimeManager::GetDeltaTime()));
    
	// Only update the scene if the game should be running (not paused)
	if (ShouldRunGameLogic()) 
    {
        SceneManager::GetInstance().UpdateScene(TimeManager::GetDeltaTime()); // REPLACE WITH DT LATER
	}
}

void Engine::StartDraw() {
    
#ifdef ANDROID
    // Ensure context is current before rendering
    WindowManager::GetPlatform()->MakeContextCurrent();

    // Check if OpenGL context is current
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);

    // __android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL State - Display: %p, Context: %p, Surface: %p",
    //                    display, context, surface);

    if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "EGL CONTEXT NOT CURRENT!");
        return;
    }
#endif

    //glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // Bright red - should be very obvious

#ifdef ANDROID
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error after glClearColor: %d", error);
    }
#endif

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Actually clear the screen!

#ifdef ANDROID
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error after glClear: %d", error);
    } else {
        // __android_log_print(ANDROID_LOG_INFO, "GAM300", "Engine::StartDraw() - Successfully cleared screen with RED");
    }
#endif
}

void Engine::Draw() {
    
#ifdef ANDROID
    // Ensure the EGL context is current
    if (!WindowManager::GetPlatform()->MakeContextCurrent()) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to make EGL context current in Draw()");
        return;
    }

    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);

    if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "EGL CONTEXT NOT CURRENT - skipping draw!");
        return;
    }

    // Additional check: verify the surface is still valid
    EGLint surfaceWidth, surfaceHeight;
    if (!eglQuerySurface(display, surface, EGL_WIDTH, &surfaceWidth) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &surfaceHeight)) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "EGL surface is invalid - skipping draw!");
        return;
    }

    try {
        SceneManager::GetInstance().DrawScene();

        // Render unified input system overlay (joysticks, virtual buttons, etc.)
        if (g_inputManager) {
            g_inputManager->RenderOverlay(surfaceWidth, surfaceHeight);
        }

    } catch (const std::exception& e) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ENGINE] SceneManager::DrawScene() threw exception: %s", e.what());
    } catch (...) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ENGINE] SceneManager::DrawScene() threw unknown exception");
    }
    
#else
    SceneManager::GetInstance().DrawScene();
#endif
}

void Engine::EndDraw() {
	WindowManager::SwapBuffers();

	// Only process input if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
		// Update unified input system (NEW - platform-agnostic)
		if (g_inputManager) {
			g_inputManager->Update(static_cast<float>(TimeManager::GetDeltaTime()));
		}

		//InputManager::Update(); // Legacy input system (still needed for editor)
	}

	WindowManager::PollEvents(); // Always poll events for UI and window management

	// Update cursor state at end of frame (enforces lock state, handles ImGui interference)
	WindowManager::UpdateCursorState();
}

void Engine::Shutdown() {

	ENGINE_LOG_INFO("Engine shutdown started");

	// Shutdown GameSettings first (saves any dirty settings)
	GameSettingsManager::GetInstance().Shutdown();

	RunBrainExitSystem(ECSRegistry::GetInstance().GetActiveECSManager());
	AudioManager::GetInstance().Shutdown();

	// Cleanup unified input system
	if (g_inputManager) {
		delete g_inputManager;
		g_inputManager = nullptr;
		ENGINE_LOG_INFO("Unified input system cleaned up");
	}

    SceneManager::GetInstance().ExitScene();
    PostProcessingManager::GetInstance().Shutdown();
    GraphicsManager::GetInstance().Shutdown();
    ENGINE_PRINT("[Engine] Shutdown complete\n");
}

bool Engine::IsRunning() {
	return !WindowManager::ShouldClose();
    //
    //
}

// Game state management functions
void Engine::SetGameState(GameState state) {
	GameState previousState = currentGameState;
	currentGameState = state;

	// When leaving play mode, force unlock cursor
	if (previousState == GameState::PLAY_MODE && state != GameState::PLAY_MODE) {
		WindowManager::ForceUnlockCursor();
	}
}

GameState Engine::GetGameState() {
	return currentGameState;
}

bool Engine::ShouldRunGameLogic() {
	return currentGameState == GameState::PLAY_MODE;
}

bool Engine::IsEditMode() {
	return currentGameState == GameState::EDIT_MODE;
}

bool Engine::IsPlayMode() {
	return currentGameState == GameState::PLAY_MODE;
}

bool Engine::IsPaused() {
	return currentGameState == GameState::PAUSED_MODE;
}