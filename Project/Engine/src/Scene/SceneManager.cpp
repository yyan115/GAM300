#include "pch.h"
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include <Scene/SceneInstance.hpp>
#include <filesystem>
#ifdef ANDROID
#include <android/log.h>
#endif

SceneManager::~SceneManager() {
	ExitScene();
}

// Temporary function to load the test scene.
void SceneManager::LoadTestScene() {
	ECSRegistry::GetInstance().CreateECSManager("TestScene");
	currentScene = std::make_unique<TestScene>();
	currentScenePath = "TestScene";
	currentScene->Initialize();
}

// Load a new scene from the specified path.
// The current scene is exited and cleaned up before loading the new scene.
// Also sets the new scene as the active ECSManager in the ECSRegistry.
void SceneManager::LoadScene(const std::string& scenePath) {
	// Exit and clean up the current scene if it exists.
	if (currentScene) {
		currentScene->Exit();

		ECSRegistry::GetInstance().GetECSManager(currentScenePath).ClearAllEntities();
		ECSRegistry::GetInstance().RenameECSManager(currentScenePath, scenePath);
	}

	// Create and initialize the new scene.
	currentScene = std::make_unique<TestScene>();
	currentScenePath = scenePath;

	// Deserialize the new scene data.
	//Serializer::GetInstance().DeserializeScene(scenePath);

	// Initialize the new scene.
	currentScene->Initialize();
}

void SceneManager::UpdateScene(double dt) {
	if (currentScene) {
		currentScene->Update(dt);
	}
}

void SceneManager::DrawScene() {
#ifdef ANDROID
	static int debugCount = 0;
	if (debugCount % 60 == 0) { // Log every 60 frames
		if (currentScene) {
			__android_log_print(ANDROID_LOG_INFO, "GAM300", "SceneManager::DrawScene() - calling currentScene->Draw()");
		} else {
			__android_log_print(ANDROID_LOG_WARN, "GAM300", "SceneManager::DrawScene() - NO CURRENT SCENE SET!");
		}
	}
	debugCount++;
#endif

	if (currentScene) {
		currentScene->Draw();
	}
}

void SceneManager::ExitScene() {
	if (currentScene) {
		//Serializer::GetInstance().SerializeScene(currentScenePath);
		currentScene->Exit();
		currentScene.reset();
		currentScenePath.clear();
	}
}

void SceneManager::SaveScene() {
	// Serialize the current scene data.
	//Serializer::GetInstance().SerializeScene(currentScenePath);
}

void SceneManager::SaveTempScene() {
	// Serialize the current scene data to a temporary file.
	std::string tempScenePath = currentScenePath + ".temp";
	//Serializer::GetInstance().SerializeScene(tempScenePath);
}

void SceneManager::ReloadTempScene() {
	std::string tempScenePath = currentScenePath + ".temp";
	if (std::filesystem::exists(tempScenePath)) {
		//Serializer::GetInstance().ReloadScene(tempScenePath);
	}
	else {
		// Handle the case where the temp file doesn't exist
		std::cerr << "Temp file does not exist: " << tempScenePath << std::endl;
		return; // Early exit if needed
	}
}
