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
#include <Physics/PhysicsSystem.hpp>
#include <Physics/ColliderComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include "Serialization/Serializer.hpp"
#include "Sound/AudioComponent.hpp"
#include "Graphics/Particle/ParticleComponent.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
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

	// Add FPS text (mainly for android to see FPS)
	// fpsText = ecsManager.CreateEntity();
	// ecsManager.GetComponent<NameComponent>(fpsText).name = "FPSText";
	// ecsManager.AddComponent<TextRenderComponent>(fpsText, TextRenderComponent{ "FPS PLACEHOLDER", 30, MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
	// TextRenderComponent& fpsTextComp = ecsManager.GetComponent<TextRenderComponent>(fpsText);
	// TextUtils::SetPosition(fpsTextComp, Vector3D(400, 500, 0));
	// TextUtils::SetAlignment(fpsTextComp, TextRenderComponent::Alignment::LEFT);
	
	// Test Camera Component
	Entity testCamera = ecsManager.CreateEntity();
	ecsManager.GetComponent<NameComponent>(testCamera).name = "main camera";
	ecsManager.transformSystem->SetLocalPosition(testCamera, {0, 0, 3});
	// Add camera component
	CameraComponent camComp;
	camComp.nearPlane = 0.1f;
	camComp.farPlane = 100.f;
	camComp.isActive = true;
	camComp.priority = 0;
	camComp.useFreeRotation = true;
	camComp.yaw = -90.0f;
	camComp.pitch = 0.0f;
	camComp.fov = 45.0f;
	ecsManager.AddComponent<CameraComponent>(testCamera, camComp);
	// Initialize camera system
	ecsManager.cameraSystem->Initialise();
	// Sets camera
	gfxManager.SetCamera(ecsManager.cameraSystem->GetActiveCamera());
	ENGINE_PRINT("[TEST] Camera entity created: ", testCamera, "\n");
	ENGINE_PRINT("[TEST] Active camera entity: ", ecsManager.cameraSystem->GetActiveCameraEntity(), "\n");

	// Initialize systems.
	ecsManager.transformSystem->Initialise();
	ENGINE_LOG_INFO("Transform system initialized");
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
	InitializePhysics();

	ENGINE_PRINT("Scene Initialized\n");
}

void SceneInstance::InitializeJoltPhysics() {
	auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	ecsManager.physicsSystem->InitialiseJolt();
}

void SceneInstance::InitializePhysics() {
	auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	ecsManager.physicsSystem->Initialise(ecsManager);
	ENGINE_LOG_INFO("Physics system initialized");
}

void SceneInstance::Update(double dt) {
	dt;

	// Update logic for the test scene
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	//TextRenderComponent& fpsTextComponent = mainECS.GetComponent<TextRenderComponent>(fpsText);
	//fpsTextComponent.text = std::to_string(TimeManager::GetFps());
	//TextUtils::SetText(fpsTextComponent, std::to_string(TimeManager::GetFps()));

	processInput((float)TimeManager::GetDeltaTime());

	// Update systems.
	mainECS.physicsSystem->Update((float)TimeManager::GetDeltaTime());
	mainECS.physicsSystem->physicsSyncBack(mainECS);
	mainECS.transformSystem->Update();
	mainECS.cameraSystem->Update();
	mainECS.lightingSystem->Update();

	// Update audio (handles AudioManager FMOD update + AudioComponent updates)
	if (mainECS.audioSystem) {
		mainECS.audioSystem->Update((float)dt);
	}
}

void SceneInstance::Draw() {
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	GraphicsManager& gfxManager = GraphicsManager::GetInstance();

	// Set to false so game view shows ALL sprites (not filtered by 2D/3D mode)
	gfxManager.SetRenderingForEditor(false);

	//RenderSystem::getInstance().BeginFrame();
	gfxManager.BeginFrame();
	gfxManager.Clear();

	//glm::mat4 transform = glm::mat4(1.0f);
	//transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.0f));
	//transform = glm::scale(transform, glm::vec3(0.1f, 0.1f, 0.1f));
	//RenderSystem::getInstance().Submit(backpackModel, transform, shader);

	gfxManager.SetCamera(mainECS.cameraSystem->GetActiveCamera());

	// Update frustum with the game camera BEFORE model system runs (for proper culling in game panel)
	gfxManager.UpdateFrustum();

	if (mainECS.modelSystem)
	{
		mainECS.modelSystem->Update();
	}
	if (mainECS.textSystem)
	{
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call textSystem->Update()");
//#endif
		mainECS.textSystem->Update();
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "textSystem->Update() completed");
//#endif
	}

