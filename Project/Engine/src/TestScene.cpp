#include "pch.h"
#include "TestScene.hpp"
#include "Input/InputManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include <Graphics/Renderer.hpp>
#include "Asset Manager/AssetManager.hpp"

void TestScene::Initialize()
{
	// Initialization code for the test scene

	// WOON LI TEST CODE
	// Temp testing code - create some ECS managers and entities
	ECSRegistry::GetInstance().CreateECSManager("MainWorld");
	ECSRegistry::GetInstance().SetActiveECSManager("MainWorld"); // Set as active for editor access
	// ECSRegistry::GetInstance().CreateECSManager("SecondaryWorld");
	ECSManager &mainECS = ECSRegistry::GetInstance().GetECSManager("MainWorld");
	// ECSManager& secondaryECS = ECSRegistry::GetInstance().GetECSManager("SecondaryWorld");

	// Create an entity with a Renderer component in the main ECS manager
	Entity testEntt = mainECS.CreateEntity();
	mainECS.AddComponent<Renderer>(testEntt, Renderer{});
	mainECS.AddComponent<Transform>(testEntt, Transform{});
	Renderer& renderer = mainECS.GetComponent<Renderer>(testEntt);
	renderer.model = AssetManager::GetInstance().GetAsset<Model>("Resources/Models/backpack/backpack.obj");
	renderer.shader = AssetManager::GetInstance().GetAsset<Shader>("Resources/Shaders/default");
	//Transform& transform = mainECS.GetComponent<Transform>(testEntt);
	//transform.position = { 0, 0, 0 };
	//transform.scale = { .1, .1, .1 };
	//transform.rotation = { 0, 0, 0 };

	
	<< "[TestScene] Created backpack entity with ID: " << testEntt << std::endl;

	glm::mat4 transform = glm::mat4(1.0f);
	transform = glm::translate(transform, glm::vec3(0.0f, -1.0f, -5.0f)); // Move it away from camera and down a bit
	transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));		  // Larger scale to make it more visible
	mainECS.AddComponent<ModelRenderComponent>(testEntt, ModelRenderComponent{AssetManager::GetInstance().GetAsset<Model>("Resources/Models/backpack/backpack.obj"),
																			  AssetManager::GetInstance().GetAsset<Shader>("Resources/Shaders/default"),
																			  transform});

	std::cout << "[TestScene] Added ModelRenderComponent to entity " << testEntt << std::endl;

	// mainECS.CreateEntity();
	// mainECS.CreateEntity();
	// mainECS.CreateEntity();
	// mainECS.CreateEntity();

	glm::mat4 transform = glm::mat4(1.0f);
	std::cout << "[TestScene] Created backpack entity with ID: " << testEntt << std::endl;

	glm::mat4 transform = glm::mat4(1.0f);
	transform = glm::translate(transform, glm::vec3(0.0f, -1.0f, -5.0f)); // Move it away from camera and down a bit
	transform = glm::scale(transform, glm::vec3(0.5f, 0.5f, 0.5f));		  // Larger scale to make it more visible
	mainECS.AddComponent<ModelRenderComponent>(testEntt, ModelRenderComponent{AssetManager::GetInstance().GetAsset<Model>("Resources/Models/backpack/backpack.obj"),
																			  AssetManager::GetInstance().GetAsset<Shader>("Resources/Shaders/default"),
																			  transform});

	std::cout << "[TestScene] Added ModelRenderComponent to entity " << testEntt << std::endl;

	// mainECS.CreateEntity();
	// mainECS.CreateEntity();
	// mainECS.CreateEntity();
	// mainECS.CreateEntity();

	// mainECS.DestroyEntity(2);

	// secondaryECS.CreateEntity();
	// secondaryECS.CreateEntity();
	// secondaryECS.CreateEntity();

	// secondaryECS.ClearAllEntities();

	// GRAPHICS TEST CODE
	mainECS.transformSystem->Initialise();
	mainECS.renderSystem->Initialise(SCR_WIDTH, SCR_HEIGHT);

	// Loads model
	// backpackModel = std::make_shared<Model>("Resources/Models/backpack/backpack.obj");
	// shader = std::make_shared<Shader>("Resources/Shaders/default.vert", "Resources/Shaders/default.frag");

	// Creates light
	lightShader = std::make_shared<Shader>();
	lightShader->LoadAsset("Resources/Shaders/light");
	std::vector<std::shared_ptr<Texture>> emptyTextures = {};
	lightCubeMesh = std::make_shared<Mesh>(lightVertices, lightIndices, emptyTextures);

	// Sets camera
	mainECS.renderSystem->SetCamera(&camera);

	// Initialize systems.

	std::cout << "TestScene Initialized" << std::endl;
}

