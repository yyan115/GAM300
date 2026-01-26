#include "pch.h"
#include <Scene/SceneInstance.hpp>
#include <Input/InputManager.h>
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
#include <Physics/PhysicsSystem.hpp>
#include <Physics/ColliderComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include <Physics/Kinematics/CharacterControllerSystem.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include "Serialization/Serializer.hpp"
#include "Sound/AudioComponent.hpp"
#include "Graphics/Particle/ParticleComponent.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"
#ifdef ANDROID
#include <android/log.h>
#endif
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <Logging.hpp>
#include "Graphics/PostProcessing/PostProcessingManager.hpp"

#include <Animation/AnimationComponent.hpp>
#include <Animation/AnimationSystem.hpp>
#include <UI/Button/ButtonComponent.hpp>
#include <UI/Slider/SliderComponent.hpp>
#include <Multi-threading/SequentialSystemOrchestrator.hpp>
#include <Multi-threading/ParallelSystemOrchestrator.hpp>

Entity fpsText;

void testing(ECSManager&);
//SceneInstance::SceneInstance() {
//	if (!multithreadSystems)
//	{
//		systemOrchestrator = std::make_unique<SequentialSystemOrchestrator>();
//	}
//	else
//	{
//		systemOrchestrator = std::make_unique<ParallelSystemOrchestrator>();
//	}
//}
//
//SceneInstance::SceneInstance(const std::string& path) : IScene(path) {
//	if (!multithreadSystems)
//	{
//		systemOrchestrator = std::make_unique<SequentialSystemOrchestrator>();
//	}
//	else
//	{
//		systemOrchestrator = std::make_unique<ParallelSystemOrchestrator>();
//	}
//}

void SceneInstance::Initialize()
{
	// Initialize GraphicsManager first
	GraphicsManager& gfxManager = GraphicsManager::GetInstance();
	// gfxManager.Initialize(WindowManager::GetWindowWidth(), WindowManager::GetWindowHeight());
	gfxManager.Initialize(RunTimeVar::window.width, RunTimeVar::window.height);
	// Get the ECS manager for this scene
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetECSManager(scenePath);

	if (!PostProcessingManager::GetInstance().Initialize())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Engine] Failed to initialize Post-Processing!\n");
	}
	ENGINE_PRINT("[Engine] Post-processing initialized with HDR\n");

	// Configure HDR settings
	auto* hdrEffect = PostProcessingManager::GetInstance().GetHDREffect();
	if (hdrEffect)
	{
		hdrEffect->SetEnabled(true);
		hdrEffect->SetExposure(1.f);
		hdrEffect->SetGamma(2.2f);
		hdrEffect->SetToneMappingMode(HDREffect::ToneMappingMode::REINHARD);
		ENGINE_PRINT("[SceneInstance] HDR initialized and enabled\n");
	}

	// CreateHDRTestScene(ecsManager); // Commented out - only use for HDR testing
	
	// Any test code
	//testing(ecsManager);


	// Initialize systems.
	ecsManager.transformSystem->Initialise();
	ENGINE_LOG_INFO("Transform system initialized");
	// Initialize camera system
	ecsManager.cameraSystem->Initialise();
	// Set camera if one exists in the scene
	Entity activeCam = ecsManager.cameraSystem->GetActiveCameraEntity();
	if (activeCam != UINT32_MAX && ecsManager.cameraSystem->GetActiveCamera())
	{
		gfxManager.SetCamera(ecsManager.cameraSystem->GetActiveCamera());
		ENGINE_LOG_INFO("[SceneInstance] Camera set to GraphicsManager, active camera entity: " + std::to_string(activeCam));
	}
	else
	{
		ENGINE_LOG_WARN("[SceneInstance] No active camera found! Game panel will not render. Please add a camera to your scene.");
	}
	ecsManager.modelSystem->Initialise();
	ENGINE_LOG_INFO("Model system initialized");
	ecsManager.debugDrawSystem->Initialise();
	ENGINE_LOG_INFO("Debug system initialized");
	ecsManager.textSystem->Initialise();
	ENGINE_LOG_INFO("Text system initialized");
	ecsManager.spriteSystem->Initialise();
	ENGINE_LOG_INFO("Sprite system initialized");
	ecsManager.particleSystem->Initialise();
	ENGINE_LOG_INFO("Particle system initialized");
	ecsManager.animationSystem->Initialise();
	ENGINE_LOG_INFO("Animation system initialized");
	InitializePhysics(); // can we all do like this?
	ecsManager.scriptSystem->Initialise(ecsManager);
	ENGINE_LOG_INFO("Script system initialized");
	ecsManager.spriteAnimationSystem->Initialise();
	ENGINE_LOG_INFO("Sprite Animation system initialized");
	ecsManager.uiAnchorSystem->Initialise(ecsManager);
	ENGINE_LOG_INFO("UI Anchor system initialized");
	ecsManager.buttonSystem->Initialise(ecsManager);
	ENGINE_LOG_INFO("Button system initialized");
	ecsManager.sliderSystem->Initialise(ecsManager);
	ENGINE_LOG_INFO("Slider system initialized");

	if (!multithreadSystems)
	{
		systemOrchestrator = std::make_unique<SequentialSystemOrchestrator>();
	}
	else
	{
		systemOrchestrator = std::make_unique<ParallelSystemOrchestrator>();
	}

	ENGINE_PRINT("Scene Initialized\n");
}

