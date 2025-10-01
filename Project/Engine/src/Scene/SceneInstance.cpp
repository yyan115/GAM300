#include "pch.h"
#include <Scene/SceneInstance.hpp>
#include <Input/InputManager.hpp>
#include <Input/Keys.h>
#include <WindowManager.hpp>
#include <cmath>
#include <ECS/ECSRegistry.hpp>
#include <Asset Manager/AssetManager.hpp>
#include "TimeManager.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <Transform/TransformComponent.hpp>
#include <Graphics/TextRendering/TextUtils.hpp>
#include "ECS/NameComponent.hpp"
#include <Graphics/Lights/LightComponent.hpp>
#include "Serialization/Serializer.hpp"
#include "Sound/AudioComponent.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <Logging.hpp>

Entity fpsText;

void SceneInstance::Initialize() {
	// Initialize GraphicsManager first
	GraphicsManager& gfxManager = GraphicsManager::GetInstance();
	//gfxManager.Initialize(WindowManager::GetWindowWidth(), WindowManager::GetWindowHeight());
	gfxManager.Initialize(RunTimeVar::window.width, RunTimeVar::window.height);
	// WOON LI TEST CODE
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetECSManager(scenePath);

	if (scenePath == "Resources/Scenes/FakeScene.scene") {
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

		// SPRITE
		Entity sprite = ecsManager.CreateEntity();
		NameComponent& spriteName = ecsManager.GetComponent<NameComponent>(sprite);
		spriteName.name = "sprite_test";
		// Load resources first
		auto spriteTexture = ResourceManager::GetInstance().GetResource<Texture>("Resources/Textures/awesomeface.png");
		auto spriteShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("sprite")); 
		// Add component with constructor parameters
		ecsManager.AddComponent<SpriteRenderComponent>(sprite, SpriteRenderComponent{ spriteTexture, spriteShader });
		// Get reference and configure
		auto& spriteComponent = ecsManager.GetComponent<SpriteRenderComponent>(sprite); 
		spriteComponent.is3D = false;  // 2D screen space
		spriteComponent.position = glm::vec3(25.0f, 700.0f, 0.0f);  // Screen coordinates (pixels)
		spriteComponent.scale = glm::vec3(200.0f, 200.0f, 1.0f);
		spriteComponent.isVisible = true;

		// With billboard effect
		Entity sprite3D = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(sprite3D, { 2.0f, 1.0f, 0.0f });  // World coordinates
		ecsManager.transformSystem->SetLocalScale(sprite3D, { 1.0f, 1.0f, 1.0f });
		ecsManager.transformSystem->SetLocalRotation(sprite3D, { 0, 0, 0 });
		NameComponent& spriteName3D = ecsManager.GetComponent<NameComponent>(sprite3D);
		spriteName3D.name = "sprite_3d_test";
		auto spriteTexture3D = ResourceManager::GetInstance().GetResource<Texture>("Resources/Textures/awesomeface.jpg");
		auto spriteShader3D = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("sprite"));
		ecsManager.AddComponent<SpriteRenderComponent>(sprite3D, SpriteRenderComponent{ spriteTexture, spriteShader });
		auto& spriteComponent3D = ecsManager.GetComponent<SpriteRenderComponent>(sprite3D);
		spriteComponent3D.is3D = true;
		spriteComponent3D.scale = glm::vec3(0.5f, 0.5f, 0.5f);  // World units, not pixels
		spriteComponent3D.isVisible = true;
	
		// Without billboard effect
		Entity sprite3DFlat = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(sprite3D, { -2.0f, 1.0f, 0.0f });  // World coordinates
		ecsManager.transformSystem->SetLocalScale(sprite3D, { 1.0f, 1.0f, 1.0f });
		ecsManager.transformSystem->SetLocalRotation(sprite3D, { 0, 0, 0 });
		NameComponent& spriteName3DFlat = ecsManager.GetComponent<NameComponent>(sprite3DFlat);
		spriteName3D.name = "sprite_3d_flat_test";
		ecsManager.AddComponent<SpriteRenderComponent>(sprite3DFlat, SpriteRenderComponent{ spriteTexture, spriteShader });
		auto& spriteComponent3DFlat = ecsManager.GetComponent<SpriteRenderComponent>(sprite3DFlat);
		spriteComponent3DFlat.is3D = true;
		spriteComponent3DFlat.scale = glm::vec3(0.5f, 0.5f, 0.5f);  // World units, not pixels
		spriteComponent3DFlat.isVisible = true;
		spriteComponent3DFlat.enableBillboard = false;
	
		// Initialize lighting system and create light entities
		if (ecsManager.lightingSystem) 
		{
			ecsManager.lightingSystem->Initialise();

			// Create a directional light (sun)
			Entity sunLight = ecsManager.CreateEntity();
			NameComponent& sunName = ecsManager.GetComponent<NameComponent>(sunLight);
			sunName.name = "Sun";
			ecsManager.AddComponent<Transform>(sunLight, Transform{});

			DirectionalLightComponent sunLightComp;
			sunLightComp.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
			sunLightComp.ambient = glm::vec3(0.05f);
			sunLightComp.diffuse = glm::vec3(0.4f);
			sunLightComp.specular = glm::vec3(0.5f);
			sunLightComp.enabled = true;
			ecsManager.AddComponent<DirectionalLightComponent>(sunLight, sunLightComp);
			ecsManager.lightingSystem->RegisterEntity(sunLight);

			// Create point lights
			std::vector<Vector3D> pointLightPositions = {
				Vector3D(0.7f,  0.2f,  2.0f),
				Vector3D(2.3f, -3.3f, -4.0f),
				Vector3D(-4.0f,  2.0f, -12.0f),
				Vector3D(0.0f,  0.0f, -3.0f)
			};

			for (size_t i = 0; i < pointLightPositions.size(); i++) 
			{
				Entity pointLight = ecsManager.CreateEntity();
				NameComponent& pointLightName = ecsManager.GetComponent<NameComponent>(pointLight);
				pointLightName.name = "Point Light " + std::to_string(i);
				ecsManager.transformSystem->SetLocalPosition(pointLight, pointLightPositions[i]);
				ecsManager.transformSystem->SetLocalScale(pointLight, { .01f, .01f, .01f });
				// ecsManager.transformSystem->SetLocalRotation(pointLight, {}); // IF NEEDED
				
				// Test Model
				ecsManager.AddComponent<ModelRenderComponent>(pointLight, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile("Resources/Models/FinalBaseMesh.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")) });

				PointLightComponent pointLightComp;
				pointLightComp.ambient = glm::vec3(0.05f);
				pointLightComp.diffuse = glm::vec3(0.8f);
				pointLightComp.specular = glm::vec3(1.0f);
				pointLightComp.constant = 1.0f;
				pointLightComp.linear = 0.09f;
				pointLightComp.quadratic = 0.032f;
				pointLightComp.enabled = true;
				ecsManager.AddComponent<PointLightComponent>(pointLight, pointLightComp);
				ecsManager.lightingSystem->RegisterEntity(pointLight); 
				
			}

			// Create a spot light that follows the camera
			Entity spotLight = ecsManager.CreateEntity();
			NameComponent& spotLightName = ecsManager.GetComponent<NameComponent>(spotLight);
			spotLightName.name = "Flashlight";
			ecsManager.transformSystem->SetLocalPosition(spotLight, Vector3D{ 0.f, 0.f, 3.f});
			//ecsManager.transformSystem->SetLocalScale(pointLight, { .01f, .01f, .01f }); // IF NEEDED
			// ecsManager.transformSystem->SetLocalRotation(pointLight, {}); // IF NEEDED

			SpotLightComponent spotLightComp;
			spotLightComp.direction = camera.Front;
			spotLightComp.ambient = glm::vec3(0.0f);
			spotLightComp.diffuse = glm::vec3(1.0f);
			spotLightComp.specular = glm::vec3(1.0f);
			spotLightComp.constant = 1.0f;
			spotLightComp.linear = 0.09f;
			spotLightComp.quadratic = 0.032f;
			spotLightComp.cutOff = 0.976f;
			spotLightComp.outerCutOff = 0.966f;
			spotLightComp.enabled = true;
			ecsManager.AddComponent<SpotLightComponent>(spotLight, spotLightComp);
			ecsManager.lightingSystem->RegisterEntity(spotLight);
		}
		ENGINE_PRINT("[Scene] Lighting system entity count: ", ecsManager.lightingSystem->entities.size(), "\n");

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

		// Test Audio
		// Create an entity with AudioComponent
		Entity audioEntity = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(audioEntity, { 0, 0, 0 });
		NameComponent& audioName = ecsManager.GetComponent<NameComponent>(audioEntity);
		audioName.name = "Audio Test Entity";
				
		// Add AudioComponent
		AudioComponent audioComp;
		audioComp.Clip = "Resources/Audio/sfx/start menu bgm.ogg";
		audioComp.Volume = 0.3f;
		audioComp.Loop = true;
		audioComp.PlayOnAwake = true;
		audioComp.Spatialize = false;
		ecsManager.AddComponent<AudioComponent>(audioEntity, audioComp);

		// Add FPS text (mainly for android to see FPS)
		fpsText = ecsManager.CreateEntity();
		ecsManager.GetComponent<NameComponent>(fpsText).name = "FPSText";
		ecsManager.AddComponent<TextRenderComponent>(fpsText, TextRenderComponent{ "FPS PLACEHOLDER", 30, MetaFilesManager::GetGUID128FromAssetFile("Resources/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
		TextRenderComponent& fpsTextComp = ecsManager.GetComponent<TextRenderComponent>(fpsText);
		TextUtils::SetPosition(fpsTextComp, Vector3D(0, 0, 0));
		TextUtils::SetAlignment(fpsTextComp, TextRenderComponent::Alignment::LEFT);
	}

	// Creates light
	lightShader = std::make_shared<Shader>();
	lightShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("light"));
	//lightShader->LoadAsset("Resources/Shaders/light");
	std::vector<std::shared_ptr<Texture>> emptyTextures = {};
	lightCubeMesh = std::make_shared<Mesh>(lightVertices, lightIndices, emptyTextures);

	// Sets camera
	gfxManager.SetCamera(&camera);

	// Initialize systems.
	ecsManager.transformSystem->Initialise();
	ecsManager.modelSystem->Initialise();
	ecsManager.debugDrawSystem->Initialise();
	ecsManager.textSystem->Initialise();
	ecsManager.spriteSystem->Initialise();

	ENGINE_PRINT("Scene Initialized\n");
}

