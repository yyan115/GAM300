#include "pch.h"
#include <Scene/SceneInstance.hpp>
#include <Input/InputManager.hpp>
#include <Input/Keys.h>
#include <WindowManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Asset Manager/AssetManager.hpp>
#include "TimeManager.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Transform/TransformComponent.hpp>
#include <Graphics/TextRendering/TextUtils.hpp>
#include "ECS/NameComponent.hpp"
#include "Serialization/Serializer.hpp"
#include "Sound/AudioComponent.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <Logging.hpp>

void SceneInstance::Initialize() {
	// Initialize GraphicsManager first
	GraphicsManager& gfxManager = GraphicsManager::GetInstance();
	//gfxManager.Initialize(WindowManager::GetWindowWidth(), WindowManager::GetWindowHeight());
	gfxManager.Initialize(RunTimeVar::window.width, RunTimeVar::window.height);
	// WOON LI TEST CODE
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetECSManager(scenePath);

	if (scenePath == "TestScene") {
		// Create a backpack entity with a Renderer component in the main ECS manager
		Entity backpackEntt = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(backpackEntt, { 0, 0, 0 });
		ecsManager.transformSystem->SetLocalScale(backpackEntt, { .1f, .1f, .1f });
		ecsManager.transformSystem->SetLocalRotation(backpackEntt, { 0, 0, 0 });
		NameComponent& backpackName = ecsManager.GetComponent<NameComponent>(backpackEntt);
		backpackName.name = "dora the explorer";
		ecsManager.AddComponent<ModelRenderComponent>(backpackEntt, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile("Resources/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default"))});
		//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
		//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

		Entity backpackEntt2 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(backpackEntt2, { 1, -0.5f, 0 });
		ecsManager.transformSystem->SetLocalScale(backpackEntt2, { .2f, .2f, .2f });
		ecsManager.transformSystem->SetLocalRotation(backpackEntt2, { 0, 0, 0 });
		NameComponent& backpack2Name = ecsManager.GetComponent<NameComponent>(backpackEntt2);
		backpack2Name.name = "ash ketchum";
		ecsManager.AddComponent<ModelRenderComponent>(backpackEntt2, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile("Resources/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")) });
		//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt2, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
		//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

		Entity backpackEntt3 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(backpackEntt3, { -2, 0.5f, 0 });
		ecsManager.transformSystem->SetLocalScale(backpackEntt3, { .5f, .5f, .5f });
		ecsManager.transformSystem->SetLocalRotation(backpackEntt3, { 50, 70, 20 });
		NameComponent& backpack3Name = ecsManager.GetComponent<NameComponent>(backpackEntt3);
		backpack3Name.name = "indiana jones";
		ecsManager.AddComponent<ModelRenderComponent>(backpackEntt3, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile("Resources/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")) });
		//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt3, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
		//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

		// Text entity test
		Entity text = ecsManager.CreateEntity();
		ecsManager.GetComponent<NameComponent>(text).name = "Text1";
		ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "hello woody", 48, MetaFilesManager::GetGUID128FromAssetFile("Resources/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
		//ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "Hello World!", ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf"), ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text")) });
		TextRenderComponent& textComp = ecsManager.GetComponent<TextRenderComponent>(text);
		TextUtils::SetPosition(textComp, Vector3D(800, 100, 0));
		TextUtils::SetAlignment(textComp, TextRenderComponent::Alignment::CENTER);

		//Entity text2 = ecsManager.CreateEntity();
		//ecsManager.GetComponent<NameComponent>(text2).name = "Text2";
		//ecsManager.AddComponent<TextRenderComponent>(text2, TextRenderComponent{ "woohoo?", ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf", 20), ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text")) });
		//TextRenderComponent& textComp2 = ecsManager.GetComponent<TextRenderComponent>(text2);
		//TextUtils::SetPosition(textComp2, Vector3D(800, 800, 0));
		//TextUtils::SetAlignment(textComp2, TextRenderComponent::Alignment::CENTER);
	}

	// Creates light
	lightShader = std::make_shared<Shader>();
	lightShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("light"));
	//lightShader->LoadAsset("Resources/Shaders/light");
	std::vector<std::shared_ptr<Texture>> emptyTextures = {};
	lightCubeMesh = std::make_shared<Mesh>(lightVertices, lightIndices, emptyTextures);

	// Sets camera
	gfxManager.SetCamera(&camera);

	// Test Audio
	{
		// Initialize AudioSystem
		if (!AudioSystem::GetInstance().Initialise())
		{
			ENGINE_LOG_ERROR("Failed to initialize AudioSystem");
		}
		else
		{
			// Create an entity with AudioComponent
			Entity audioEntity = ecsManager.CreateEntity();
			ecsManager.transformSystem->SetLocalPosition(audioEntity, { 0, 0, 0 });
			NameComponent& audioName = ecsManager.GetComponent<NameComponent>(audioEntity);
			audioName.name = "Audio Test Entity";
			
			// Add AudioComponent
			AudioComponent audioComp;
			audioComp.AudioAssetPath = "Resources/Audio/sfx/Test_duck.wav";
			audioComp.Volume = 0.8f;
			audioComp.Loop = false;
			audioComp.PlayOnStart = true;
			audioComp.Spatialize = false;
			ecsManager.AddComponent<AudioComponent>(audioEntity, audioComp);
			
			// The AudioComponent will automatically load and play the audio on awake
		}
	}

	// Initialize systems.
	ecsManager.transformSystem->Initialise();
	ecsManager.modelSystem->Initialise();
	ecsManager.debugDrawSystem->Initialise();
	ecsManager.textSystem->Initialise();

	ENGINE_PRINT("Scene Initialized\n");
}

