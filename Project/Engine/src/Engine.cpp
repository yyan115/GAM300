#include "pch.h"

#include "Graphics/OpenGL.h"
#include "Platform/Platform.h"
#include "Graphics/LightManager.hpp"

#ifdef ANDROID
#include <EGL/egl.h>
#include <android/log.h>
#include "Platform/IPlatform.h"
#endif

#include "Engine.h"
#include "Logging.hpp"

#include <WindowManager.hpp>
#include <Input/InputManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include "TimeManager.hpp"
#include <Sound/AudioSystem.hpp>

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
		std::cerr << "[Engine] Failed to initialize logging system!" << std::endl;
		return false;
	}
	SetGameState(GameState::PLAY_MODE);
	WindowManager::Initialize(SCR_WIDTH, SCR_HEIGHT, TEMP::windowTitle.c_str());

    ENGINE_LOG_INFO("Engine initializing...");

	// WOON LI TEST CODE
	InputManager::Initialize();

	// Platform-specific asset initialization
#ifndef __ANDROID__
	// Desktop platforms: Initialize assets immediately (filesystem-based)
	MetaFilesManager::InitializeAssetMetaFiles("Resources");
#endif
	// Android: Asset initialization happens in JNI after AssetManager is set

	//TEST ON ANDROID FOR REFLECTION - IF NOT WORKING, INFORM IMMEDIATELY
