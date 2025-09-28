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
#include "Physics/RigidbodyComponent.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Math/Matrix4x4.hpp"

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
		// Create physics test objects (invisible boxes for collision testing)
		Entity box1 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(box1, { 0, 8.0f, 0 }); // Much higher to see it fall
		ecsManager.transformSystem->SetLocalScale(box1, { 1.0f, 1.0f, 1.0f });
		ecsManager.transformSystem->SetLocalRotation(box1, { 0, 0, 0 });
		NameComponent& box1Name = ecsManager.GetComponent<NameComponent>(box1);
		box1Name.name = "Falling Box";

		// Add physics components to box 1 (dynamic)
		RigidbodyComponent rigidbody1;
		rigidbody1.bodyType = BodyType::Dynamic;
		rigidbody1.mass = 1.0f;
		rigidbody1.restitution = 0.3f;
		rigidbody1.friction = 0.7f;
		rigidbody1.isGravityEnabled = true;
		ecsManager.AddComponent<RigidbodyComponent>(box1, rigidbody1);

		ColliderComponent collider1;
		collider1.type = ColliderType::Box;
		collider1.size = Vector3D(0.5f, 0.5f, 0.5f);
		ecsManager.AddComponent<ColliderComponent>(box1, collider1);

		Entity box2 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(box2, { 3.0f, 6.0f, 0 }); // Higher and to the side
		ecsManager.transformSystem->SetLocalScale(box2, { 1.0f, 1.0f, 1.0f });
		ecsManager.transformSystem->SetLocalRotation(box2, { 0, 0, 0 });
		NameComponent& box2Name = ecsManager.GetComponent<NameComponent>(box2);
		box2Name.name = "Kinematic Box";

		// Add physics components to box 2 (kinematic)
		RigidbodyComponent rigidbody2;
		rigidbody2.bodyType = BodyType::Kinematic;
		rigidbody2.mass = 2.0f;
		rigidbody2.restitution = 0.5f;
		rigidbody2.friction = 0.5f;
		rigidbody2.isGravityEnabled = false; // Kinematic bodies don't need gravity
		ecsManager.AddComponent<RigidbodyComponent>(box2, rigidbody2);

		ColliderComponent collider2;
		collider2.type = ColliderType::Box;
		collider2.size = Vector3D(0.6f, 0.6f, 0.6f);
		ecsManager.AddComponent<ColliderComponent>(box2, collider2);

		Entity box3 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(box3, { -3.0f, 4.0f, 0 }); // Higher and to the other side
		ecsManager.transformSystem->SetLocalScale(box3, { 1.0f, 1.0f, 1.0f });
		ecsManager.transformSystem->SetLocalRotation(box3, { 0, 0, 0 });
		NameComponent& box3Name = ecsManager.GetComponent<NameComponent>(box3);
		box3Name.name = "Static Box";

		// Add physics components to box 3 (static)
		RigidbodyComponent rigidbody3;
		rigidbody3.bodyType = BodyType::Static;
		rigidbody3.mass = 0.0f; // Static bodies don't need mass
		rigidbody3.restitution = 0.8f;
		rigidbody3.friction = 1.0f;
		rigidbody3.isGravityEnabled = false; // Static bodies don't move
		ecsManager.AddComponent<RigidbodyComponent>(box3, rigidbody3);

		ColliderComponent collider3;
		collider3.type = ColliderType::Box;
		collider3.size = Vector3D(1.0f, 1.0f, 1.0f); // Larger static collider
		ecsManager.AddComponent<ColliderComponent>(box3, collider3);

		// Create player entity with physics
		playerEntity = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(playerEntity, { 0, 2.0f, 0 }); // Start above ground
		ecsManager.transformSystem->SetLocalScale(playerEntity, { 0.5f, 1.8f, 0.5f }); // Human-like proportions
		ecsManager.transformSystem->SetLocalRotation(playerEntity, { 0, 0, 0 });
		NameComponent& playerName = ecsManager.GetComponent<NameComponent>(playerEntity);
		playerName.name = "Player";

		// Add physics components to player (dynamic capsule for character controller feel)
		RigidbodyComponent playerRigidbody;
		playerRigidbody.bodyType = BodyType::Dynamic;
		playerRigidbody.mass = 70.0f; // Average human weight
		playerRigidbody.restitution = 0.0f; // No bouncing for player
		playerRigidbody.friction = 0.8f; // Good traction
		playerRigidbody.isGravityEnabled = true;
		ecsManager.AddComponent<RigidbodyComponent>(playerEntity, playerRigidbody);

		ColliderComponent playerCollider;
		playerCollider.type = ColliderType::Capsule;
		playerCollider.size = Vector3D(0.5f, 1.8f, 0.5f); // Radius in x, height in y
		ecsManager.AddComponent<ColliderComponent>(playerEntity, playerCollider);

		// Create ground entity for physics objects to land on
		Entity groundEntity = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(groundEntity, { 0, -2.0f, 0 });
		ecsManager.transformSystem->SetLocalScale(groundEntity, { 10.0f, 0.1f, 10.0f }); // Large flat ground
		ecsManager.transformSystem->SetLocalRotation(groundEntity, { 0, 0, 0 });
		NameComponent& groundName = ecsManager.GetComponent<NameComponent>(groundEntity);
		groundName.name = "Ground";

		// Add static physics to ground
		RigidbodyComponent groundRigidbody;
		groundRigidbody.bodyType = BodyType::Static;
		groundRigidbody.mass = 0.0f;
		groundRigidbody.restitution = 0.2f; // Small bounce
		groundRigidbody.friction = 0.9f; // High friction for ground
		groundRigidbody.isGravityEnabled = false;
		ecsManager.AddComponent<RigidbodyComponent>(groundEntity, groundRigidbody);

		ColliderComponent groundCollider;
		groundCollider.type = ColliderType::Box;
		groundCollider.size = Vector3D(10.0f, 0.1f, 10.0f); // Match the scale
		ecsManager.AddComponent<ColliderComponent>(groundEntity, groundCollider);

		// Text entity test - commented out to avoid meta file issues
		// Entity text = ecsManager.CreateEntity();
		// ecsManager.GetComponent<NameComponent>(text).name = "Text1";
		// ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "hello woody", 48, MetaFilesManager::GetGUID128FromAssetFile("Resources/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
		// TextRenderComponent& textComp = ecsManager.GetComponent<TextRenderComponent>(text);
		// TextUtils::SetPosition(textComp, Vector3D(800, 100, 0));
		// TextUtils::SetAlignment(textComp, TextRenderComponent::Alignment::CENTER);

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
			audioComp.PlayOnAwake = true;
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

	// Initialize physics system
	if (ecsManager.physicsSystem) {
		ecsManager.physicsSystem->Initialise();
	}

	ENGINE_PRINT("Scene Initialized\n");
}

