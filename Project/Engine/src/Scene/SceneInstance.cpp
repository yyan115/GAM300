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
#ifdef ANDROID
#include <android/log.h>
#endif
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <Logging.hpp>

#include <Animation/AnimationComponent.hpp>
#include <Animation/AnimationSystem.hpp>

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
		ecsManager.AddComponent<ModelRenderComponent>(backpackEntt, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile( AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")),
			MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat")});
		//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
		//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

		Entity backpackEntt2 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(backpackEntt2, { 1, -0.5f, 0 });
		ecsManager.transformSystem->SetLocalScale(backpackEntt2, { .2f, .2f, .2f });
		ecsManager.transformSystem->SetLocalRotation(backpackEntt2, { 0, 0, 0 });
		NameComponent& backpack2Name = ecsManager.GetComponent<NameComponent>(backpackEntt2);
		backpack2Name.name = "ash ketchum";
		ecsManager.AddComponent<ModelRenderComponent>(backpackEntt2, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")),
			MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat")});
		//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt2, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
		//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});
		
		//PHYSICS TEST CODE
		ecsManager.physicsSystem->Initialise();
		Entity physicsBoxObj = ecsManager.CreateEntity();
		ecsManager.AddComponent<Transform>(physicsBoxObj, Transform{});
		ecsManager.AddComponent<RigidBodyComponent>(physicsBoxObj, RigidBodyComponent{});
		ecsManager.AddComponent<ColliderComponent>(physicsBoxObj, ColliderComponent{});
		Transform& physicsTransform = ecsManager.GetComponent<Transform>(physicsBoxObj);
		physicsTransform.localPosition = { 0.5f, 60.5f, 0 };
		physicsTransform.localScale = { .8f, .8f, .8f };
		physicsTransform.localRotation = { 0, 0, 0, 0 };

		RigidBodyComponent& rb = ecsManager.GetComponent<RigidBodyComponent>(physicsBoxObj);
		rb.motion = Motion::Dynamic;
		rb.ccd = false;
		rb.transform_dirty = true;
		rb.motion_dirty = true;
		rb.collider_seen_version = 0;

		ColliderComponent& col = ecsManager.GetComponent<ColliderComponent>(physicsBoxObj);
		col.shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
		col.layer = Layers::MOVING;
		col.version++;

		ecsManager.AddComponent<ModelRenderComponent>(physicsBoxObj, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")), MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat") });

		
		// ---- FLOOR (static, invisible) ----
		Entity floor = ecsManager.CreateEntity();
		ecsManager.AddComponent<Transform>(floor, Transform{});
		ecsManager.AddComponent<RigidBodyComponent>(floor, RigidBodyComponent{});
		ecsManager.AddComponent<ColliderComponent>(floor, ColliderComponent{});

		// Transform: center the floor at y = 0 (top surface near y = +0.5 since half-extent is 0.5)
		auto& floorTr = ecsManager.GetComponent<Transform>(floor);
		floorTr.localPosition = { 0.0f, -0.5f, 0.0f };
		floorTr.localRotation = { 0, 0, 0, 0 };
		floorTr.localScale = { 1, 1, 1 }; // render-only; physics size comes from the shape

		auto& floorRb = ecsManager.GetComponent<RigidBodyComponent>(floor);
		floorRb.motion = Motion::Static;
		floorRb.ccd = false;
		floorRb.transform_dirty = true;
		floorRb.motion_dirty = true;
		floorRb.collider_seen_version = 0;

		ColliderComponent& floorCol = ecsManager.GetComponent<ColliderComponent>(floor);
		// Large, thin box: 200 x 1 x 200 (half-extents 100,0.5,100)
		floorCol.shape = new JPH::BoxShape(JPH::Vec3(100.f, 0.5f, 100.f));
		floorCol.layer = Layers::NON_MOVING;
		floorCol.version++;

		// ---- SECOND DYNAMIC BACKPACK (Sphere shape for bouncy behavior) ----
		Entity backpack2 = ecsManager.CreateEntity();
		ecsManager.AddComponent<Transform>(backpack2, Transform{});
		ecsManager.AddComponent<RigidBodyComponent>(backpack2, RigidBodyComponent{});
		ecsManager.AddComponent<ColliderComponent>(backpack2, ColliderComponent{});

		auto& backpack2Tr = ecsManager.GetComponent<Transform>(backpack2);
		backpack2Tr.localPosition = { 2.0f, 8.0f, 0.0f }; // High up, offset to the right
		backpack2Tr.localScale = { 0.8f, 0.8f, 0.8f };
		backpack2Tr.localRotation = { 0, 0, 0, 0 };

		auto& backpack2Rb = ecsManager.GetComponent<RigidBodyComponent>(backpack2);
		backpack2Rb.motion = Motion::Dynamic;
		backpack2Rb.ccd = false;
		backpack2Rb.transform_dirty = true;
		backpack2Rb.motion_dirty = true;
		backpack2Rb.collider_seen_version = 0;

		auto& backpack2Col = ecsManager.GetComponent<ColliderComponent>(backpack2);
		// Sphere shape for better bouncing
		backpack2Col.shape = new JPH::SphereShape(0.6f);
		backpack2Col.layer = Layers::MOVING;
		backpack2Col.version++;

		ecsManager.AddComponent<ModelRenderComponent>(backpack2, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")), MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat") });

		// Add name component
		ecsManager.AddComponent<NameComponent>(backpack2, NameComponent{});
		NameComponent& physicsBackpack2Name = ecsManager.GetComponent<NameComponent>(backpack2);
		physicsBackpack2Name.name = "Bouncy Sphere Backpack";

		// ---- THIRD DYNAMIC BACKPACK (Different position for collision demo) ----
		Entity backpack3 = ecsManager.CreateEntity();
		ecsManager.AddComponent<Transform>(backpack3, Transform{});
		ecsManager.AddComponent<RigidBodyComponent>(backpack3, RigidBodyComponent{});
		ecsManager.AddComponent<ColliderComponent>(backpack3, ColliderComponent{});

		auto& backpack3Tr = ecsManager.GetComponent<Transform>(backpack3);
		backpack3Tr.localPosition = { -1.5f, 6.0f, 0.0f }; // High up, offset to the left
		backpack3Tr.localScale = { 0.7f, 0.7f, 0.7f };
		backpack3Tr.localRotation = { 0, 0, 0, 0 };

		auto& backpack3Rb = ecsManager.GetComponent<RigidBodyComponent>(backpack3);
		backpack3Rb.motion = Motion::Dynamic;
		backpack3Rb.ccd = false;
		backpack3Rb.transform_dirty = true;
		backpack3Rb.motion_dirty = true;
		backpack3Rb.collider_seen_version = 0;

		auto& backpack3Col = ecsManager.GetComponent<ColliderComponent>(backpack3);
		// Box shape with different size
		backpack3Col.shape = new JPH::BoxShape(JPH::Vec3(0.4f, 0.4f, 0.4f));
		backpack3Col.layer = Layers::MOVING;
		backpack3Col.version++;

		ecsManager.AddComponent<ModelRenderComponent>(backpack3, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")), MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat") });

		// Add name component
		ecsManager.AddComponent<NameComponent>(backpack3, NameComponent{});
		NameComponent& physicsBackpack3Name = ecsManager.GetComponent<NameComponent>(backpack3);
		physicsBackpack3Name.name = "Box Backpack";

		ecsManager.physicsSystem->physicsAuthoring(ecsManager);

		Entity backpackEntt3 = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(backpackEntt3, { -2, 0.5f, 0 });
		ecsManager.transformSystem->SetLocalScale(backpackEntt3, { .5f, .5f, .5f });
		ecsManager.transformSystem->SetLocalRotation(backpackEntt3, { 50, 70, 20 });
		NameComponent& backpack3Name = ecsManager.GetComponent<NameComponent>(backpackEntt3);
		backpack3Name.name = "indiana jones";
		ecsManager.AddComponent<ModelRenderComponent>(backpackEntt3, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/backpack/backpack.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")),
			MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat") });
		//ecsManager.AddComponent<ModelRenderComponent>(backpackEntt3, ModelRenderComponent{ ResourceManager::GetInstance().GetResource<Model>("Resources/Models/backpack/backpack.obj"),
		//	ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"))});

		// SPRITE
		Entity sprite = ecsManager.CreateEntity();
		NameComponent& spriteName = ecsManager.GetComponent<NameComponent>(sprite);
		spriteName.name = "sprite_test";
		// Load resources first
		auto spriteTexture = MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Textures/awesomeface.png");
		auto spriteShader = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));
		// Add component with constructor parameters
		ecsManager.AddComponent<SpriteRenderComponent>(sprite, SpriteRenderComponent{ spriteTexture, spriteShader });
		// Get reference and configure
		auto& spriteComponent = ecsManager.GetComponent<SpriteRenderComponent>(sprite);
		spriteComponent.is3D = false;  // 2D screen space
		spriteComponent.position = Vector3D(25.0f, 700.0f, 0.0f);  // Screen coordinates (pixels)
		spriteComponent.scale = Vector3D(200.0f, 200.0f, 1.0f);
		spriteComponent.isVisible = true;

		// With billboard effect
		Entity sprite3D = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(sprite3D, { 2.0f, 1.0f, 0.0f });  // World coordinates
		ecsManager.transformSystem->SetLocalScale(sprite3D, { 1.0f, 1.0f, 1.0f });
		ecsManager.transformSystem->SetLocalRotation(sprite3D, { 0, 0, 0 });
		NameComponent& spriteName3D = ecsManager.GetComponent<NameComponent>(sprite3D);
		spriteName3D.name = "sprite_3d_test";
		ecsManager.AddComponent<SpriteRenderComponent>(sprite3D, SpriteRenderComponent{ spriteTexture, spriteShader });
		auto& spriteComponent3D = ecsManager.GetComponent<SpriteRenderComponent>(sprite3D);
		spriteComponent3D.is3D = true;
		spriteComponent3D.position = Vector3D(2.0f, 1.0f, 0.0f);  // Set position for 3D rendering
		spriteComponent3D.saved3DPosition = Vector3D(2.0f, 1.0f, 0.0f);  // Initialize saved position
		spriteComponent3D.scale = Vector3D(0.5f, 0.5f, 0.5f);  // World units, not pixels
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
		spriteComponent3DFlat.position = Vector3D(-2.0f, 1.0f, 0.0f);  // Set position for 3D rendering
		spriteComponent3DFlat.saved3DPosition = Vector3D(-2.0f, 1.0f, 0.0f);  // Initialize saved position
		spriteComponent3DFlat.scale = Vector3D(0.5f, 0.5f, 0.5f);  // World units, not pixels
		spriteComponent3DFlat.isVisible = true;
		spriteComponent3DFlat.enableBillboard = false;

		// Text entity test
		Entity text = ecsManager.CreateEntity();
		ecsManager.GetComponent<NameComponent>(text).name = "Text1";
		ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "hello woody", 48, MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
		//ecsManager.AddComponent<TextRenderComponent>(text, TextRenderComponent{ "Hello World!", ResourceManager::GetInstance().GetFontResource("Resources/Fonts/Kenney Mini.ttf"), ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text")) });
		TextRenderComponent& textComp = ecsManager.GetComponent<TextRenderComponent>(text);
		TextUtils::SetPosition(textComp, Vector3D(800, 100, 0));
		TextUtils::SetAlignment(textComp, TextRenderComponent::Alignment::CENTER);

		// Test Audio
		// Create an entity with AudioComponent
		Entity audioEntity = ecsManager.CreateEntity();
		ecsManager.transformSystem->SetLocalPosition(audioEntity, { 0, 0, 0 });
		NameComponent& audioName = ecsManager.GetComponent<NameComponent>(audioEntity);
		audioName.name = "Audio Test Entity";
				
		// Add AudioComponent
		AudioComponent audioComp;
		audioComp.Clip = AssetManager::GetInstance().GetRootAssetDirectory() + "/Audio/sfx/start menu bgm.ogg";
		audioComp.Volume = 0.3f;
		audioComp.Loop = true;
		audioComp.PlayOnAwake = true;
		audioComp.Spatialize = false;
		ecsManager.AddComponent<AudioComponent>(audioEntity, audioComp);

		// Add FPS text (mainly for android to see FPS)
		fpsText = ecsManager.CreateEntity();
		ecsManager.GetComponent<NameComponent>(fpsText).name = "FPSText";
		ecsManager.AddComponent<TextRenderComponent>(fpsText, TextRenderComponent{ "FPS PLACEHOLDER", 30, MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
		TextRenderComponent& fpsTextComp = ecsManager.GetComponent<TextRenderComponent>(fpsText);
		TextUtils::SetPosition(fpsTextComp, Vector3D(400, 500, 0));
		TextUtils::SetAlignment(fpsTextComp, TextRenderComponent::Alignment::LEFT);

		// Test Particle
		Entity particleEntity = ecsManager.CreateEntity();
		ecsManager.GetComponent<NameComponent>(particleEntity).name = "Test Particles";
		ecsManager.AddComponent<ParticleComponent>(particleEntity, ParticleComponent{});
		auto& particleComp = ecsManager.GetComponent<ParticleComponent>(particleEntity);
		ecsManager.transformSystem->SetLocalPosition(particleEntity, { 0, 1, -3 }); // Used to set emitterPosition variable
		particleComp.emissionRate = 50.0f;
		particleComp.maxParticles = 1000;
		particleComp.particleLifetime = 2.0f;
		particleComp.startSize = 0.2f;
		particleComp.endSize = 0.05f;
		particleComp.startColor = Vector3D(1.0f, 0.8f, 0.2f);  // Orange
		particleComp.startColorAlpha = 1.0f;
		particleComp.endColor = Vector3D(1.0f, 0.0f, 0.0f);    // Red fade out
		particleComp.endColorAlpha = 0.0f;
		particleComp.initialVelocity = Vector3D(0, 2.0f, 0);
		particleComp.velocityRandomness = 1.0f;
		particleComp.gravity = Vector3D(0, -2.0f, 0);
		// Load resources
		particleComp.particleTexture = ResourceManager::GetInstance().GetResource<Texture>("Resources/Textures/awesomeface.png");
		particleComp.texturePath = "Resources/Textures/awesomeface.png";  // Store path for display
		particleComp.particleShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("particle"));

	}

	// Initialize lighting system and create light entities
	if (ecsManager.lightingSystem)
	{
		ecsManager.lightingSystem->Initialise();

		//// Create a directional light (sun)
		//Entity sunLight = ecsManager.CreateEntity();
		//NameComponent& sunName = ecsManager.GetComponent<NameComponent>(sunLight);
		//sunName.name = "Sun";
		//ecsManager.AddComponent<Transform>(sunLight, Transform{});

		//DirectionalLightComponent sunLightComp;
		//sunLightComp.direction = Vector3D(-0.2f, -1.0f, -0.3f);
		//sunLightComp.ambient = Vector3D(0.05f, 0.05f, 0.05f);
		//sunLightComp.diffuse = Vector3D(0.4f, 0.4f, 0.4f);
		//sunLightComp.specular = Vector3D(0.5f, 0.5f, 0.5f);
		//sunLightComp.enabled = true;
		//ecsManager.AddComponent<DirectionalLightComponent>(sunLight, sunLightComp);
		////ecsManager.lightingSystem->RegisterEntity(sunLight);

		//// Create point lights
		//std::vector<Vector3D> pointLightPositions = {
		//	Vector3D(0.7f,  0.2f,  2.0f),
		//	Vector3D(2.3f, -3.3f, -4.0f),
		//	Vector3D(-4.0f,  2.0f, -12.0f),
		//	Vector3D(0.0f,  0.0f, -3.0f)
		//};

		//for (size_t i = 0; i < pointLightPositions.size(); i++)
		//{
		//	Entity pointLight = ecsManager.CreateEntity();
		//	NameComponent& pointLightName = ecsManager.GetComponent<NameComponent>(pointLight);
		//	pointLightName.name = "Point Light " + std::to_string(i);
		//	ecsManager.transformSystem->SetLocalPosition(pointLight, pointLightPositions[i]);
		//	ecsManager.transformSystem->SetLocalScale(pointLight, { .01f, .01f, .01f });
		//	// ecsManager.transformSystem->SetLocalRotation(pointLight, {}); // IF NEEDED

		//	// Test Model
		//	ecsManager.AddComponent<ModelRenderComponent>(pointLight, ModelRenderComponent{ MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Models/FinalBaseMesh.obj"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("default")),
		//		MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/Backpack Material.mat") });

		//	PointLightComponent pointLightComp;
		//	pointLightComp.ambient = Vector3D(0.05f, 0.05f, 0.05f);
		//	pointLightComp.diffuse = Vector3D(0.8f, 0.8f, 0.8f);
		//	pointLightComp.specular = Vector3D(1.0f, 1.0f, 1.0f);
		//	pointLightComp.constant = 1.0f;
		//	pointLightComp.linear = 0.09f;
		//	pointLightComp.quadratic = 0.032f;
		//	pointLightComp.enabled = true;
		//	ecsManager.AddComponent<PointLightComponent>(pointLight, pointLightComp);
		//	//ecsManager.lightingSystem->RegisterEntity(pointLight);

		//}

		//// Create a spot light that follows the camera
		//Entity spotLight = ecsManager.CreateEntity();
		//NameComponent& spotLightName = ecsManager.GetComponent<NameComponent>(spotLight);
		//spotLightName.name = "Flashlight";
		//ecsManager.transformSystem->SetLocalPosition(spotLight, Vector3D{ 0.f, 0.f, 3.f });
		////ecsManager.transformSystem->SetLocalScale(pointLight, { .01f, .01f, .01f }); // IF NEEDED
		//// ecsManager.transformSystem->SetLocalRotation(pointLight, {}); // IF NEEDED

		//SpotLightComponent spotLightComp;
		//spotLightComp.direction = Vector3D::ConvertGLMToVector3D(camera.Front);
		//spotLightComp.ambient = Vector3D::Zero();
		//spotLightComp.diffuse = Vector3D::Ones();
		//spotLightComp.specular = Vector3D::Ones();
		//spotLightComp.constant = 1.0f;
		//spotLightComp.linear = 0.09f;
		//spotLightComp.quadratic = 0.032f;
		//spotLightComp.cutOff = 0.976f;
		//spotLightComp.outerCutOff = 0.966f;
		//spotLightComp.enabled = true;
		//ecsManager.AddComponent<SpotLightComponent>(spotLight, spotLightComp);
		////ecsManager.lightingSystem->RegisterEntity(spotLight);
	}
	ENGINE_PRINT("[Scene] Lighting system entity count: ", ecsManager.lightingSystem->entities.size(), "\n");
	

	// Add FPS text (mainly for android to see FPS)
	fpsText = ecsManager.CreateEntity();
	ecsManager.GetComponent<NameComponent>(fpsText).name = "FPSText";
	ecsManager.AddComponent<TextRenderComponent>(fpsText, TextRenderComponent{ "FPS PLACEHOLDER", 30, MetaFilesManager::GetGUID128FromAssetFile(AssetManager::GetInstance().GetRootAssetDirectory() + "/Fonts/Kenney Mini.ttf"), MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text")) });
	TextRenderComponent& fpsTextComp = ecsManager.GetComponent<TextRenderComponent>(fpsText);
	TextUtils::SetPosition(fpsTextComp, Vector3D(400, 500, 0));
	TextUtils::SetAlignment(fpsTextComp, TextRenderComponent::Alignment::LEFT);

	// Sets camera
	gfxManager.SetCamera(&camera);

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
	ecsManager.animationSystem->Initialise();

	ENGINE_PRINT("Scene Initialized\n");
}

void SceneInstance::Update(double dt) {
	dt;

	// Update logic for the test scene
	ECSManager& mainECS = ECSRegistry::GetInstance().GetECSManager(scenePath);

	TextRenderComponent& fpsTextComponent = mainECS.GetComponent<TextRenderComponent>(fpsText);
	fpsTextComponent.text = TimeManager::GetFps();
	TextUtils::SetText(fpsTextComponent, std::to_string(TimeManager::GetFps()));

	processInput((float)TimeManager::GetDeltaTime());

	// Update systems.
	mainECS.physicsSystem->Update((float)TimeManager::GetDeltaTime());
	mainECS.physicsSystem->physicsSyncBack(mainECS);
	mainECS.transformSystem->Update();

	mainECS.animationSystem->Update();


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

	gfxManager.SetCamera(&camera);
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

	if(mainECS.animationSystem)
		mainECS.animationSystem->Update(); // After rendering to reset any pose changes if necessary


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
	ECSRegistry::GetInstance().GetECSManager(scenePath).physicsSystem->Shutdown();
	ECSRegistry::GetInstance().GetECSManager(scenePath).particleSystem->Shutdown();
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
