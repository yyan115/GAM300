#pragma once
#include <string>
#include <Scene/Scene.hpp>
#include "Engine.h"

class ENGINE_API SceneManager {
public:
	static SceneManager& GetInstance() {
		static SceneManager instance;
		return instance;
	}

	// Temporary function to load the test scene.
	void LoadTestScene();

	void LoadScene(const std::string& scenePath);

	void UpdateScene(double dt);

	void DrawScene();

	void ExitScene();

	void SaveScene();

	void InitializeScenePhysics();
	void ShutDownScenePhysics();

	// Saves the current scene to a temporary file.
	// To be called when the play button is pressed in the editor to save the scene state just before hitting play.
	void SaveTempScene();

	// Reloads the current scene's temporary file.
	// To be called when the stop button is pressed in the editor to revert any changes made during play mode.
	void ReloadTempScene();

	// Get the current scene (returns null if no scene is loaded)
	IScene* GetCurrentScene() { return currentScene.get(); }

	std::string GetSceneName() const;

private:
	SceneManager() = default;

	~SceneManager();

	std::unique_ptr<IScene> currentScene = nullptr;
	std::string currentScenePath;
	std::string currentSceneName;
};