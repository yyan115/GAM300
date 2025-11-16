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
#include <Input/InputManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include "Game AI/BrainSystems.hpp"
#include <Scene/SceneManager.hpp>
#include "TimeManager.hpp"
#include "Sound/AudioManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Performance/PerformanceProfiler.hpp"

#ifdef ANDROID
#include "Input/VirtualControls.hpp"
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
	InputManager::Initialize();

	// Initialize AudioManager on desktop now that platform assets are available
	if (!AudioManager::GetInstance().Initialise()) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to initialize AudioManager\n");
	} else {
		ENGINE_PRINT("[Engine] AudioManager initialized\n");
	}


	// Android: Asset initialization happens in JNI after AssetManager is set

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
		lastScenePath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Scenes/New Scene.scene";
		ENGINE_LOG_INFO("No previous scene found, loading default scene");
	}
	else {
		ENGINE_LOG_INFO("Loading last opened scene: " + lastScenePath);
	}
	SceneManager::GetInstance().LoadScene(lastScenePath);
#else
	// Game build always loads default scene
	SceneManager::GetInstance().LoadScene(AssetManager::GetInstance().GetRootAssetDirectory() + "/Scenes/luascene.scene"); ///Scenes/basicLevel.scene
	ENGINE_LOG_INFO("Loaded default scene");
#endif

#ifdef ANDROID
    // Initialize virtual controls for Android
    VirtualControls::Initialize();
    ENGINE_LOG_INFO("Virtual controls initialized");
#endif

	ENGINE_LOG_INFO("Graphics resources initialized successfully");
	return true;
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
        
        // Render virtual controls on top of everything (Android only)
        VirtualControls::Render(surfaceWidth, surfaceHeight);
        
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
		InputManager::Update();
	}

	WindowManager::PollEvents(); // Always poll events for UI and window management
}

void Engine::Shutdown() {

	ENGINE_LOG_INFO("Engine shutdown started");
	RunBrainExitSystem(ECSRegistry::GetInstance().GetActiveECSManager());
	AudioManager::GetInstance().Shutdown();
    EngineLogging::Shutdown();
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