void SceneInstance::InitializeJoltPhysics()
{
	std::cout<<"=== InitializeJoltPhysics START ===";
	auto &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	ecsManager.physicsSystem->InitialiseJolt();
	ENGINE_LOG_INFO("=== InitializeJoltPhysics END ===");
}

void SceneInstance::InitializePhysics()
{
	auto &ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	ecsManager.physicsSystem->Initialise(ecsManager);
	ENGINE_LOG_INFO("Physics system initialized");
}

void SceneInstance::Update(double dt)
{
	dt;

	// Update logic for the test scene
	ECSManager &mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	// TextRenderComponent& fpsTextComponent = mainECS.GetComponent<TextRenderComponent>(fpsText);
	// fpsTextComponent.text = std::to_string(TimeManager::GetFps());
	// TextUtils::SetText(fpsTextComponent, std::to_string(TimeManager::GetFps()));

	processInput((float)TimeManager::GetDeltaTime());

	updateSynchronized = false;
	systemOrchestrator->Update();
	updateSynchronized = true;
}

void SceneInstance::Draw()
{

	ECSManager &mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	GraphicsManager &gfxManager = GraphicsManager::GetInstance();

	// Set to false so game view shows ALL sprites (not filtered by 2D/3D mode)
	gfxManager.SetRenderingForEditor(false);

	// Update viewport dimensions to match window size
	WindowManager::SetViewportDimensions(RunTimeVar::window.width, RunTimeVar::window.height);
	gfxManager.SetViewportSize(RunTimeVar::window.width, RunTimeVar::window.height);

	// Begin HDR rendering to floating-point framebuffer (this also clears the buffer)
	PostProcessingManager::GetInstance().BeginHDRRender(RunTimeVar::window.width, RunTimeVar::window.height);
	gfxManager.BeginFrame();

	Entity activeCam = mainECS.cameraSystem ? mainECS.cameraSystem->GetActiveCameraEntity() : UINT32_MAX;
	if (activeCam != UINT32_MAX && mainECS.HasComponent<CameraComponent>(activeCam))
	{
		auto &camComp = mainECS.GetComponent<CameraComponent>(activeCam);
		gfxManager.Clear(camComp.backgroundColor.r, camComp.backgroundColor.g, camComp.backgroundColor.b, 1.0f);
	}
	else
	{
		gfxManager.Clear(0.192f, 0.301f, 0.475f, 1.0f);
	}

	// Update transforms before camera (camera needs up-to-date transform matrices)
	mainECS.transformSystem->Update();

	// Update camera system (detects camera changes, switches cameras, updates from components)
	// This needs to run even in edit mode so the game panel shows the correct camera
	mainECS.cameraSystem->Update();

	gfxManager.SetCamera(mainECS.cameraSystem->GetActiveCamera());

	// Update frustum with the game camera BEFORE model system runs (for proper culling in game panel)
	gfxManager.UpdateFrustum();

	drawSynchronized = false;
	systemOrchestrator->Draw();
	drawSynchronized = true;

// #ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "debugDrawSystem->Update() completed");
// #endif
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

	// End HDR rendering and apply tone-mapping to default framebuffer (screen)
	PostProcessingManager::GetInstance().EndHDRRender(0, RunTimeVar::window.width, RunTimeVar::window.height);
}