void SceneInstance::Update(double dt) {
	dt;

	// Update logic for the test scene
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	processInput((float)TimeManager::GetDeltaTime());

	// Update systems.
	mainECS.transformSystem->Update();
	
	if (mainECS.audioSystem)
	{
		mainECS.audioSystem->Update();
	}
}

void SceneInstance::Draw() {
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	GraphicsManager& gfxManager = GraphicsManager::GetInstance();
	//RenderSystem::getInstance().BeginFrame();
	gfxManager.BeginFrame();
	gfxManager.Clear();

	//glm::mat4 transform = glm::mat4(1.0f);
	//transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.0f));
	//transform = glm::scale(transform, glm::vec3(0.1f, 0.1f, 0.1f));
	//RenderSystem::getInstance().Submit(backpackModel, transform, shader);

	gfxManager.SetCamera(&camera);
	if (mainECS.modelSystem)
	{
		mainECS.modelSystem->Update();
	}
	if (mainECS.textSystem)
	{
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call textSystem->Update()");
#endif
		mainECS.textSystem->Update();
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "textSystem->Update() completed");
#endif
	}
	// Test debug drawing
	//DebugDrawSystem::DrawCube(Vector3D(0, 1, 0), Vector3D(1, 1, 1), Vector3D(1, 0, 0)); // Red cube above origin
	//DebugDrawSystem::DrawSphere(Vector3D(2, 0, 0), 1.0f, Vector3D(0, 1, 0)); // Green sphere to the right
	//DebugDrawSystem::DrawLine(Vector3D(0, 0, 0), Vector3D(3, 3, 3), Vector3D(0, 0, 1)); // Blue line diagonal
	//auto backpackModel = ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj");
	//DebugDrawSystem::DrawMeshWireframe(backpackModel, Vector3D(-2, 0, 0), Vector3D(1, 1, 0), 0.0f);

	// Update debug draw system to submit to graphics manager
	if (mainECS.debugDrawSystem)
	{
		mainECS.debugDrawSystem->Update();
	}
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call gfxManager.Render()");
#endif
	gfxManager.Render();
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "gfxManager.Render() completed");
#endif

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call DrawLightCubes()");
#endif
	// 5. Draw light cubes manually (temporary - you can make this a system later)
	DrawLightCubes();
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() completed");
#endif

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call gfxManager.EndFrame()");
#endif
	// 6. End frame
	gfxManager.EndFrame();
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "gfxManager.EndFrame() completed");
#endif

