#include "pch.h"
#include <ECS/ECSRegistry.hpp>
#include <Scene/SceneManager.hpp>
#include <Scene/SceneInstance.hpp>
#include <filesystem>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <ECS/NameComponent.hpp>
#include "rapidjson/prettywriter.h"
#include <Serialization/Serializer.hpp>
#include "Logging.hpp"
#include <Utilities/FileUtilities.hpp>
#include "Logging.hpp"

ENGINE_API SceneManager::~SceneManager() {
	ExitScene();
}

// Temporary function to load the test scene.
void SceneManager::LoadTestScene() {
	ECSRegistry::GetInstance().CreateECSManager("Resources/Scenes/FakeScene.scene");
	currentScene = std::make_unique<SceneInstance>("Resources/Scenes/FakeScene.scene");
	currentScenePath = "Resources/Scenes/FakeScene.scene";
	currentSceneName = "FakeScene";
	currentScene->Initialize();
}

// Load a new scene from the specified path.
// The current scene is exited and cleaned up before loading the new scene.
// Also sets the new scene as the active ECSManager in the ECSRegistry.
ENGINE_API void SceneManager::LoadScene(const std::string& scenePath) {
	// Exit and clean up the current scene if it exists.
	if (currentScene) 
    {
		currentScene->Exit();

		ECSRegistry::GetInstance().GetECSManager(currentScenePath).ClearAllEntities();
		ECSRegistry::GetInstance().RenameECSManager(currentScenePath, scenePath);
	}

	// Create and initialize the new scene.
	currentScene = std::make_unique<SceneInstance>(scenePath);
	currentScenePath = scenePath;
	std::filesystem::path p(currentScenePath);
	currentSceneName = p.stem().generic_string();

	// Deserialize the new scene data.
	Serializer::DeserializeScene(scenePath);
    
	// Initialize the new scene.
	currentScene->Initialize();
}

void SceneManager::UpdateScene(double dt) {
	if (currentScene) {
		currentScene->Update(dt);
	}
}

void SceneManager::DrawScene() {
	if (currentScene) {
		currentScene->Draw();
		//std::cout << "drawn scene scenemanager\n";
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

//Simple JSON save scene
ENGINE_API void SceneManager::SaveScene() {
	Serializer::SerializeScene(currentScenePath); // TEMP, replace with currentScenePath later
	if (FileUtilities::CopyFile(currentScenePath, (FileUtilities::GetSolutionRootDir() / currentScenePath).generic_string())) {
		ENGINE_LOG_INFO("Copied scene file to root project folder: " + currentScenePath);
	}
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
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "Temp file does not exist: ", tempScenePath, "\n");
		return; // Early exit if needed
	}
}

std::string SceneManager::GetSceneName() const {
	return currentSceneName;
}
