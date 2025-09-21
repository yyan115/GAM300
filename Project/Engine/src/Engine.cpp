#include "pch.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Graphics/LightManager.hpp"

#include "Engine.h"
#include "Logging.hpp"

#include <WindowManager.hpp>
#include <Input/InputManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include "TimeManager.hpp"

namespace TEMP {
	std::string windowTitle = "GAM300";
}

// Static member definition
GameState Engine::s_CurrentGameState = GameState::EDIT_MODE;

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

	WindowManager::Initialize(SCR_WIDTH, SCR_HEIGHT, TEMP::windowTitle.c_str());

    ENGINE_LOG_INFO("Engine initializing...");

	// WOON LI TEST CODE
	InputManager::Initialize(WindowManager::getWindow());
	MetaFilesManager::InitializeAssetMetaFiles("Resources");

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

	//lightManager.printLightStats();

	ENGINE_LOG_INFO("Engine initialization completed successfully");
	
	// Add some test logging messages
	ENGINE_LOG_WARN("This is a test warning message");
	ENGINE_LOG_ERROR("This is a test error message");
	
	return true;
}

void Engine::Update() {
	// Only update the scene if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
		SceneManager::GetInstance().UpdateScene(TimeManager::GetDeltaTime()); // REPLACE WITH DT LATER
	}
}

void Engine::StartDraw() {
}

void Engine::Draw() {
	SceneManager::GetInstance().DrawScene();
	
}

void Engine::EndDraw() {
	glfwSwapBuffers(WindowManager::getWindow());

	// Only process input if the game should be running (not paused)
	if (ShouldRunGameLogic()) {
		InputManager::Update();
	}

	glfwPollEvents(); // Always poll events for UI and window management
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
	s_CurrentGameState = state;
}

GameState Engine::GetGameState() {
	return s_CurrentGameState;
}

bool Engine::ShouldRunGameLogic() {
	return s_CurrentGameState == GameState::PLAY_MODE;
}

bool Engine::IsEditMode() {
	return s_CurrentGameState == GameState::EDIT_MODE;
}

bool Engine::IsPlayMode() {
	return s_CurrentGameState == GameState::PLAY_MODE;
}

bool Engine::IsPaused() {
	return s_CurrentGameState == GameState::PAUSED_MODE;
}