void SceneInstance::Update(double dt) {
	dt;

	// Update logic for the test scene
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	TextRenderComponent& fpsTextComponent = mainECS.GetComponent<TextRenderComponent>(fpsText);
	//fpsTextComponent.text = TimeManager::GetFps();
	TextUtils::SetText(fpsTextComponent, std::to_string(TimeManager::GetFps()));

	processInput((float)TimeManager::GetDeltaTime());

	// Update systems.
	mainECS.transformSystem->Update();
	mainECS.lightingSystem->Update();

	// Update audio (handles AudioManager FMOD update + AudioComponent updates)
	if (mainECS.audioSystem) {
		mainECS.audioSystem->Update((float)dt);
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

	if (mainECS.spriteSystem) {
		mainECS.spriteSystem->Update();
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

	// MADE IT so that you must drag to look around
	// 
	// Only process mouse look when left mouse button is held down
	if (InputManager::GetMouseButton(Input::MouseButton::LEFT))
	{
		float xpos = (float)InputManager::GetMouseX();
		float ypos = (float)InputManager::GetMouseY();

		// Reset firstMouse when starting a new mouse drag (fixes touch jump on Android)
		// Check for large jumps in mouse position which indicates a new touch
		if (InputManager::GetMouseButtonDown(Input::MouseButton::LEFT) || firstMouse)
		{
			firstMouse = false;
			lastX = xpos;
			lastY = ypos;
			return; // Skip this frame to prevent jump
		}

		// Also check for large position jumps (indicates finger lifted and touched elsewhere) - fixed jumps on desktop also as a side effect (intended to be ifdef)
		float positionJump = sqrt((xpos - lastX) * (xpos - lastX) + (ypos - lastY) * (ypos - lastY));
		if (positionJump > 100.0f) // Threshold for detecting new touch
		{
			lastX = xpos;
			lastY = ypos;
			return; // Skip this frame to prevent jump
		}

		float xoffset = xpos - lastX;
		float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

		lastX = xpos;
		lastY = ypos;

		camera.ProcessMouseMovement(xoffset, yoffset);
	}
	else
	{
		// When mouse button is released, reset for next touch
		firstMouse = true;
	}
}