void SceneInstance::Update(double dt) {
	(void)dt; // Suppress unused parameter warning

	// Update logic for the test scene
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	processInput((float)TimeManager::GetDeltaTime());

	// Update systems.
	mainECS.transformSystem->Update();

	// Update physics system
	if (mainECS.physicsSystem) {
		mainECS.physicsSystem->Update((float)dt);
	}

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
	// Draw physics debug visualization
	ECSManager& debugECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	// Iterate through all entities with physics components and draw their colliders
	for (const auto& entity : debugECS.GetActiveEntities()) {
		if (debugECS.HasComponent<RigidbodyComponent>(entity) && debugECS.HasComponent<ColliderComponent>(entity) && debugECS.HasComponent<Transform>(entity)) {
			auto& rigidbody = debugECS.GetComponent<RigidbodyComponent>(entity);
			auto& collider = debugECS.GetComponent<ColliderComponent>(entity);
			auto& transform = debugECS.GetComponent<Transform>(entity);

			// Get world position from transform
			Vector3D worldPos = Matrix4x4::ExtractTranslation(transform.worldMatrix);
			Vector3D worldScale = transform.localScale;

			// Choose color based on body type
			Vector3D debugColor;
			switch (rigidbody.bodyType) {
				case BodyType::Static:
					debugColor = Vector3D(1, 0, 0); // Red for static
					break;
				case BodyType::Kinematic:
					debugColor = Vector3D(0, 0, 1); // Blue for kinematic
					break;
				case BodyType::Dynamic:
					debugColor = Vector3D(0, 1, 0); // Green for dynamic
					break;
			}

			// Draw collider based on type
			switch (collider.type) {
				case ColliderType::Box:
					DebugDrawSystem::DrawCube(
						worldPos + collider.center,
						Vector3D(collider.size.x * worldScale.x, collider.size.y * worldScale.y, collider.size.z * worldScale.z),
						debugColor
					);
					break;
				case ColliderType::Sphere:
					DebugDrawSystem::DrawSphere(
						worldPos + collider.center,
						collider.size.x * worldScale.x, // Use x component as radius
						debugColor
					);
					break;
				case ColliderType::Capsule:
					// For capsule, draw a cube for now (could be enhanced later)
					DebugDrawSystem::DrawCube(
						worldPos + collider.center,
						Vector3D(collider.size.x * worldScale.x * 2, collider.size.y * worldScale.y, collider.size.x * worldScale.x * 2),
						debugColor
					);
					break;
				default:
					break;
			}
		}
	}

	// Original test debug drawing for reference
	DebugDrawSystem::DrawLine(Vector3D(0, 0, 0), Vector3D(3, 3, 3), Vector3D(0, 0, 1)); // Blue line diagonal

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
	// Static variable to track cursor state
	static bool cursorLocked = true;
	static bool initialized = false;

	// Initialize cursor lock on first run
	if (!initialized) {
		WindowManager::SetCursorMode(true); // Start with cursor locked
		initialized = true;
	}

	// Toggle cursor lock with ESC key
	if (InputManager::GetKeyDown(Input::Key::ESC)) {
		cursorLocked = !cursorLocked;

		if (cursorLocked) {
			// Lock cursor to center and hide it
			WindowManager::SetCursorMode(true); // true = locked/hidden
			firstMouse = true; // Reset mouse for smooth transition
		} else {
			// Free cursor and show it
			WindowManager::SetCursorMode(false); // false = free/visible
		}
		return; // Don't process other input while toggling
	}

	// Move player entity if it exists
	if (playerEntity != static_cast<Entity>(-1)) {
		ENGINE_LOG_DEBUG("[ProcessInput] Player entity exists: " + std::to_string(playerEntity));

		ECSManager& ecsManager = ECSRegistry::GetInstance().GetECSManager(scenePath);

		bool hasTransform = ecsManager.HasComponent<Transform>(playerEntity);
		bool hasPhysicsSystem = (ecsManager.physicsSystem != nullptr);

		ENGINE_LOG_DEBUG("[ProcessInput] Has Transform: " + std::string(hasTransform ? "YES" : "NO"));
		ENGINE_LOG_DEBUG("[ProcessInput] Has PhysicsSystem: " + std::string(hasPhysicsSystem ? "YES" : "NO"));

		if (hasTransform && hasPhysicsSystem) {
			bool hasRigidbody = ecsManager.HasComponent<RigidbodyComponent>(playerEntity);
			ENGINE_LOG_DEBUG("[ProcessInput] Has RigidbodyComponent: " + std::string(hasRigidbody ? "YES" : "NO"));

			if (hasRigidbody) {
				auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(playerEntity);
				ENGINE_LOG_DEBUG("[ProcessInput] Physics body handle: " + std::string(rigidbody.physicsBodyHandle ? "EXISTS" : "NULL"));
			}

			float moveSpeed = 50.0f; // Force magnitude for movement
			Vector3D moveForce(0, 0, 0);

			// Calculate movement direction based on camera orientation (only horizontal plane)
			glm::vec3 forward = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
			glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

			bool keyPressed = false;
			if (InputManager::GetKey(Input::Key::W)) {
				moveForce += Vector3D(forward.x, 0, forward.z) * moveSpeed;
				keyPressed = true;
				ENGINE_LOG_DEBUG("[ProcessInput] W key pressed");
			}
			if (InputManager::GetKey(Input::Key::S)) {
				moveForce -= Vector3D(forward.x, 0, forward.z) * moveSpeed;
				keyPressed = true;
				ENGINE_LOG_DEBUG("[ProcessInput] S key pressed");
			}
			if (InputManager::GetKey(Input::Key::A)) {
				moveForce -= Vector3D(right.x, 0, right.z) * moveSpeed;
				keyPressed = true;
				ENGINE_LOG_DEBUG("[ProcessInput] A key pressed");
			}
			if (InputManager::GetKey(Input::Key::D)) {
				moveForce += Vector3D(right.x, 0, right.z) * moveSpeed;
				keyPressed = true;
				ENGINE_LOG_DEBUG("[ProcessInput] D key pressed");
			}

			// Apply force to player entity through physics system
			if (moveForce.x != 0 || moveForce.z != 0) {
				ENGINE_LOG_DEBUG("[ProcessInput] Applying force: (" + std::to_string(moveForce.x) + ", " + std::to_string(moveForce.y) + ", " + std::to_string(moveForce.z) + ")");
				ecsManager.physicsSystem->ApplyForce(playerEntity, moveForce);
			}

			// Update camera to follow player
			Transform& playerTransform = ecsManager.GetComponent<Transform>(playerEntity);
			Vector3D playerPos = Matrix4x4::ExtractTranslation(playerTransform.worldMatrix);

			// Position camera behind and above the player
			glm::vec3 offset = -forward * 3.0f + glm::vec3(0, 2.0f, 0); // 3 units behind, 2 units up
			camera.Position = glm::vec3(playerPos.x, playerPos.y, playerPos.z) + offset;
		} else {
			ENGINE_LOG_DEBUG("[ProcessInput] Missing required components for player movement");
		}
	} else {
		// Fallback to direct camera movement if no player entity
		float cameraSpeed = 2.5f * deltaTime;
		if (InputManager::GetKey(Input::Key::W))
			camera.Position += cameraSpeed * camera.Front;
		if (InputManager::GetKey(Input::Key::S))
			camera.Position -= cameraSpeed * camera.Front;
		if (InputManager::GetKey(Input::Key::A))
			camera.Position -= glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;
		if (InputManager::GetKey(Input::Key::D))
			camera.Position += glm::normalize(glm::cross(camera.Front, camera.Up)) * cameraSpeed;
	}

	// Only process mouse movement if cursor is locked
	if (cursorLocked) {
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

		// Reduce sensitivity to make camera movement less aggressive
		const float sensitivity = 0.3f; // Much lower sensitivity
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		lastX = xpos;
		lastY = ypos;

		camera.ProcessMouseMovement(xoffset, yoffset);
	}
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