//std::cout << "drawn\n";
}

void SceneInstance::Exit() {
	// Exit systems.
	//ECSRegistry::GetInstance().GetECSManager(scenePath).modelSystem->Exit();
	ENGINE_PRINT("TestScene Exited\n");
	//std::cout << "TestScene Exited" << std::endl;
}

void SceneInstance::processInput(float deltaTime)
{
	if (InputManager::GetKeyDown(Input::Key::ESC))
		WindowManager::SetWindowShouldClose();

	float cameraSpeed = 2.5f * deltaTime;
	if (InputManager::GetKey(Input::Key::W))
		camera.Position += cameraSpeed * camera.Front;
	if (InputManager::GetKey(Input::Key::S))
		camera.Position -= cameraSpeed * camera.Front;
	if (InputManager::GetKey(Input::Key::A))
		camera.Position -= glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;
	if (InputManager::GetKey(Input::Key::D))
		camera.Position += glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;

	float xpos = (float)InputManager::GetMouseX();
	float ypos = (float)InputManager::GetMouseY();

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void SceneInstance::DrawLightCubes()
{
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - checking lightShader");
#endif

	// Check if lightShader is valid (asset loading might have failed on Android)
	if (!lightShader) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_WARN, "GAM300", "DrawLightCubes() - lightShader is null, skipping");
#endif
		return;
	}

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - lightShader is valid");
#endif

	// Get light positions from LightManager instead of renderSystem
	LightManager& lightManager = LightManager::getInstance();
	const auto& pointLights = lightManager.getPointLights();

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - about to loop through %zu lights", pointLights.size());
#endif

	// Draw light cubes at point light positions
	for (size_t i = 0; i < pointLights.size() && i < 4; i++) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "DrawLightCubes() - processing light %zu", i);
#endif
		lightShader->Activate();

		// Set up matrices for light cube
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, pointLights[i].position);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f)); // Make them smaller

		// Set up view and projection matrices
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 projection = glm::perspective(
			glm::radians(camera.Zoom),
			//(float)WindowManager::GetWindowWidth() / (float)WindowManager::GetWindowHeight(),
			(float)RunTimeVar::window.width / (float)RunTimeVar::window.height,
			0.1f, 100.0f
		);

		lightShader->setMat4("model", lightModel);
		lightShader->setMat4("view", view);
		lightShader->setMat4("projection", projection);
		//lightShader->setVec3("lightColor", pointLights[i].diffuse); // Use light color

		lightCubeMesh->Draw(*lightShader, camera);
	}
}

void SceneInstance::DrawLightCubes(const Camera& cameraOverride)
{
	// Get light positions from LightManager instead of renderSystem
	LightManager& lightManager = LightManager::getInstance();
	const auto& pointLights = lightManager.getPointLights();

	// Draw light cubes at point light positions
	for (size_t i = 0; i < pointLights.size() && i < 4; i++) {
		lightShader->Activate();

		// Set up matrices for light cube
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, pointLights[i].position);
		lightModel = glm::scale(lightModel, glm::vec3(0.2f)); // Make them smaller

		// Set up view and projection matrices using the override camera
		glm::mat4 view = cameraOverride.GetViewMatrix();
		glm::mat4 projection = glm::perspective(
			glm::radians(cameraOverride.Zoom),
			//(float)WindowManager::GetWindowWidth() / (float)WindowManager::GetWindowHeight(),
			(float)RunTimeVar::window.width / (float)RunTimeVar::window.height,
			0.1f, 100.0f
		);

		lightShader->setMat4("model", lightModel);
		lightShader->setMat4("view", view);
		lightShader->setMat4("projection", projection);
		//lightShader->setVec3("lightColor", pointLights[i].diffuse); // Use light color

		lightCubeMesh->Draw(*lightShader, cameraOverride);
	}
}