void SceneInstance::Exit()
{
	// Exit systems.
	// ECSRegistry::GetInstance().GetECSManager(scenePath).modelSystem->Exit();
	// ECSRegistry::GetInstance().GetActiveECSManager().physicsSystem->Shutdown();
	ECSRegistry::GetInstance().GetECSManager(scenePath).characterControllerSystem->Shutdown();
	ShutDownPhysics();
	PostProcessingManager::GetInstance().Shutdown();
	ECSRegistry::GetInstance().GetECSManager(scenePath).particleSystem->Shutdown();
	ECSRegistry::GetInstance().GetECSManager(scenePath).scriptSystem->Shutdown();
	systemOrchestrator.reset();
	ENGINE_PRINT("TestScene Exited\n");
}

void SceneInstance::ShutDownPhysics()
{
	ECSRegistry::GetInstance().GetECSManager(scenePath).physicsSystem->Shutdown();
}

void SceneInstance::processInput(float deltaTime)
{
	// ESC handling is now done in Lua (camera_follow.lua) for cursor lock toggle
	// Game-specific pause menus should also be handled in Lua

	ECSManager &mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);
	Entity activeCam = mainECS.cameraSystem->GetActiveCameraEntity();

	if (activeCam == UINT32_MAX)
		return;

	auto &camComp = mainECS.GetComponent<CameraComponent>(activeCam);

	// Only process input if using free rotation (gameplay camera)
	if (!camComp.useFreeRotation)
		return;

	Camera *camera = mainECS.cameraSystem->GetActiveCamera();

	// float cameraSpeed = 2.5f * deltaTime;
	// if (InputManager::GetKey(Input::Key::W))
	// if (InputManager::GetKey(Input::Key::S))
	// if (InputManager::GetKey(Input::Key::A))
	// if (InputManager::GetKey(Input::Key::D))

	// temp
	Entity player{};
	const auto &all = mainECS.GetAllEntities();
	for (Entity e : all)
	{
		std::string enttName = mainECS.GetComponent<NameComponent>(e).name;
		if (enttName == "Kachujin")
		{
			player = e;
		}
	}

	// Temp player controls for playable level
	//  Backwards = +z

	/*if (InputManager::GetKey(Input::Key::W))
	{
		ENGINE_LOG_DEBUG("[ProcessInput] W Key Pressed");
		Transform playerPos = mainECS.GetComponent<Transform>(player);
		mainECS.transformSystem->SetLocalPosition(player, Vector3D(playerPos.localPosition.x, playerPos.localPosition.y, playerPos.localPosition.z - 0.01f));
		mainECS.transformSystem->SetLocalRotation(player, Vector3D(0, 180, 0));
		camera->ProcessKeyboard(FORWARD, 0.004f);
	}
	if (InputManager::GetKey(Input::Key::S))
	{
		ENGINE_LOG_DEBUG("[ProcessInput] S Key Pressed");
		Transform playerPos = mainECS.GetComponent<Transform>(player);
		mainECS.transformSystem->SetLocalPosition(player, Vector3D(playerPos.localPosition.x, playerPos.localPosition.y, playerPos.localPosition.z + 0.01f));
		mainECS.transformSystem->SetLocalRotation(player, Vector3D(0, 0, 0));
		camera->ProcessKeyboard(BACKWARD, 0.004f);
	}
	if (InputManager::GetKey(Input::Key::A))
	{
		ENGINE_LOG_DEBUG("[ProcessInput] A Key Pressed");
		Transform playerPos = mainECS.GetComponent<Transform>(player);
		mainECS.transformSystem->SetLocalPosition(player, Vector3D(playerPos.localPosition.x - 0.01f, playerPos.localPosition.y, playerPos.localPosition.z));
		mainECS.transformSystem->SetLocalRotation(player, Vector3D(0, -90, 0));
		camera->ProcessKeyboard(LEFT, 0.004f);
	}
	if (InputManager::GetKey(Input::Key::D))
	{
		ENGINE_LOG_DEBUG("[ProcessInput] D Key Pressed");
		Transform playerPos = mainECS.GetComponent<Transform>(player);
		mainECS.transformSystem->SetLocalPosition(player, Vector3D(playerPos.localPosition.x + 0.01f, playerPos.localPosition.y, playerPos.localPosition.z));
		mainECS.transformSystem->SetLocalRotation(player, Vector3D(0, 90, 0));
		camera->ProcessKeyboard(RIGHT, 0.004f);
	}*/

	// TODO: Migrate debug camera controls to new input system action bindings
	// Zoom with keys (N to zoom out, M to zoom in)
	//if (InputManager::GetKey(Input::Key::N))
	//	mainECS.cameraSystem->ZoomCamera(activeCam, deltaTime); // Zoom out
	//if (InputManager::GetKey(Input::Key::M))
	//	mainECS.cameraSystem->ZoomCamera(activeCam, -deltaTime); // Zoom in

	// Test camera shake with Spacebar
	//if (InputManager::GetKeyDown(Input::Key::SPACE))
	//	mainECS.cameraSystem->ShakeCamera(activeCam, 0.3f, 0.5f); // intensity=0.3, duration=0.5

	//// MADE IT so that you must drag to look around
	////
	//// Only process mouse look when left mouse button is held down
	//static float lastX = 0.0f;
	//static float lastY = 0.0f;
	//static bool firstMouse = true;

	//if (InputManager::GetMouseButton(Input::MouseButton::LEFT))
	//{
	//	float xpos = (float)InputManager::GetMouseX();
	//	float ypos = (float)InputManager::GetMouseY();

	//	// Reset firstMouse when starting a new mouse drag (fixes touch jump on Android)
	//	// Check for large jumps in mouse position which indicates a new touch
	//	if (InputManager::GetMouseButtonDown(Input::MouseButton::LEFT) || firstMouse)
	//	{
	//		firstMouse = false;
	//		lastX = xpos;
	//		lastY = ypos;
	//		return; // Skip this frame to prevent jump
	//	}

	//	// Also check for large position jumps (indicates finger lifted and touched elsewhere) - fixed jumps on desktop also as a side effect (intended to be ifdef)
	//	float positionJump = sqrt((xpos - lastX) * (xpos - lastX) + (ypos - lastY) * (ypos - lastY));
	//	if (positionJump > 100.0f) // Threshold for detecting new touch
	//	{
	//		lastX = xpos;
	//		lastY = ypos;
	//		return; // Skip this frame to prevent jump
	//	}

	//	float xoffset = xpos - lastX;
	//	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	//	lastX = xpos;
	//	lastY = ypos;

	//	camera->ProcessMouseMovement(xoffset, yoffset);
	//}
	//else
	//{
	//	// When mouse button is released, reset for next touch
	//	firstMouse = true;
	//}

	// TODO: Migrate debug toggle to new input system action bindings
	//if (InputManager::GetKeyDown(Input::Key::H))
	//{
	//	auto *hdr = PostProcessingManager::GetInstance().GetHDREffect();
	//	hdr->SetEnabled(!hdr->IsEnabled());
	//	ENGINE_PRINT("[HDR] Toggled: ", hdr->IsEnabled(), "\n");
	//}
	// Sync camera state back to component and transform
	camComp.fov = camera->Zoom;
	camComp.yaw = camera->Yaw;
	camComp.pitch = camera->Pitch;
	Vector3D newPos(camera->Position.x, camera->Position.y, camera->Position.z);
	mainECS.transformSystem->SetLocalPosition(activeCam, newPos);
}