#if 1
	{
        bool reflection_ok = true;
        bool serialization_ok = true;

        std::cout << "=== Running reflection + serialization single-main test for Matrix4x4 ===\n";

        // --- Reflection-only checks ---
        std::cout << "\n[1] Reflection metadata + runtime access checks\n";
        using T = Matrix4x4;
        TypeDescriptor* td = nullptr;
        try {
            td = TypeResolver<T>::Get();
        }
        catch (const std::exception& ex) {
            std::cout << "ERROR: exception while calling TypeResolver::Get(): " << ex.what() << "\n";
        }
        catch (...) {
            std::cout << "ERROR: unknown exception calling TypeResolver::Get()\n";
        }

        if (!td) {
            std::cout << "FAIL: TypeResolver<Matrix4x4>::Get() returned null. Ensure REFL_REGISTER_START(Matrix4x4) is compiled & linked.\n";
            reflection_ok = false;
        }
        else {
            std::cout << "Type name: " << td->ToString() << ", size: " << td->size << "\n";
            auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
            if (!sdesc) {
                std::cout << "FAIL: descriptor is not TypeDescriptor_Struct\n";
                reflection_ok = false;
            }
            else {
                std::cout << "Member count: " << sdesc->members.size() << "\n";
                // Print members and basic checks
                for (size_t i = 0; i < sdesc->members.size(); ++i) {
                    const auto& m = sdesc->members[i];
                    std::string mname = m.name ? m.name : "<null>";
                    std::string tname = m.type ? m.type->ToString() : "<null-type>";
                    std::cout << "  [" << i << "] name='" << mname << "' type='" << tname << "'\n";
                    if (!m.type) {
                        std::cout << "    -> FAIL: member has null TypeDescriptor\n";
                        reflection_ok = false;
                    }
                    if (tname.find('&') != std::string::npos) {
                        std::cout << "    -> FAIL: member type contains '&' (strip references in macro). See REFL_REGISTER_PROPERTY fix.\n";
                        reflection_ok = false;
                    }
                    if (!m.get_ptr) {
                        std::cout << "    -> FAIL: member.get_ptr is null\n";
                        reflection_ok = false;
                    }
                }

                // Runtime read/write via get_ptr to prove reflection can access object memory
                try {
                    T v{};
                    auto* floatDesc = TypeResolver<float>::Get();
                    auto* doubleDesc = TypeResolver<double>::Get();
                    auto* intDesc = TypeResolver<int>::Get();
                    auto* uintDesc = TypeResolver<unsigned int>::Get();
                    auto* int64Desc = TypeResolver<long long>::Get();
                    auto* uint64Desc = TypeResolver<unsigned long long>::Get();
                    auto* boolDesc = TypeResolver<bool>::Get();

                    TypeDescriptor* stringDesc = nullptr;
                    try { stringDesc = TypeResolver<std::string>::Get(); }
                    catch (...) { /* maybe not registered */ }

                    struct TestEntry {
                        size_t idx;
                        const char* name;
                        std::function<void(void*)> assign;
                        std::function<bool(void*)> compare;
                    };
                    std::vector<TestEntry> tests;

                    // Build test list by inspecting members
                    for (size_t i = 0; i < sdesc->members.size(); ++i) {
                        const auto& m = sdesc->members[i];
                        if (!m.type || !m.get_ptr) {
                            std::cout << "    -> skipping member[" << i << "] (null type or no get_ptr)\n";
                            continue;
                        }

                        void* addr = m.get_ptr(&v);

                        if (m.type == floatDesc) {
                            float sample = 1.2345f + static_cast<float>(i) * 0.5f;
                            tests.push_back({
                                i, "float",
                                [sample](void* p) { *reinterpret_cast<float*>(p) = sample; },
                                [sample](void* p) { return std::fabs(static_cast<double>(*reinterpret_cast<float*>(p)) - static_cast<double>(sample)) < 1e-6; }
                                });
                        }
                        else if (m.type == doubleDesc) {
                            double sample = 1.2345 + static_cast<double>(i) * 0.5;
                            tests.push_back({
                                i, "double",
                                [sample](void* p) { *reinterpret_cast<double*>(p) = sample; },
                                [sample](void* p) { return std::fabs(*reinterpret_cast<double*>(p) - sample) < 1e-9; }
                                });
                        }
                        else if (m.type == intDesc) {
                            int sample = static_cast<int>(i) * 7 + 1;
                            tests.push_back({
                                i, "int",
                                [sample](void* p) { *reinterpret_cast<int*>(p) = sample; },
                                [sample](void* p) { return *reinterpret_cast<int*>(p) == sample; }
                                });
                        }
                        else if (m.type == uintDesc) {
                            unsigned sample = static_cast<unsigned>(i) * 11u + 3u;
                            tests.push_back({
                                i, "unsigned",
                                [sample](void* p) { *reinterpret_cast<unsigned*>(p) = sample; },
                                [sample](void* p) { return *reinterpret_cast<unsigned*>(p) == sample; }
                                });
                        }
                        else if (m.type == int64Desc) {
                            long long sample = static_cast<long long>(i) * 100000000LL + 5LL;
                            tests.push_back({
                                i, "long long",
                                [sample](void* p) { *reinterpret_cast<long long*>(p) = sample; },
                                [sample](void* p) { return *reinterpret_cast<long long*>(p) == sample; }
                                });
                        }
                        else if (m.type == uint64Desc) {
                            unsigned long long sample = static_cast<unsigned long long>(i) * 100000000ULL + 9ULL;
                            tests.push_back({
                                i, "unsigned long long",
                                [sample](void* p) { *reinterpret_cast<unsigned long long*>(p) = sample; },
                                [sample](void* p) { return *reinterpret_cast<unsigned long long*>(p) == sample; }
                                });
                        }
                        else if (m.type == boolDesc) {
                            bool sample = (i % 2) == 0;
                            tests.push_back({
                                i, "bool",
                                [sample](void* p) { *reinterpret_cast<bool*>(p) = sample; },
                                [sample](void* p) { return *reinterpret_cast<bool*>(p) == sample; }
                                });
                        }
                        else if (stringDesc && m.type == stringDesc) {
                            std::string sample = std::string("test_str_") + std::to_string(i);
                            tests.push_back({
                                i, "std::string",
                                [sample](void* p) { *reinterpret_cast<std::string*>(p) = sample; },
                                [sample](void* p) { return *reinterpret_cast<std::string*>(p) == sample; }
                                });
                        }
                        else {
                            std::cout << "    -> skipping member[" << i << "] type='" << m.type->ToString() << "' (unsupported for assignment/comparison)\n";
                        }
                    } // end for members

                    // Assign values
                    for (const auto& te : tests) {
                        void* addr = sdesc->members[te.idx].get_ptr(&v);
                        te.assign(addr);
                    }

                    // Verify assignments in-place (runtime read/write)
                    bool values_ok = true;
                    if (tests.empty()) {
                        std::cout << "    -> WARN: no supported primitive members found; cannot validate values\n";
                        values_ok = false;
                    }
                    else {
                        for (const auto& te : tests) {
                            void* addr = sdesc->members[te.idx].get_ptr(&v);
                            bool ok = false;
                            try { ok = te.compare(addr); }
                            catch (...) { ok = false; }
                            if (!ok) {
                                values_ok = false;
                                std::cout << "    -> MISMATCH: member[" << te.idx << "] type=" << te.name << "\n";
                            }
                        }
                        ENGINE_PRINT( EngineLogging::LogLevel::Warn ,"    -> WARN: fewer than 3 members; cannot fully validate values\n");
                        values_ok = false;
                    }

                    std::cout << "  Runtime read/write via get_ptr: " << (values_ok ? "OK" : "MISMATCH") << "\n";
                    if (!values_ok) reflection_ok = false;
                }
                catch (const std::exception& ex) {
                    std::cout << "    -> FAIL: exception during runtime read/write: " << ex.what() << "\n";
                    reflection_ok = false;
                }
            }
        }

        // --- Serialization checks (uses TypeDescriptor::Serialize / SerializeJson / Deserialize) ---
        std::cout << "\n[2] Serialization + round-trip checks\n";
        if (!td) {
            std::cout << "SKIP: serialization checks because TypeDescriptor was not available\n";
            serialization_ok = false;
        }
        else {
            try {
                // Create sample object and populate via reflection
                T src{};
                auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
                if (!sdesc) {
                    std::cout << "FAIL: not a struct descriptor; cannot serialize\n";
                    serialization_ok = false;
                }
                else {
                    // prepare known descriptors (some may throw if not registered; catch that)
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

                    struct TestEntry {
                        size_t idx;
                        std::string type_name;
                        std::function<void(void*)> assign;
                        std::function<bool(const void*, const void*)> compare;
                    };
                    std::vector<TestEntry> tests;

                    // Build tests for each supported member
                    for (size_t i = 0; i < sdesc->members.size(); ++i) {
                        const auto& m = sdesc->members[i];
                        if (!m.type || !m.get_ptr) {
                            std::cout << "  -> skipping member[" << i << "] (null type or no get_ptr)\n";
                            continue;
                        }

                        if (m.type == floatDesc) {
                            float sample = 1.2345f + static_cast<float>(i) * 0.5f;
                            tests.push_back({ i, "float",
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
                            tests.push_back({ i, "double",
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
                            tests.push_back({ i, "int",
                                [sample](void* p) { *reinterpret_cast<int*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    return *reinterpret_cast<const int*>(a) == *reinterpret_cast<const int*>(b);
                                }
                                });
                        }
                        else if (m.type == uintDesc) {
                            unsigned sample = static_cast<unsigned>(i) * 11u + 3u;
                            tests.push_back({ i, "unsigned",
                                [sample](void* p) { *reinterpret_cast<unsigned*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    return *reinterpret_cast<const unsigned*>(a) == *reinterpret_cast<const unsigned*>(b);
                                }
                                });
                        }
                        else if (m.type == int64Desc) {
                            long long sample = static_cast<long long>(i) * 1000000LL + 5LL;
                            tests.push_back({ i, "long long",
                                [sample](void* p) { *reinterpret_cast<long long*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    return *reinterpret_cast<const long long*>(a) == *reinterpret_cast<const long long*>(b);
                                }
                                });
                        }
                        else if (m.type == uint64Desc) {
                            unsigned long long sample = static_cast<unsigned long long>(i) * 1000000ULL + 9ULL;
                            tests.push_back({ i, "unsigned long long",
                                [sample](void* p) { *reinterpret_cast<unsigned long long*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    return *reinterpret_cast<const unsigned long long*>(a) == *reinterpret_cast<const unsigned long long*>(b);
                                }
                                });
                        }
                        else if (m.type == boolDesc) {
                            bool sample = (i % 2) == 0;
                            tests.push_back({ i, "bool",
                                [sample](void* p) { *reinterpret_cast<bool*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    return *reinterpret_cast<const bool*>(a) == *reinterpret_cast<const bool*>(b);
                                }
                                });
                        }
                        else if (stringDesc && m.type == stringDesc) {
                            std::string sample = std::string("test_str_") + std::to_string(i);
                            tests.push_back({ i, "std::string",
                                [sample](void* p) { *reinterpret_cast<std::string*>(p) = sample; },
                                [sample](const void* a, const void* b) {
                                    return *reinterpret_cast<const std::string*>(a) == *reinterpret_cast<const std::string*>(b);
                                }
                                });
                        }
                        else {
                            std::cout << "  -> skipping member[" << i << "] type='" << m.type->ToString() << "' (unsupported for assignment/comparison)\n";
                        }
                    } // for members

                    // Assign values into src for all tests
                    for (const auto& te : tests) {
                        void* addr = sdesc->members[te.idx].get_ptr(&src);
                        try { te.assign(addr); }
                        catch (...) {
                            std::cout << "  -> exception assigning member[" << te.idx << "]\n";
                        }
                    }
                    if (sdesc->members.size() >= 3) {
                        *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&src)) = 10.0f;
                        *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&src)) = -3.5f;
                        *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&src)) = 0.25f;
                    }
                    else {
                        std::cout << "  WARN: not enough members to populate canonical values\n";
                    }

                    // 1) Text Serialize
                    std::stringstream ss;
                    td->Serialize(&src, ss);
                    std::string text_out = ss.str();
                    std::cout << "  Text Serialize output: " << text_out << "\n";

                    // 2) rapidjson SerializeJson -> string
                    rapidjson::Document dout;
                    td->SerializeJson(&src, dout);
                    rapidjson::StringBuffer sb;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
                    dout.Accept(writer);
                    std::cout << "  rapidjson Serialize output: " << sb.GetString() << "\n";

                    // 3) Round-trip deserialize
                    T dst{};
                    rapidjson::Document din;
                    din.Parse(sb.GetString());
                    td->Deserialize(&dst, din);

                    // Compare tested members
                    size_t matched = 0;
                    for (const auto& te : tests) {
                        const void* a = sdesc->members[te.idx].get_ptr(&src);
                        const void* b = sdesc->members[te.idx].get_ptr(&dst);
                        bool ok = false;
                        try { ok = te.compare(a, b); }
                        catch (...) { ok = false; }
                        std::cout << "  member[" << te.idx << "] (" << te.type_name << "): " << (ok ? "MATCH" : "MISMATCH") << "\n";
                        if (ok) ++matched;
                    }

                    bool match = (!tests.empty() && matched == tests.size());
                    if (tests.empty()) {
                        std::cout << "  WARN: no supported members were tested; ensure primitive descriptors are registered.\n";
                    }
                    std::cout << "  Tested members: " << tests.size() << ", matched: " << matched << "\n";
                    ENGINE_PRINT("  Round-trip equality: ", (match ? "OK" : "MISMATCH"), "\n");
                    if (!match) serialization_ok = false;
                }
            }
            catch (...) {
                std::cout << "FAIL: unknown error during serialization tests\n";
                serialization_ok = false;
            }
        }

        // --- Registry introspection (optional) ---
        std::cout << "\n[3] Registry contents (keys):\n";
        for (const auto& kv : TypeDescriptor::type_descriptor_lookup()) {
            std::cout << "  " << kv.first << "\n";
        }

        // --- Summary & exit code ---
        std::cout << "\n=== SUMMARY ===\n";
        std::cout << "Reflection: " << (reflection_ok ? "PASS" : "FAIL") << "\n";
        std::cout << "Serialization: " << (serialization_ok ? "PASS" : "FAIL") << "\n";

        if (!reflection_ok) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn,
                R"(
                NOTE: if you hit a linker error mentioning GetPrimitiveDescriptor<float&>() or you see member types printed with '&',
                apply the macro fix to strip references when resolving member types in the macro:
                Replace the TypeResolver line in REFL_REGISTER_PROPERTY with:
                  TypeResolver<std::remove_reference_t<decltype(std::declval<T>().VARIABLE)>>::Get()
                This prevents requesting descriptors for reference types (e.g. float&).
                )" "\n");
        }

	}