void TestScene::Update()
{
	// Update logic for the test scene
	ECSManager &mainECS = ECSRegistry::GetInstance().GetECSManager("MainWorld");

	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	processInput();

	mainECS.transformSystem->update();

	//RenderSystem::getInstance().BeginFrame();
	mainECS.renderSystem->Clear();

	// glm::mat4 transform = glm::mat4(1.0f);
	// transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.0f));
	// transform = glm::scale(transform, glm::vec3(0.1f, 0.1f, 0.1f));
	// RenderSystem::getInstance().Submit(backpackModel, transform, shader);

	mainECS.renderSystem->Render();
	mainECS.renderSystem->EndFrame();

	// Draw light cube
	const glm::vec3 *lightPositions = mainECS.renderSystem->getPointLightPositions();
	for (unsigned int i = 0; i < 4; i++)
	{
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, lightPositions[i]);
		lightShader->setMat4("model", lightModel);
		lightCubeMesh->Draw(*lightShader, camera);
	}

	// Update systems.
}

void TestScene::Exit()
{
	// Cleanup code for the test scene

	// Exit systems.

	std::cout << "TestScene Exited" << std::endl;
}

void TestScene::processInput()
{
	if (InputManager::GetKeyDown(GLFW_KEY_ESCAPE))
		glfwSetWindowShouldClose(WindowManager::getWindow(), true);

	float cameraSpeed = 2.5f * deltaTime;
	if (InputManager::GetKey(GLFW_KEY_W))
		camera.Position += cameraSpeed * camera.Front;
	if (InputManager::GetKey(GLFW_KEY_S))
		camera.Position -= cameraSpeed * camera.Front;
	if (InputManager::GetKey(GLFW_KEY_A))
		camera.Position -= glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;
	if (InputManager::GetKey(GLFW_KEY_D))
		camera.Position += glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;

	double xpos = InputManager::GetMouseX();
	double ypos = InputManager::GetMouseY();

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	double xoffset = xpos - lastX;
	double yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement((float)xoffset, (float)yoffset);
}

void TestScene::DrawLightCubes()
{
	DrawLightCubes(camera);
}

void TestScene::DrawLightCubes(const Camera &cameraOverride)
{
	DrawLightCubes(camera);
}

void TestScene::DrawLightCubes(const Camera &cameraOverride)
{
	// Get light positions from LightManager instead of renderSystem
	LightManager &lightManager = LightManager::getInstance();
	const auto &pointLights = lightManager.getPointLights();

	// Draw light cubes at point light positions
	for (size_t i = 0; i < pointLights.size() && i < 4; i++)
	{
		lightShader->Activate();

		// Set up matrices for light cube
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, pointLights[i].position);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f)); // Make them smaller

		// Set up view and projection matrices using the provided camera
		glm::mat4 view = cameraOverride.GetViewMatrix();
		glm::mat4 projection = glm::perspective(
			glm::radians(cameraOverride.Zoom),
			(float)WindowManager::GetWindowWidth() / (float)WindowManager::GetWindowHeight(),
			0.1f, 100.0f);

		lightShader->setMat4("model", lightModel);
		lightShader->setMat4("view", view);
		lightShader->setMat4("projection", projection);
		// lightShader->setVec3("lightColor", pointLights[i].diffuse); // Use light color

		lightCubeMesh->Draw(*lightShader, cameraOverride);
	}
}