void SceneInstance::CreateHDRTestScene(ECSManager &ecsManager)
{
	ENGINE_PRINT("[HDR Test] Creating test scene with bright objects...\n");

	// Get your cube model GUID (adjust path to your actual cube model)
	std::string cubeModelPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/cube.obj";
	GUID_128 cubeModelGUID = MetaFilesManager::GetGUID128FromAssetFile(cubeModelPath);

	// Get shader GUID
	std::string shaderPath = ResourceManager::GetPlatformShaderPath("default");
	GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);

	// Create 5 cubes with increasing brightness
	for (int i = 0; i < 5; i++)
	{
		Entity cube = ecsManager.CreateEntity();

		// Set name
		ecsManager.GetComponent<NameComponent>(cube).name = "HDR Test Cube " + std::to_string(i);

		// Set transform - space them out horizontally
		float xPos = (i - 2) * 6.0f; // -4, -2, 0, 2, 4
		ecsManager.transformSystem->SetLocalPosition(cube, Vector3D(xPos, 0.0f, -5.0f));
		ecsManager.transformSystem->SetLocalScale(cube, Vector3D(0.5f, 0.5f, 0.5f));

		// Add model render component
		ModelRenderComponent modelComp(cubeModelGUID, shaderGUID, GUID_128{});
		ecsManager.AddComponent<ModelRenderComponent>(cube, modelComp);

		// Create material with increasing emission (this is the key for HDR testing!)
		auto material = std::make_shared<Material>("HDR Test Material " + std::to_string(i));

		// Calculate brightness - ranges from 1.0 to 9.0
		float brightness = 1.0f + i * 2.0f;

		// Set emissive color (makes it glow)
		material->SetEmissive(glm::vec3(brightness, brightness * 0.9f, brightness * 0.8f));

		// Set basic material properties
		material->SetDiffuse(glm::vec3(0.8f, 0.8f, 0.8f));
		material->SetSpecular(glm::vec3(0.5f, 0.5f, 0.5f));
		material->SetShininess(32.0f);

		// Apply material to the model
		ecsManager.GetComponent<ModelRenderComponent>(cube).SetMaterial(material);

		ENGINE_PRINT("[HDR Test] Created cube ", i, " at x=", xPos, " with brightness=", brightness, "\n");
	}

	// Add a very bright point light to really test HDR
	Entity brightLight = ecsManager.CreateEntity();
	ecsManager.GetComponent<NameComponent>(brightLight).name = "HDR Test Light";
	ecsManager.transformSystem->SetLocalPosition(brightLight, Vector3D(0.0f, 3.0f, -3.0f));

	// Use PointLightComponent instead of LightComponent
	PointLightComponent lightComp;
	lightComp.color = Vector3D(1.0f, 0.95f, 0.9f); // Warm white
	lightComp.intensity = 1.0f;					   // VERY bright for HDR testing!
	lightComp.constant = 1.0f;
	lightComp.linear = 0.09f;
	lightComp.quadratic = 0.032f;
	lightComp.ambient = Vector3D(0.1f, 0.1f, 0.1f);
	lightComp.diffuse = Vector3D(1.0f, 0.95f, 0.9f);
	lightComp.specular = Vector3D(1.0f, 1.0f, 1.0f);
	lightComp.enabled = true;

	ecsManager.AddComponent<PointLightComponent>(brightLight, lightComp);

	ENGINE_PRINT("[HDR Test] Test scene created successfully!\n");
	ENGINE_PRINT("[HDR Test] Expected result:\n");
	ENGINE_PRINT("[HDR Test] - WITHOUT HDR: All bright cubes would look similar (white)\n");
	ENGINE_PRINT("[HDR Test] - WITH HDR: Each cube should have distinct brightness levels\n");
}

