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

namespace TEMP {
	std::string windowTitle = "GAM300";
}

// Static member definition
GameState Engine::currentGameState = GameState::EDIT_MODE;

const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

//RenderSystem& renderer = RenderSystem::getInstance();
//std::shared_ptr<Model> backpackModel;
//std::shared_ptr<Shader> shader;
////----------------LIGHT-------------------
//std::shared_ptr<Shader> lightShader;
//std::shared_ptr<Mesh> lightCubeMesh;

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
	MetaFilesManager::InitializeAssetMetaFiles("Resources");

	// Note: Scene loading and lighting setup moved to InitializeGraphicsResources()
	// This will be called after the graphics context is ready

	//lightManager.printLightStats();

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

void Engine::Update() {
	// Only update the scene if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
		SceneManager::GetInstance().UpdateScene(WindowManager::getDeltaTime()); // REPLACE WITH DT LATER
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

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // Bright red - should be very obvious

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

    // __android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL State - Display: %p, Context: %p, Surface: %p",
    //                     display, context, surface);

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
#endif

    // --- Bind default framebuffer ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Query actual surface size ---
    int width = WindowManager::GetViewportWidth();
    int height = WindowManager::GetViewportHeight();
    glViewport(0, 0, width, height);

    // --- Disable states that can hide geometry ---
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // --- Clear screen to grey for visibility ---
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // --- Triangle test (disabled) ---
    /*static GLuint vao = 0, vbo = 0, shaderProgram = 0;
    if (vao == 0) {
#ifdef ANDROID
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up triangle shaders and geometry...");
#endif
        float vertices[] = {
            // positions        // colors
             0.0f,  0.5f, 0.0f,  1,0,0,  // top (red)
            -0.5f, -0.5f, 0.0f,  0,1,0,  // left (green)
             0.5f, -0.5f, 0.0f,  0,0,1   // right (blue)
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // color
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // --- Minimal vertex + fragment shaders ---
        const char* vertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 fragColor;
void main() {
    fragColor = aColor;
    gl_Position = vec4(aPos, 1.0);
}
)";

        const char* fragmentSrc = R"(#version 300 es
precision mediump float;
in vec3 fragColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(fragColor, 1.0);
}
)";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSrc, nullptr);
        glCompileShader(vertexShader);

#ifdef ANDROID
        GLint success;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
            __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Vertex shader compilation failed: %s", infoLog);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "GAM300", "Vertex shader compiled successfully");
        }
#endif

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSrc, nullptr);
        glCompileShader(fragmentShader);

#ifdef ANDROID
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
            __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Fragment shader compilation failed: %s", infoLog);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "GAM300", "Fragment shader compiled successfully");
        }
#endif

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

#ifdef ANDROID
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Shader program linking failed: %s", infoLog);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "GAM300", "Shader program linked successfully");
        }
#endif

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

#ifdef ANDROID
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "Triangle setup completed");
#endif
    }

    // --- Draw triangle ---
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "About to draw triangle - shader: %d, vao: %d", shaderProgram, vao);

    // Check if shader program is valid
    GLint isProgram = glIsProgram(shaderProgram);
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "glIsProgram result: %d", isProgram);

    if (shaderProgram == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Shader program is 0 - not created properly!");
        return;
    }
#endif

    glUseProgram(shaderProgram);
#ifdef ANDROID
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error after glUseProgram: %d", error);
    }
#endif

    glBindVertexArray(vao);
#ifdef ANDROID
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error after glBindVertexArray: %d", error);
    }
#endif

    glDrawArrays(GL_TRIANGLES, 0, 3);

#ifdef ANDROID
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error after glDrawArrays: %d", error);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "Triangle rendered successfully");
    }
#endif*/
	SceneManager::GetInstance().DrawScene();
	
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
    EngineLogging::Shutdown();
    std::cout << "[Engine] Shutdown complete" << std::endl;
}

bool Engine::IsRunning() {
	return !WindowManager::ShouldClose();
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