#endif

	// Note: Scene loading and lighting setup moved to InitializeGraphicsResources()
	// This will be called after the graphics context is ready

	//lightManager.printLightStats();

	// Test Audio
	/*{
		if (!AudioSystem::GetInstance().Initialise())
		{
			ENGINE_LOG_ERROR("Failed to initialize AudioSystem");
		}
		else
		{
			AudioHandle h = AudioSystem::GetInstance().LoadAudio("Resources/Audio/sfx/Test_duck.wav");
			if (h != 0) {
				AudioSystem::GetInstance().Play(h, false, 0.5f);
			}
		}
	}*/

	ENGINE_LOG_INFO("Engine initialization completed successfully");
	
	// Add some test logging messages
	ENGINE_LOG_WARN("This is a test warning message");
	ENGINE_LOG_ERROR("This is a test error message");
	
    std::cout << "test\n";
    
	return true;
}

bool Engine::InitializeGraphicsResources() {
	ENGINE_LOG_INFO("Initializing graphics resources...");

	// Load test scene
	SceneManager::GetInstance().LoadTestScene();

	// ---Set Up Lighting---
	LightManager& lightManager = LightManager::getInstance();
	const auto& pointLights = lightManager.getPointLights();
	// Set up directional light
	lightManager.setDirectionalLight(
		glm::vec3(-0.2f, -1.0f, -0.3f),
		glm::vec3(0.4f, 0.4f, 0.4f)
	);

	// Add point lights
	glm::vec3 lightPositions[] = {
		glm::vec3(0.7f,  0.2f,  2.0f),
		glm::vec3(2.3f, -3.3f, -4.0f),
		glm::vec3(-4.0f,  2.0f, -12.0f),
		glm::vec3(0.0f,  0.0f, -3.0f)
	};

	for (int i = 0; i < 4; i++)
	{
		lightManager.addPointLight(lightPositions[i], glm::vec3(0.8f, 0.8f, 0.8f));
	}

	// Set up spotlight
	lightManager.setSpotLight(
		glm::vec3(0.0f),
		glm::vec3(0.0f, 0.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
	);

	ENGINE_LOG_INFO("Graphics resources initialized successfully");
	return true;
}

bool Engine::InitializeAssets() {
	// Initialize asset meta files - called after platform is ready (e.g., Android AssetManager set)
	MetaFilesManager::InitializeAssetMetaFiles("Resources");
	return true;
}

void Engine::Update() {

    TimeManager::UpdateDeltaTime();
    
	// Only update the scene if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
        SceneManager::GetInstance().UpdateScene(TimeManager::GetDeltaTime()); // REPLACE WITH DT LATER


		// Test Audio
		AudioSystem::GetInstance().Update();
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
    } catch (const std::exception& e) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ENGINE] SceneManager::DrawScene() threw exception: %s", e.what());
    } catch (...) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ENGINE] SceneManager::DrawScene() threw unknown exception");
    }
#else
    SceneManager::GetInstance().DrawScene();
    //std::cout << "drawn scene\n";
#endif
}

void Engine::EndDraw() {
	WindowManager::SwapBuffers();

	// Only process input if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
		InputManager::Update();
	}

	WindowManager::PollEvents(); // Always poll events for UI and window management
}

void Engine::Shutdown() {
	ENGINE_LOG_INFO("Engine shutdown started");
	AudioSystem::GetInstance().Shutdown();
    EngineLogging::Shutdown();
    std::cout << "[Engine] Shutdown complete" << std::endl;
}

bool Engine::IsRunning() {
	return !WindowManager::ShouldClose();
    //
    //
}

// Game state management functions
void Engine::SetGameState(GameState state) {
	currentGameState = state;
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