void SceneInstance::CreateDefaultCamera(ECSManager &ecsManager)
{
	ENGINE_PRINT("[SceneInstance] Creating default main camera...\n");

	// Create camera entity
	Entity cameraEntity = ecsManager.CreateEntity();
	ecsManager.GetComponent<NameComponent>(cameraEntity).name = "Main Camera";

	// Set transform - position camera back a bit so it can see objects at origin
	ecsManager.transformSystem->SetLocalPosition(cameraEntity, Vector3D(0.0f, 0.0f, 5.0f));

	// Add camera component with default settings
	CameraComponent camComp;
	camComp.isActive = true;
	camComp.fov = 45.0f;
	camComp.nearPlane = 0.1f;
	camComp.farPlane = 1000.0f;
	camComp.movementSpeed = 2.5f;
	camComp.mouseSensitivity = 0.1f;
	camComp.yaw = -90.0f; // Looking forward (-Z)
	camComp.pitch = 0.0f; // Level horizon
	camComp.minZoom = 1.0f;
	camComp.maxZoom = 90.0f;

	ecsManager.AddComponent<CameraComponent>(cameraEntity, camComp);

	ENGINE_PRINT("[SceneInstance] Default camera created successfully (Entity ID: ", cameraEntity, ")\n");
}


void testing(ECSManager& ecsManager)
{
	// ===== TEXT WRAPPING TEST =====
	Entity textEntity = ecsManager.CreateEntity();
	ecsManager.AddComponent<NameComponent>(textEntity, NameComponent("Text Wrap Test"));

	GUID_128 fontGUID = MetaFilesManager::GetGUID128FromAssetFile("Resources/Fonts/Kenney Mini.ttf");
	GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text"));

	// Add component first
	TextRenderComponent textComp("This is a long piece of text that should wrap to multiple lines when word wrap is enabled.", 32, fontGUID, shaderGUID);
	textComp.position = Vector3D(100.0f, 500.0f, 0.0f);
	textComp.color = Vector3D(1.0f, 1.0f, 1.0f);
	ecsManager.AddComponent<TextRenderComponent>(textEntity, textComp);

	// Now get reference and set wrap properties
	auto& text = ecsManager.GetComponent<TextRenderComponent>(textEntity);
	text.wordWrap = true;
	text.maxWidth = 300.0f;
	text.lineSpacing = 1.2f;

	std::cout << "[Test] Set wordWrap: " << text.wordWrap << ", maxWidth: " << text.maxWidth << std::endl;

	Entity sAnim = ecsManager.CreateEntity();
	ecsManager.AddComponent<NameComponent>(sAnim, NameComponent("Sprite Animation Entity"));
	ecsManager.AddComponent<SpriteAnimationComponent>(sAnim, SpriteAnimationComponent());
	ecsManager.AddComponent<SpriteRenderComponent>(sAnim, SpriteRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile("Resources/Textures/idle_1.png"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default"))});

	std::string idlePath[3] = {
		"Resources/Textures/idle_1.png",
		"Resources/Textures/idle_2.png",
		"Resources/Textures/idle_3.png"
	};

	GUID_128 frame1 = MetaFilesManager::GetGUID128FromAssetFile(idlePath[0]);
	GUID_128 frame2 = MetaFilesManager::GetGUID128FromAssetFile(idlePath[1]);
	GUID_128 frame3 = MetaFilesManager::GetGUID128FromAssetFile(idlePath[2]);

	// Setting Current Sprite
	auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(sAnim);
	sprite.is3D = true;

	auto& anim = ecsManager.GetComponent<SpriteAnimationComponent>(sAnim);

	SpriteAnimationClip idleClip;
	idleClip.name = "Idle";
	idleClip.loop = true;

	for(int i = 0; i < 3; ++i) 
	{
		SpriteFrame frame;
		frame.textureGUID = MetaFilesManager::GetGUID128FromAssetFile(idlePath[i]);
		frame.texturePath = idlePath[i];
		frame.uvOffset = glm::vec2(0.0f, 0.0f);
		frame.uvScale = glm::vec2(1.0f, 1.0f);
		frame.duration = 1.0f; // 0.2 seconds per frame

		idleClip.frames.push_back(frame);
	}

	anim.clips.push_back(idleClip);
	anim.Play("Idle");
}