#pragma once
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/Font.hpp"
#include <Scene/Scene.hpp>
#include <Graphics/GraphicsManager.hpp>
#include "ECS/ECSManager.hpp"
class SceneInstance : public IScene {
public:
	SceneInstance() = default;
	SceneInstance(const std::string& path) : IScene(path) {}
	~SceneInstance() override = default;

	void Initialize() override;
	void InitializeJoltPhysics() override;
	void InitializePhysics() override;

	void Update(double dt) override;
	void Draw() override;
	void Exit() override;
	void ShutDownPhysics() override;
	void processInput(float deltaTime); // temp function

	const unsigned int SCR_WIDTH = 800;
	const unsigned int SCR_HEIGHT = 600;

	// camera
	/*Camera camera{ glm::vec3(0.0f, 0.0f, 3.0f) };
	float lastX = SCR_WIDTH / 2.0f;
	float lastY = SCR_HEIGHT / 2.0f;
	bool firstMouse = true;*/

	std::shared_ptr<Model> backpackModel;
	std::shared_ptr<Shader> shader;

	// Text rendering members
	std::shared_ptr<Font> testFont;
	std::shared_ptr<Shader> textShader;

private:
	void CreateHDRTestScene(ECSManager& ecsManager); // Testing
	void CreateDefaultCamera(ECSManager& ecsManager); // Create default main camera for new scenes
};