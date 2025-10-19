#pragma once
#include <string>
#include <Scene/Scene.hpp>
#include "Engine.h"

class SceneManager {
public:
	static ENGINE_API SceneManager& GetInstance();

	// Temporary function to load the test scene.
	void LoadTestScene();

	void ENGINE_API LoadScene(const std::string& scenePath);

	void UpdateScene(double dt);

	void DrawScene();

	void ExitScene();

	void ENGINE_API SaveScene();

	void ENGINE_API InitializeScenePhysics();
	void ENGINE_API ShutDownScenePhysics();

	// Saves the current scene to a temporary file.
	// To be called when the play button is pressed in the editor to save the scene state just before hitting play.
	void ENGINE_API SaveTempScene();

	// Reloads the current scene's temporary file.
	// To be called when the stop button is pressed in the editor to revert any changes made during play mode.
	void ENGINE_API ReloadTempScene();

	// Get the current scene (returns null if no scene is loaded)
	IScene* GetCurrentScene() { return currentScene.get(); }

	ENGINE_API std::string GetSceneName() const;

private:
	SceneManager() = default;

	ENGINE_API ~SceneManager();

	std::unique_ptr<IScene> currentScene = nullptr;
	std::string currentScenePath;
	std::string currentSceneName;
};