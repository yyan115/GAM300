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
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to initialize logging system!\n");
		//std::cerr << "[Engine] Failed to initialize logging system!" << std::endl;
		return false;
	}
	SetGameState(GameState::PLAY_MODE);
	WindowManager::Initialize(SCR_WIDTH, SCR_HEIGHT, TEMP::windowTitle.c_str());

    ENGINE_PRINT("Engine initializing...");

	// WOON LI TEST CODE
	InputManager::Initialize();

	// Platform-specific asset initialization
#ifndef __ANDROID__
	// Desktop platforms: Initialize assets immediately (filesystem-based)
	MetaFilesManager::InitializeAssetMetaFiles("Resources");
	// Initialize AudioSystem on desktop now that platform assets are available
	if (!AudioSystem::GetInstance().Initialise()) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to initialize AudioSystem\n");
	} else {
		ENGINE_PRINT("[Engine] AudioSystem initialized\n");
	}
#endif
	// Android: Asset initialization happens in JNI after AssetManager is set

	//TEST ON ANDROID FOR REFLECTION - IF NOT WORKING, INFORM IMMEDIATELY
#if 1
	{
        bool reflection_ok = true;
        bool serialization_ok = true;

        ENGINE_PRINT("=== Running reflection + serialization single-main test for Matrix4x4 ===\n");

        // --- Reflection-only checks ---
        ENGINE_PRINT("\n[1] Reflection metadata + runtime access checks\n");

        using T = Matrix4x4;
        TypeDescriptor* td = nullptr;
        try {
            td = TypeResolver<T>::Get();
        }
        catch (const std::exception& ex) {
            //std::cout << "ERROR: exception while calling TypeResolver::Get(): " << ex.what() << "\n";
            ENGINE_PRINT("ERROR: exception while calling TypeResolver::Get(): ", ex.what(), "\n");
        }
        catch (...) {
            //std::cout << "ERROR: unknown exception calling TypeResolver::Get()\n";
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "ERROR: unknown exception calling TypeResolver::Get()\n");
        }

        if (!td) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: TypeResolver<Matrix4x4>::Get() returned null. Ensure REFL_REGISTER_START(Matrix4x4) is compiled & linked.\n");
            reflection_ok = false;
        }
        else {
            //std::cout << "Type name: " << td->ToString() << ", size: " << td->size << "\n";
            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "Type name: ", td, ", size: ", td->size);

            auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
            if (!sdesc) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: descriptor is not TypeDescriptor_Struct\n");
                //std::cout << "FAIL: descriptor is not TypeDescriptor_Struct\n";
                reflection_ok = false;
            }
            else {
                ENGINE_PRINT(EngineLogging::LogLevel::Debug, "Member count: ", sdesc->members.size(), "\n");
                //std::cout << "Member count: " << sdesc->members.size() << "\n";
                // Print members and basic checks
                for (size_t i = 0; i < sdesc->members.size(); ++i) {
                    const auto& m = sdesc->members[i];
                    std::string mname = m.name ? m.name : "<null>";
                    std::string tname = m.type ? m.type->ToString() : "<null-type>";
                    //std::cout << "  [" << i << "] name='" << mname << "' type='" << tname << "'\n";
                    ENGINE_PRINT("  [", i, "] name='", mname, "' type='", tname, "'\n");

                    if (!m.type) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: member has null TypeDescriptor\n");
                        reflection_ok = false;
                    }
                    if (tname.find('&') != std::string::npos) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: member type contains '&' (strip references in macro). See REFL_REGISTER_PROPERTY fix.\n");
                        reflection_ok = false;
                    }
                    if (!m.get_ptr) {
                        ENGINE_PRINT( EngineLogging::LogLevel::Error, "    -> FAIL: member.get_ptr is null\n");
                        reflection_ok = false;
                    }
                }

                // Runtime read/write via get_ptr to prove reflection can access object memory
                try {
                    T v{};
                    if (sdesc->members.size() >= 1) *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&v)) = 1.2345f;
                    if (sdesc->members.size() >= 2) *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&v)) = 2.5f;
                    if (sdesc->members.size() >= 3) *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&v)) = -7.125f;

                    bool values_ok = true;
                    if (sdesc->members.size() >= 3) {
                        float a = *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&v));
                        float b = *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&v));
                        float c = *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&v));
                        if (!(a == 1.2345f && b == 2.5f && c == -7.125f)) values_ok = false;
                    }
                    else {
                        ENGINE_PRINT( EngineLogging::LogLevel::Warn ,"    -> WARN: fewer than 3 members; cannot fully validate values\n");
                        values_ok = false;
                    }
                    ENGINE_PRINT(
                        EngineLogging::LogLevel::Info, "  Runtime read/write via get_ptr: ", (values_ok ? "OK" : "MISMATCH"), "\n");
                    if (!values_ok) reflection_ok = false;
                }
                catch (const std::exception& ex) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: exception during runtime read/write: " , ex.what() , "\n");
                    reflection_ok = false;
                }
                catch (...) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Error, "    -> FAIL: unknown exception during runtime read/write\n");
                    reflection_ok = false;
                }
            }
        }

        // --- Serialization checks (uses TypeDescriptor::Serialize / SerializeJson / Deserialize) ---
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "\n[2] Serialization + round-trip checks\n");

        if (!td) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "SKIP: serialization checks because TypeDescriptor was not available\n");
            serialization_ok = false;
        }
        else {
            try {
                // Create sample object and populate via reflection
                T src{};
                auto* sdesc = dynamic_cast<TypeDescriptor_Struct*>(td);
                if (!sdesc) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Error,"FAIL: not a struct descriptor; cannot serialize\n");
                    serialization_ok = false;
                }
                else {
                    if (sdesc->members.size() >= 3) {
                        *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&src)) = 10.0f;
                        *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&src)) = -3.5f;
                        *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&src)) = 0.25f;
                    }
                    else {
                        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "  WARN: not enough members to populate canonical values\n");
                    }
                    // 1) Text Serialize
                    std::stringstream ss;
                    td->Serialize(&src, ss);
                    std::string text_out = ss.str();
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "  Text Serialize output: ", text_out + "\n");
                    // 2) rapidjson SerializeJson -> string
                    rapidjson::Document dout;
                    td->SerializeJson(&src, dout);
                    rapidjson::StringBuffer sb;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
                    dout.Accept(writer);
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "  rapidjson Serialize output: ",  sb.GetString(), "\n");
                    // 3) Round-trip deserialize
                    T dst{};
                    rapidjson::Document din;
                    din.Parse(sb.GetString());
                    td->Deserialize(&dst, din);

                    bool match = true;
                    if (sdesc->members.size() >= 3) {
                        float a = *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&src));
                        float b = *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&src));
                        float c = *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&src));
                        float a2 = *reinterpret_cast<float*>(sdesc->members[0].get_ptr(&dst));
                        float b2 = *reinterpret_cast<float*>(sdesc->members[1].get_ptr(&dst));
                        float c2 = *reinterpret_cast<float*>(sdesc->members[2].get_ptr(&dst));
                        if (!(a == a2 && b == b2 && c == c2)) match = false;
                    }
                    else {
                        match = false;
                    }

                    ENGINE_PRINT("  Round-trip equality: ", (match ? "OK" : "MISMATCH"), "\n");
                    if (!match) serialization_ok = false;
                }
            }
            catch (const std::exception& ex) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "FAIL: exception during serialization tests: ",  ex.what(), "\n");
                serialization_ok = false;
            }
            catch (...) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error,
                    "FAIL: unknown error during serialization tests\n");
                serialization_ok = false;
            }
        }

        // --- Registry introspection (optional) ---
        ENGINE_PRINT("\n[3] Registry contents (keys):\n");

        for (const auto& kv : TypeDescriptor::type_descriptor_lookup()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "  ", kv.first, "\n");
        }
        // --- Summary & exit code ---

        ENGINE_PRINT("\n=== SUMMARY ===\n");
        ENGINE_PRINT(reflection_ok ? EngineLogging::LogLevel::Info : EngineLogging::LogLevel::Error, "Reflection: ", (reflection_ok ? "PASS" : "FAIL"), "\n");
        ENGINE_PRINT(serialization_ok ? EngineLogging::LogLevel::Info : EngineLogging::LogLevel::Error, "Serialization: ", (serialization_ok ? "PASS" : "FAIL"), "\n");

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
    ENGINE_LOG_INFO("Loaded test scene");

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
	//MetaFilesManager::InitializeAssetMetaFiles("Resources");
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
    ENGINE_PRINT("[Engine] Shutdown complete\n"); 
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