//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call spriteSystem->Update()");
//#endif
	if (mainECS.spriteSystem) {
		mainECS.spriteSystem->Update();
	}
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "spriteSystem->Update() completed");
//#endif
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call particleSystem->Update()");
//#endif
	if (mainECS.particleSystem)
	{
		mainECS.particleSystem->Update();
	}
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "particleSystem->Update() completed");
//#endif
	// Test debug drawing
	//DebugDrawSystem::DrawCube(Vector3D(0, 1, 0), Vector3D(1, 1, 1), Vector3D(1, 0, 0)); // Red cube above origin
	//DebugDrawSystem::DrawSphere(Vector3D(2, 0, 0), 1.0f, Vector3D(0, 1, 0)); // Green sphere to the right
	//DebugDrawSystem::DrawLine(Vector3D(0, 0, 0), Vector3D(3, 3, 3), Vector3D(0, 0, 1)); // Blue line diagonal
	//auto backpackModel = ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj");
	//DebugDrawSystem::DrawMeshWireframe(backpackModel, Vector3D(-2, 0, 0), Vector3D(1, 1, 0), 0.0f);

	// Update debug draw system to submit to graphics manager
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call debugDrawSystem->Update()");
//#endif
	if (mainECS.debugDrawSystem)
	{
		mainECS.debugDrawSystem->Update();
	}
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "debugDrawSystem->Update() completed");
//#endif
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

}

void SceneInstance::Exit() {
	// Exit systems.
	//ECSRegistry::GetInstance().GetECSManager(scenePath).modelSystem->Exit();
	//ECSRegistry::GetInstance().GetActiveECSManager().physicsSystem->Shutdown();
	ShutDownPhysics();
	ECSRegistry::GetInstance().GetECSManager(scenePath).particleSystem->Shutdown();
	ENGINE_PRINT("TestScene Exited\n");
}

void SceneInstance::ShutDownPhysics() {
	ECSRegistry::GetInstance().GetECSManager(scenePath).physicsSystem->Shutdown();
}

void SceneInstance::processInput(float deltaTime)
{
	if (InputManager::GetKeyDown(Input::Key::ESC))
		WindowManager::SetWindowShouldClose();

	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);
	Entity activeCam = mainECS.cameraSystem->GetActiveCameraEntity();

	if (activeCam == 0) return;

	auto& camComp = mainECS.GetComponent<CameraComponent>(activeCam);

	// Only process input if using free rotation (gameplay camera)
	if (!camComp.useFreeRotation) return;

	Camera* camera = mainECS.cameraSystem->GetActiveCamera();

	float cameraSpeed = 2.5f * deltaTime;
	if (InputManager::GetKey(Input::Key::W))
		camera->ProcessKeyboard(FORWARD, deltaTime);
	if (InputManager::GetKey(Input::Key::S))
		camera->ProcessKeyboard(BACKWARD, deltaTime);
	if (InputManager::GetKey(Input::Key::A))
		camera->ProcessKeyboard(LEFT, deltaTime);
	if (InputManager::GetKey(Input::Key::D))
		camera->ProcessKeyboard(RIGHT, deltaTime);

	// Zoom with keys (N to zoom out, M to zoom in)
	if (InputManager::GetKey(Input::Key::N))
		mainECS.cameraSystem->ZoomCamera(activeCam, deltaTime);  // Zoom out
	if (InputManager::GetKey(Input::Key::M))
		mainECS.cameraSystem->ZoomCamera(activeCam, -deltaTime); // Zoom in

	// Test camera shake with Spacebar
	if (InputManager::GetKeyDown(Input::Key::SPACE))
		mainECS.cameraSystem->ShakeCamera(activeCam, 0.3f, 0.5f); // intensity=0.3, duration=0.5

	// MADE IT so that you must drag to look around
	// 
	// Only process mouse look when left mouse button is held down
	if (InputManager::GetMouseButton(Input::MouseButton::LEFT))
	{
		static float lastX = 0.0f;
		static float lastY = 0.0f;
		static bool firstMouse = true;

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

		camera->ProcessMouseMovement(xoffset, yoffset);
	}
	else
	{
		// When mouse button is released, reset for next touch
		static bool firstMouse = true;
		firstMouse = true;
	}

	// Sync camera position back to transform
	camComp.fov = camera->Zoom;
	Vector3D newPos(camera->Position.x, camera->Position.y, camera->Position.z);
	mainECS.transformSystem->SetLocalPosition(activeCam, newPos);
}
