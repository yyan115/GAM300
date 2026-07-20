#include "pch.h"
#include "Graphics/GraphicsManager.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

#ifdef ANDROID
#include <android/log.h>
#endif

// Tracy GPU profiling (desktop only)
#if defined(TRACY_ENABLE) && !defined(ANDROID) && !defined(__APPLE__)
#include "tracy/TracyOpenGL.hpp"
#define PROFILE_GPU_CONTEXT   TracyGpuContext
#define PROFILE_GPU_ZONE(name) TracyGpuZone(name)
#else
#define PROFILE_GPU_CONTEXT   ((void)0)
#define PROFILE_GPU_ZONE(name) ((void)0)
#endif

#include <Transform/TransformSystem.hpp>
#include <ECS/ECSManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <ECS/SortingLayerManager.hpp>
#include "Logging.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Camera/CameraSystem.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Graphics/Instancing/InstancingManager.hpp"
#include "TimeManager.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"

namespace {
	#if !defined(_WIN32) && !defined(ANDROID)
	bool ShaderResourceExists(const std::string& shaderPath)
	{
		if (shaderPath.empty())
		{
			return false;
		}

		return std::filesystem::exists(shaderPath) ||
			std::filesystem::exists(shaderPath + ".meta") ||
			std::filesystem::exists(shaderPath + ".vert") ||
			std::filesystem::exists(shaderPath + ".frag") ||
			std::filesystem::exists(shaderPath + ".geom");
	}
	#endif
}

GraphicsManager& GraphicsManager::GetInstance()
{
	static GraphicsManager instance;
	return instance;
}

bool GraphicsManager::Initialize(int window_width, int window_height)
{
	(void)window_width;
	(void)window_height;
	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Enable face culling (backface culling)
	if (faceCullingEnabled)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	GLenum cMode = GL_BACK;
	if (cullMode == CullMode::FRONT) cMode = GL_FRONT;
	if (cullMode == CullMode::FRONT_AND_BACK) cMode = GL_FRONT_AND_BACK;
	glCullFace(cMode);      // Cull back-facing triangles

	GLenum fFace = GL_CCW;
	if (frontFace == FrontFace::CW) fFace = GL_CW;
	glFrontFace(fFace);      // Counter-clockwise winding = front face

	// Initialize Tracy GPU profiling context (must be after GL is ready)
	PROFILE_GPU_CONTEXT;

	// Initialize skybox
	InitializeSkybox();

	// Load depth prepass shader (PC only — Android uses OpenGL ES which doesn't support #version 430)
#ifdef ANDROID
	m_depthPrepassEnabled = false;
#else
	{
		std::string prepassPath = ResourceManager::GetPlatformShaderPath("depth_prepass");
	#if !defined(_WIN32)
		if (!ShaderResourceExists(prepassPath))
		{
			ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[GraphicsManager] depth_prepass shader not found - depth prepass disabled\n");
			m_depthPrepassEnabled = false;
		}
		else
	#endif
		{
			m_depthPrepassShader = ResourceManager::GetInstance().GetResource<Shader>(prepassPath);
			if (!m_depthPrepassShader)
			{
				ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[GraphicsManager] depth_prepass shader failed to load - depth prepass disabled\n");
				m_depthPrepassEnabled = false;
			}
			else
			{
				ENGINE_PRINT("[GraphicsManager] Depth prepass shader loaded\n");
			}
		}
	}
#endif

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.lightingSystem)
	{
		ecsManager.lightingSystem->Initialise();

		ecsManager.lightingSystem->SetShadowRenderCallback(
			[this](Shader& depthShader) {
				RenderSceneForShadows(depthShader);
			}
		);
	}

	InitCameraUBO();

	ENGINE_PRINT("[GraphicsManager] Initialized - Face culling enabled\n");
	return true;
}

void GraphicsManager::Shutdown()
{
	ECSManager& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

	renderQueue.clear();
	currentCamera = nullptr;
	mainECS.spriteSystem->Shutdown();
	mainECS.particleSystem->Shutdown();
	mainECS.cameraSystem->Shutdown();

	if (m_cameraUBO != 0) {
		glDeleteBuffers(1, &m_cameraUBO);
		m_cameraUBO = 0;
	}

	if (skyboxVAO != 0) {
		glDeleteVertexArrays(1, &skyboxVAO);
		skyboxVAO = 0;
	}
	if (skyboxVBO != 0) {
		glDeleteBuffers(1, &skyboxVBO);
		skyboxVBO = 0;
	}
	skyboxShader = nullptr;

	ENGINE_PRINT("[GraphicsManager] Shutdown\n");
}

void GraphicsManager::BeginFrame()
{
	renderQueue.clear();
	deferredQueue.clear();

	// Reset state tracking
	m_currentShader = nullptr;
	m_currentMaterial = nullptr;
	m_sortingStats.Reset();

	if (InstancingManager::GetInstance().IsEnabled())
	{
		InstancingManager::GetInstance().BeginFrame();

		// Update frustum NOW so TryAddInstance (called during Update) uses the
		// current-frame frustum rather than the previous frame's frustum.
		// Without this, frustum-culled instanceable models silently disappear:
		// they are neither added to a batch nor submitted to the render queue.
		UpdateFrustum();
		InstancingManager::GetInstance().SetFrustum(
			frustumCullingEnabled ? &viewFrustum : nullptr);
	}
}

void GraphicsManager::EndFrame()
{
	
}

void GraphicsManager::Clear(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GraphicsManager::SetCamera(Camera* camera)
{
	if (camera != nullptr)
	{
		currentCamera = camera;
	}
	else
	{
		// Keep the current camera if trying to set null (prevents crashes)
		ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[GraphicsManager] Attempted to set null camera, keeping current camera\n");
	}
}

void GraphicsManager::SetViewportSize(int width, int height)
{
	viewportWidth = width;
	viewportHeight = height;

	// Also set OpenGL viewport to keep state synchronized
	glViewport(0, 0, width, height);
}

void GraphicsManager::GetViewportSize(int& width, int& height) const
{
	width = viewportWidth;
	height = viewportHeight;
}

void GraphicsManager::Submit(std::unique_ptr<IRenderComponent> renderItem)
{
	std::lock_guard<std::mutex> lock(renderQueueMutex);
	if (renderItem && renderItem->isVisible)
	{
		renderQueue.push_back(std::move(renderItem));
	}
}

void GraphicsManager::SubmitBatch(std::vector<std::unique_ptr<IRenderComponent>>&& renderItems)
{
	if (renderItems.empty())
	{
		return;
	}

	std::lock_guard<std::mutex> lock(renderQueueMutex);
	renderQueue.reserve(renderQueue.size() + renderItems.size());
	for (auto& renderItem : renderItems)
	{
		if (renderItem && renderItem->isVisible)
		{
			renderQueue.push_back(std::move(renderItem));
		}
	}
}

void GraphicsManager::UpdateFrustum()
{
	PROFILE_FUNCTION();

	if (!currentCamera)
	{
		return;
	}

	if (frustumCullingEnabled)
	{
		ViewportDimensions currentVP = GetCurrentViewport();
		int renderWidth = currentVP.width;
		int renderHeight = currentVP.height;
		float aspectRatio = currentVP.aspectRatio;

		glm::mat4 view;
		glm::mat4 projection;

		if (IsRenderingForEditor() && Is2DMode())
		{
			view = glm::mat4(1.0f);
			float viewWidth = renderWidth * currentCamera->OrthoZoomLevel;
			float viewHeight = renderHeight * currentCamera->OrthoZoomLevel;
			float halfWidth = viewWidth * 0.5f;
			float halfHeight = viewHeight * 0.5f;
			float left = currentCamera->Position.x - halfWidth;
			float right = currentCamera->Position.x + halfWidth;
			float bottom = currentCamera->Position.y - halfHeight;
			float top = currentCamera->Position.y + halfHeight;
			projection = glm::ortho(left, right, bottom, top, -1000.0f, 1000.0f);
		}
		else
		{
			view = currentCamera->GetViewMatrix();
			projection = glm::perspective(
				glm::radians(currentCamera->Zoom),
				aspectRatio,
				0.1f, m_farPlane
			);
		}

		glm::mat4 viewProjection = projection * view;
		viewFrustum.Update(viewProjection);
	}
}

void GraphicsManager::Render()
{
	PROFILE_FUNCTION();
	{
		PROFILE_SCOPED("GM::GPUZoneScope");
		PROFILE_GPU_ZONE("Render");

	if (!currentCamera)
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GraphicsManager] Warning: No camera set for rendering!\n");
		return;
	}

	if (frustumCullingEnabled)
	{
		cullingStats.Reset();
	}

	// NOTE: m_hasBloomEmissionThisFrame is NOT reset here — it was already set
	// during the system update phase (ModelSystem, SpriteSystem, ParticleSystem,
	// TextRenderingSystem) which runs BEFORE Render(). Resetting here would
	// clear the flag before the post-process pass reads it.
	// The flag is reset at the start of ModelSystem::Update() instead.

	currentFrameViewport = GetCurrentViewport();

	// Compute view/projection once for the whole frame and upload to Camera UBO.
	// All shaders that declare CameraBlock automatically receive these values.
	glm::mat4 frameView = currentCamera->GetViewMatrix();
	glm::mat4 frameProjection = glm::perspective(
		glm::radians(currentCamera->Zoom),
		currentFrameViewport.aspectRatio,
		0.1f, m_farPlane
	);
	if (m_cameraUBO != 0)
		UploadCameraUBO(frameView, frameProjection, currentCamera->Position);

	ECSManager* ecsManagerPtr = nullptr;
	{
		PROFILE_SCOPED("GM::GetECSManager");
		ecsManagerPtr = &ECSRegistry::GetInstance().GetActiveECSManager();
	}
	ECSManager& ecsManager = *ecsManagerPtr;

	{
		PROFILE_SCOPED("GM::ShadowMaps");
		if (ecsManager.lightingSystem)
		{
			ecsManager.lightingSystem->RenderShadowMaps(
				PostProcessingManager::GetInstance().GetHDRFramebuffer(),
				currentFrameViewport.width,
				currentFrameViewport.height);
		}
	}

	// Render skybox first (before other objects)
	{
		PROFILE_SCOPED("GM::Skybox");
		RenderSkybox();
	}

	// Separate models from other render items, moving excluded items to deferred queue
	m_modelRenderItems.clear();
	m_otherRenderItems.clear();
	m_modelRenderItems.reserve(renderQueue.size());
	m_otherRenderItems.reserve(renderQueue.size());
	deferredQueue.reserve(deferredQueue.size() + renderQueue.size());

	{
		PROFILE_SCOPED("GM::QueueSeparation");
		for (auto& item : renderQueue)
		{
			if (!item)
			{
				continue;
			}

			if (item->excludeFromPostProcess)
			{
				deferredQueue.push_back(std::move(item));
				continue;
			}
			if (item->GetRenderKind() == RenderComponentKind::Model)
			{
				m_modelRenderItems.push_back(item.get());
			}
			else
			{
				m_otherRenderItems.push_back(item.get());
			}
		}
	}
	// Enable MRT so bloom-capable shaders can write to the bloom emission texture
	{
		PROFILE_SCOPED("GM::EnableBloomMRT");
		PostProcessingManager::GetInstance().EnableBloomMRT();
	}

	// Bind skybox texture for environment reflections (high texture unit to avoid conflicts)
	{
		PROFILE_SCOPED("GM::EnvReflectionBind");
		ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
		Entity activeCam = ecs.cameraSystem ? ecs.cameraSystem->GetActiveCameraEntity() : UINT32_MAX;
		bool hasEnv = false;
		float envIntensity = 1.0f;
		if (activeCam != UINT32_MAX && ecs.HasComponent<CameraComponent>(activeCam)) {
			auto& camComp = ecs.GetComponent<CameraComponent>(activeCam);
			if (camComp.envReflectionEnabled && camComp.skyboxTexture) {
				glActiveTexture(GL_TEXTURE12);
				glBindTexture(GL_TEXTURE_2D, camComp.skyboxTexture->ID);
				hasEnv = true;
				envIntensity = camComp.envReflectionIntensity;
			}
		}
		envReflectionActive = hasEnv;
		envReflectionIntensityValue = envIntensity;
	}

	InstancingManager& instancing = InstancingManager::GetInstance();

	// =========================================================================
	// DEPTH PREPASS — write depth for all opaque geometry before color passes.
	// This ensures the expensive main fragment shaders only run on visible pixels.
	// Skipped in 2D mode (ortho, no overdraw problem) and on Android (GLES).
	// =========================================================================
	if (m_depthPrepassEnabled && m_depthPrepassShader && Is3DMode())
	{
		PROFILE_SCOPED("GM::DepthPrepass");
		PROFILE_GPU_ZONE("DepthPrepass");
		RunDepthPrepass(frameView, frameProjection);
		// Main color pass: depth already written — test equal, skip re-write
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
	}

	{
		PROFILE_SCOPED("GM::InstancingRender");
		if (instancing.IsEnabled())
		{
			// Render all batched instances
			instancing.RenderBatches(frameView, frameProjection, currentCamera->Position);

			// End instancing frame
			instancing.EndFrame();

		//// Print stats every 300 frames
		//static int frameCount = 0;
		//if (++frameCount % 300 == 0)
		//{
		//	const auto& stats = instancing.GetStats();
		//	std::cout << "[Instancing Stats]" << std::endl;
		//	std::cout << "  Total objects: " << stats.totalObjects << std::endl;
		//	std::cout << "  Instanced: " << stats.instancedObjects << std::endl;
		//	std::cout << "  Non-instanced: " << stats.nonInstancedObjects << std::endl;
		//	std::cout << "  Batches: " << stats.batchCount << std::endl;
		//	std::cout << "  Draw calls: " << stats.drawCalls << std::endl;
		//	std::cout << "  Culled: " << stats.culledObjects << std::endl;
		//	std::cout << "  Efficiency: " << stats.GetBatchEfficiency() << "%" << std::endl;
		//}

		//// Update stats
		//const auto& instStats = instancing.GetStats();
		//// log stats
		//std::cout << "Instanced: " << instStats.instancedObjects << ", Batches: " << instStats.batchCount << std::endl;
		}
	}

	{
		PROFILE_SCOPED("GM::ModelSort");
		m_modelSortEntries.clear();
		m_modelSortEntries.reserve(m_modelRenderItems.size());
		const glm::vec3 camPos = currentCamera ? currentCamera->Position : glm::vec3(0.0f);

		for (IRenderComponent* item : m_modelRenderItems)
		{
			auto* model = static_cast<ModelRenderComponent*>(item);
			ModelSortEntry entry;
			entry.item = item;
			entry.layer = model->material && model->material->GetOpacity() < 1.0f
				? RenderLayer::Type::LAYER_TRANSPARENT
				: RenderLayer::Type::LAYER_OPAQUE;

			const Vector3D pos = Matrix4x4::ExtractTranslation(model->transform);
			const float dx = pos.x - camPos.x;
			const float dy = pos.y - camPos.y;
			const float dz = pos.z - camPos.z;
			entry.distanceSq = dx * dx + dy * dy + dz * dz;

			if (entry.layer == RenderLayer::Type::LAYER_OPAQUE)
			{
				// Front-to-back: group into ~5-unit buckets so objects at similar
				// depths still batch by state (reduces shader/material switches).
				entry.depthBucket = static_cast<int>(entry.distanceSq / 25.0f);
				entry.stateKey = RenderSortKey(entry.layer,
					m_idCache.GetShaderId(model->shader.get()),
					m_idCache.GetMaterialId(model->material.get()),
					m_idCache.GetModelId(model->model.get()));
			}

			m_modelSortEntries.push_back(entry);
		}

		std::sort(m_modelSortEntries.begin(), m_modelSortEntries.end(),
			[](const ModelSortEntry& a, const ModelSortEntry& b) {
				// Layer is always primary (opaque before transparent)
				if (a.layer != b.layer)
					return a.layer < b.layer;

				if (a.layer == RenderLayer::Type::LAYER_OPAQUE)
				{
					if (a.depthBucket != b.depthBucket)
						return a.depthBucket < b.depthBucket;
					// Same depth bucket — sort by state to minimise GPU state switches.
					return a.stateKey < b.stateKey;
				}

				// Back-to-front for transparency: correct alpha blending.
				return a.distanceSq > b.distanceSq;
			});

		for (size_t i = 0; i < m_modelSortEntries.size(); ++i)
		{
			m_modelRenderItems[i] = m_modelSortEntries[i].item;
		}

		// Sort other items by their existing sorting logic (sprites, text, etc.)
		std::sort(m_otherRenderItems.begin(), m_otherRenderItems.end(),
			[](IRenderComponent* a, IRenderComponent* b) {
				// Keep your existing 2D sorting logic here
				return a->renderOrder < b->renderOrder;
			});
	}

	// =========================================================================
	// Render models with state tracking
	// =========================================================================
	{
		PROFILE_SCOPED("GM::ModelRenderLoop");
		bool blendingOn = false;
		for (IRenderComponent* item : m_modelRenderItems)
		{
			ModelRenderComponent* modelItem = static_cast<ModelRenderComponent*>(item);
			// Skip if it was handled by instancing — but only for fully opaque, non-fading objects.
			// Transparent and fading objects must go through the individual render path for correct blending.
			bool isTransparent = (modelItem->distanceFadeOpacity < 1.0f) ||
				(modelItem->material && modelItem->material->GetOpacity() < 1.0f);
			if (!isTransparent &&
				instancing.IsEnabled() &&
				!modelItem->HasAnimation() &&
				modelItem->model &&
				modelItem->model->mBoneInfoMap.empty())
			{
				continue;  // Already rendered via instancing
			}

			// Enable alpha blending when material opacity or distance fade opacity < 1.
			bool needsBlend = (modelItem->distanceFadeOpacity < 1.0f);
			if (!needsBlend) {
				if (modelItem->material) {
					needsBlend = modelItem->material->GetOpacity() < 1.0f;
				} else if (modelItem->model && !modelItem->model->meshes.empty()
					&& modelItem->model->meshes[0].material) {
					needsBlend = modelItem->model->meshes[0].material->GetOpacity() < 1.0f;
				}
			}
			if (needsBlend && !blendingOn) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDepthMask(GL_FALSE);
				blendingOn = true;
			} else if (!needsBlend && blendingOn) {
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				blendingOn = false;
			}

			RenderModelOptimized(*modelItem);  // New optimized render method
		}
		if (blendingOn) {
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		}
	}

	// =========================================================================
	// Render other items (sprites, text, particles, debug)
	// =========================================================================
	{
		PROFILE_SCOPED("GM::OtherItemsRender");
		for (IRenderComponent* item : m_otherRenderItems) {
			switch (item->GetRenderKind())
			{
			case RenderComponentKind::Text:
				RenderText(*static_cast<TextRenderComponent*>(item));
				break;
			case RenderComponentKind::Sprite:
				RenderSprite(*static_cast<SpriteRenderComponent*>(item));
				break;
			case RenderComponentKind::DebugDraw:
				RenderDebugDraw(*static_cast<DebugDrawComponent*>(item));
				break;
			case RenderComponentKind::Particle:
				RenderParticles(*static_cast<ParticleComponent*>(item));
				break;
			case RenderComponentKind::ParticleRenderItem:
				RenderParticles(*static_cast<ParticleRenderItem*>(item));
				break;
			case RenderComponentKind::Fog:
#ifndef ANDROID
				RenderFogVolume(*static_cast<FogVolumeComponent*>(item));
#endif
				break;
			default:
				break;
			}
		}
	}

	// Restore depth state changed by the prepass (transparents may have already
	// called glDepthMask(GL_TRUE), but calling it again is cheap and safe).
	if (m_depthPrepassEnabled && m_depthPrepassShader && Is3DMode())
	{
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
	}

	// Disable bloom MRT — done writing bloom emission
	{
		PROFILE_SCOPED("GM::DisableBloomMRT");
		PostProcessingManager::GetInstance().DisableBloomMRT();
	}

	// Per-frame render stats plots
#ifdef TRACY_ENABLE
	{
		const auto& instStats = InstancingManager::GetInstance().GetStats();
		PROFILE_PLOT("InstancedObjects", (double)instStats.instancedObjects);
		PROFILE_PLOT("InstancingBatches",(double)instStats.batchCount);
		PROFILE_PLOT("CulledObjects",    (double)instStats.culledObjects);
	}
#endif

	// Debug output (optional - remove in release)
	/*static int frameCount = 0;
	if (++frameCount % 300 == 0)
	{
		std::cout << "[Sorting] Objects: " << m_sortingStats.totalObjects
			<< " DrawCalls: " << m_sortingStats.drawCalls
			<< " ShaderSwitch: " << m_sortingStats.shaderSwitches
			<< " MatSwitch: " << m_sortingStats.materialSwitches << "\n";
	}*/
	} // end GM::GPUZoneScope
}

void GraphicsManager::RenderDeferred()
{
	if (deferredQueue.empty()) return;

	// Separate deferred items into models/others
	m_deferredModelRenderItems.clear();
	m_deferredOtherRenderItems.clear();
	m_deferredModelRenderItems.reserve(deferredQueue.size());
	m_deferredOtherRenderItems.reserve(deferredQueue.size());

	for (auto& item : deferredQueue)
	{
		if (!item) continue;
		if (item->GetRenderKind() == RenderComponentKind::Model)
			m_deferredModelRenderItems.push_back(item.get());
		else
			m_deferredOtherRenderItems.push_back(item.get());
	}

	// Sort others by render order
	std::sort(m_deferredOtherRenderItems.begin(), m_deferredOtherRenderItems.end(),
		[](IRenderComponent* a, IRenderComponent* b) {
			return a->renderOrder < b->renderOrder;
		});

	// Render as overlay: disable depth test, enable blending
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (IRenderComponent* item : m_deferredModelRenderItems)
	{
		RenderModel(*static_cast<ModelRenderComponent*>(item));
	}

	for (IRenderComponent* item : m_deferredOtherRenderItems)
	{
		switch (item->GetRenderKind())
		{
		case RenderComponentKind::Text:
			RenderText(*static_cast<TextRenderComponent*>(item));
			break;
		case RenderComponentKind::Sprite:
			RenderSprite(*static_cast<SpriteRenderComponent*>(item));
			break;
		case RenderComponentKind::Particle:
			RenderParticles(*static_cast<ParticleComponent*>(item));
			break;
		case RenderComponentKind::ParticleRenderItem:
			RenderParticles(*static_cast<ParticleRenderItem*>(item));
			break;
		default:
			break;
		}
	}

	// Restore state
	glEnable(GL_DEPTH_TEST);

	deferredQueue.clear();
}

void GraphicsManager::RenderModel(const ModelRenderComponent& item)
{
	if (!item.isVisible || !item.model || !item.shader) 
	{
		return;
	}
	// Calculate model matrix once
	glm::mat4 modelMatrix = item.transform.ConvertToGLM();

	// Count total objects when culling is enabled
	if (frustumCullingEnabled && currentCamera)
	{
		//cullingStats.totalObjects++;


		AABB modelBBox = item.model->GetBoundingBox();
		//glm::mat4 modelMatrix = item.transform.ConvertToGLM(); // Moved to outer scope
		AABB worldBBox = modelBBox.Transform(modelMatrix);

		// Use tolerance to prevent edge-case culling
		bool isVisible = viewFrustum.IsBoxVisible(worldBBox, 0.5f);

		if (!isVisible)
		{
			//cullingStats.culledObjects++;  // Count as culled
			return;  // Don't render
		}
	}

	// Activate the shader
	item.shader->Activate();

	// Set up all matrices and uniforms
	SetupMatrices(*item.shader,item.transform.ConvertToGLM(), true);

	// Per-entity bloom emission
	item.shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		item.shader->setVec3("bloomColor", item.bloomColor);
	}

	// Per-entity brightness boost
	item.shader->setFloat("brightnessBoost", item.brightnessBoost);

	// Per-entity fade opacity
	item.shader->setFloat("u_distanceFadeOpacity", item.distanceFadeOpacity);

	// Apply lighting
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.lightingSystem)
	{
		ecsManager.lightingSystem->ApplyLighting(*item.shader);
		ecsManager.lightingSystem->ApplyShadows(*item.shader);

		// Temporary debug - remove later
		//static bool once = false;
		//if (!once)
		//{
		//	std::cout << "[Debug] ApplyShadows called" << std::endl;
		//	once = true;
		//}
	}


	//// Draw the model with entity material
	//if (item.depthOffset)
	//{
	//	glEnable(GL_POLYGON_OFFSET_FILL);
	//	glPolygonOffset(item.depthOffsetFactor, item.depthOffsetUnits);
	//}

	if (item.HasAnimation())
		item.model->Draw(*item.shader, *currentCamera, item.material, item, item.animator);
	else
		item.model->Draw(*item.shader, *currentCamera, item.material, item);

	//if (item.depthOffset)
	//{
	//	glDisable(GL_POLYGON_OFFSET_FILL);
	//}

	//std::cout << "rendered model\n";
}

void GraphicsManager::SetupMatrices(Shader& shader, const glm::mat4& modelMatrix, bool includeNormalMatrix)
{
	shader.setMat4("model", modelMatrix);

	// Only calculate and send normal matrix if needed (for lit objects)
	if (includeNormalMatrix)
	{
		glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));
		shader.setMat3("normalMatrixCPU", normalMatrix);
	}

	if (currentCamera && !shader.UsesCameraBlock())
	{
		int renderWidth = currentFrameViewport.width;
		int renderHeight = currentFrameViewport.height;
		float aspectRatio = currentFrameViewport.aspectRatio;

		glm::mat4 view;
		glm::mat4 projection;

		// In 2D editor mode, use orthographic projection with screen-space coordinates
		if (IsRenderingForEditor() && Is2DMode()) {
			// Use identity view matrix for 2D (camera doesn't rotate)
			view = glm::mat4(1.0f);

			// Use target game resolution for consistent 2D rendering between Scene and Game panels
			float gameWidth = (float)targetGameWidth;
			float gameHeight = (float)targetGameHeight;
			float gameAspect = gameWidth / gameHeight;
			float viewportAspect = (float)renderWidth / (float)renderHeight;

			// Calculate view dimensions that preserve game aspect ratio within viewport
			float viewWidth, viewHeight;
			if (viewportAspect > gameAspect) {
				// Viewport is wider than game - fit by height, add horizontal padding
				viewHeight = gameHeight * currentCamera->OrthoZoomLevel;
				viewWidth = viewHeight * viewportAspect;
			} else {
				// Viewport is taller than game - fit by width, add vertical padding
				viewWidth = gameWidth * currentCamera->OrthoZoomLevel;
				viewHeight = viewWidth / viewportAspect;
			}

			// Create projection centered on camera Position
			// Position represents the CENTER of the view (where the camera is looking)
			float halfWidth = viewWidth * 0.5f;
			float halfHeight = viewHeight * 0.5f;
			float left = currentCamera->Position.x - halfWidth;
			float right = currentCamera->Position.x + halfWidth;
			float bottom = currentCamera->Position.y - halfHeight;
			float top = currentCamera->Position.y + halfHeight;

			projection = glm::ortho(left, right, bottom, top, -1000.0f, 1000.0f);

		} else {
			// 3D mode or game mode: use perspective projection
			view = currentCamera->GetViewMatrix();
			projection = glm::perspective(
				glm::radians(currentCamera->Zoom),
				aspectRatio,
				0.1f, m_farPlane
			);
		}

		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setVec3("cameraPos", currentCamera->Position);
	}
}

void GraphicsManager::RenderText(const TextRenderComponent& item)
{
	if (!item.isVisible || !item.font || !item.shader || item.text.empty())
	{
		return;
	}

	if (!item.is3D && IsRenderingForEditor() && !Is2DMode())
	{
		return;
	}

	// Configure depth testing based on 2D/3D mode
	if (item.is3D) {
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}

	// Enable blending for text transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Activate shader and set uniforms
	item.shader->Activate();
	glm::vec4 textColorWithAlpha = glm::vec4(item.color.ConvertToGLM(), item.alpha);
	item.shader->setVec4("textColor", textColorWithAlpha);

	// Per-entity bloom emission
	item.shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		item.shader->setVec3("bloomColor", item.bloomColor);
	}

	// Set up matrices based on whether it's 2D or 3D text
	if (item.is3D)
	{
		glm::mat4 modelMatrix = item.transform.ConvertToGLM();
		SetupMatrices(*item.shader, modelMatrix);
	}
	else
	{
		if (IsRenderingForEditor() && Is2DMode()) {
			glm::mat4 modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, item.position.ConvertToGLM());
			SetupMatrices(*item.shader, modelMatrix);
		}
		else {
			Setup2DTextMatrices(*item.shader, item.position.ConvertToGLM(), 1.0f, 1.0f);
		}
	}

	// Bind VAO and render
	glActiveTexture(GL_TEXTURE0);
	VAO* fontVAO = item.font->GetVAO();
	VBO* fontVBO = item.font->GetVBO();

	if (!fontVAO || !fontVBO)
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GraphicsManager] Font VAO/VBO not initialized!\n");
		glDisable(GL_BLEND);
		return;
	}

	fontVAO->Bind();

	// Calculate scale factors
	float worldScaleFactor = item.is3D ? 0.01f : 1.0f;
	float scaleX = item.is3D ? worldScaleFactor : (item.transformScale.x * worldScaleFactor);
	float scaleY = item.is3D ? worldScaleFactor : (item.transformScale.y * worldScaleFactor);

	// Get line height for multi-line rendering
	float lineHeight = item.font->GetTextHeight(scaleY) * item.lineSpacing;

	// Use pre-computed wrapped lines from TextRenderingSystem
	// If empty (shouldn't happen), fall back to single line
	const std::vector<std::string>& lines = item.wrappedLines.empty()
		? std::vector<std::string>{item.text}
	: item.wrappedLines;

	// Starting Y position (top of text block)
	float startY = 0.0f;

	// Render each line
	for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
	{
		const std::string& line = lines[lineIndex];

		// Calculate X starting position based on alignment for this line
		float x = 0.0f;
		float lineWidth = item.font->GetTextWidth(line, scaleX);

		if (item.alignment == TextRenderComponent::Alignment::CENTER)
		{
			x = -lineWidth / 2.0f;
		}
		else if (item.alignment == TextRenderComponent::Alignment::RIGHT)
		{
			x = -lineWidth;
		}

		// Calculate Y position for this line (line 0 at top, goes down)
		float y = startY - (lineIndex * lineHeight);

		// Render each character in the line
		for (char c : line)
		{
			const Character& ch = item.font->GetCharacter(c);
			if (ch.textureID == 0) {
				continue;
			}

			float xpos = x + ch.bearing.x * scaleX;
			float ypos = y - (ch.size.y - ch.bearing.y) * scaleY;

			float w = ch.size.x * scaleX;
			float h = ch.size.y * scaleY;

			float vertices[6][4] = {
				{ xpos,     ypos + h,   0.0f, 0.0f },
				{ xpos,     ypos,       0.0f, 1.0f },
				{ xpos + w, ypos,       1.0f, 1.0f },

				{ xpos,     ypos + h,   0.0f, 0.0f },
				{ xpos + w, ypos,       1.0f, 1.0f },
				{ xpos + w, ypos + h,   1.0f, 0.0f }
			};

			glBindTexture(GL_TEXTURE_2D, ch.textureID);
			fontVBO->UpdateData(vertices, sizeof(vertices));
			glDrawArrays(GL_TRIANGLES, 0, 6);

			x += (ch.advance >> 6) * scaleX;
		}
	}

	fontVAO->Unbind();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void GraphicsManager::Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scaleX, float scaleY)
{
	// Use target game resolution for 2D projection
	// This ensures Scene Panel and Game Panel show text at consistent positions
	int gameWidth = targetGameWidth;
	int gameHeight = targetGameHeight;

	// Use screen-space projection where (0,0) is at bottom-left corner
	// This matches traditional 2D game coordinate systems and is consistent with sprites
	glm::mat4 projection = glm::ortho(0.0f, (float)gameWidth, 0.0f, (float)gameHeight);

	glm::mat4 view = glm::mat4(1.0f); // Identity matrix for 2D

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position);  // Use position as-is
	model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

	shader.setMat4("projection", projection);
	shader.setMat4("view", view);
	shader.setMat4("model", model);
}

void GraphicsManager::RenderDebugDraw(const DebugDrawComponent& item)
{
	if (!item.isVisible || !item.shader || item.drawCommands.empty()) {
		return;
	}
	// Enable wireframe mode for debug rendering
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GraphicsManager", "Debug wireframe rendering not supported on Android");
#else
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glDisable(GL_DEPTH_TEST);
	// Activate shader
	item.shader->Activate();
	// Render each draw command
	for (const auto& drawCommand : item.drawCommands)
	{
		VAO* currentVAO = nullptr;
		unsigned int indexCount = 0;
		// Select appropriate geometry
		switch (drawCommand.type) {
		case DebugDrawType::CUBE:
			currentVAO = item.cubeVAO;
			indexCount = item.cubeIndexCount;
			break;
		case DebugDrawType::SPHERE:
			currentVAO = item.sphereVAO;
			indexCount = item.sphereIndexCount;
			break;
		case DebugDrawType::LINE:
			currentVAO = item.lineVAO;
			indexCount = 2;
			break;
		case DebugDrawType::MESH_WIREFRAME:
		{
			if (drawCommand.meshModel) 
			{
				// Create transform matrix
				glm::mat4 transform = CreateTransformMatrix(drawCommand.position.ConvertToGLM(), drawCommand.rotation.ConvertToGLM(), drawCommand.scale.ConvertToGLM());

				// Set up matrices and uniforms
				SetupMatrices(*item.shader, transform);
				item.shader->setVec3("debugColor", drawCommand.color.ConvertToGLM());

				// Draw the model in wireframe mode (wireframe is already enabled above)
				drawCommand.meshModel->Draw(*item.shader, *currentCamera);
				continue; // Skip the regular VAO rendering below
			}
			break;
		}
		default:
			continue;
		}

		if (!currentVAO) continue;

		// Create transform matrix
		glm::mat4 transform = CreateTransformMatrix(drawCommand.position.ConvertToGLM(), drawCommand.rotation.ConvertToGLM(), drawCommand.scale.ConvertToGLM());
		// Set up matrices and uniforms
		SetupMatrices(*item.shader, transform);
		item.shader->setVec3("debugColor", drawCommand.color.ConvertToGLM());
		// Bind VAO and render
		currentVAO->Bind();

		if (drawCommand.type == DebugDrawType::LINE)
		{
			glLineWidth(drawCommand.lineWidth);
			glDrawArrays(GL_LINES, 0, indexCount);
		}
		else
		{
			glDrawElements(GL_LINES, indexCount, GL_UNSIGNED_INT, 0);
		}
		currentVAO->Unbind();
	}
	// Restore render state
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (faceCullingEnabled) glEnable(GL_CULL_FACE);
#endif
}

void GraphicsManager::RenderParticles(const ParticleComponent& item)
{
	RenderParticleInstances(
		item,
		item.particleTexture,
		item.particleShader,
		item.particleVAO,
		item.quadEBO,
		item.particles.size(),
		item.additiveBlending);
}

void GraphicsManager::RenderParticles(const ParticleRenderItem& item)
{
	RenderParticleInstances(
		item,
		item.particleTexture,
		item.particleShader,
		item.particleVAO,
		item.quadEBO,
		item.particleCount,
		item.additiveBlending);
}

void GraphicsManager::RenderParticleInstances(
	const IRenderComponent& renderState,
	const std::shared_ptr<Texture>& texture,
	const std::shared_ptr<Shader>& shader,
	VAO* vao,
	EBO* ebo,
	std::size_t particleCount,
	bool additiveBlending)
{
#ifdef ANDROID
	assert(eglGetCurrentContext() != EGL_NO_CONTEXT);
#endif
	if (!renderState.isVisible || particleCount == 0 || !shader || !vao) return;
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	if (additiveBlending)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);              // Additive: glow/fire/magic
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Standard alpha: physical/solid
	glDepthMask(GL_FALSE);

	shader->Activate();

	// Setup camera matrices ONCE for all particles
	if (currentCamera) {
		if (!shader->UsesCameraBlock()) {
			glm::mat4 view = currentCamera->GetViewMatrix();
			shader->setMat4("view", view);

			glm::mat4 projection = glm::perspective(
				glm::radians(currentCamera->Zoom),
				currentFrameViewport.aspectRatio,
				0.1f, m_farPlane
			);
			shader->setMat4("projection", projection);
		}

		// Send camera vectors for billboard calculations in vertex shader
		glm::vec3 cameraRight = glm::normalize(glm::cross(currentCamera->Front, currentCamera->Up));
		shader->setVec3("cameraRight", cameraRight);
		shader->setVec3("cameraUp", currentCamera->Up);
	}

	// Bind texture if available
	if (texture) {
		glActiveTexture(GL_TEXTURE0);
		texture->Bind(0);
		shader->setInt("particleTexture", 0);
	}

	// Per-entity bloom emission
	shader->setFloat("bloomIntensity", renderState.bloomIntensity);
	if (renderState.bloomIntensity > 0.0f) {
		shader->setVec3("bloomColor", renderState.bloomColor);
	}

	// Draw ALL particles with ONE instanced draw call using indices
	vao->Bind();
	if (ebo) ebo->Bind();  // explicitly ensure EBO is bound
#ifndef NDEBUG
	GLint eboBinding = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eboBinding);
	assert(eboBinding != 0 && "VAO has no EBO bound after setup");
#endif

	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(particleCount));
	vao->Unbind();
	//item.quadEBO->Unbind();

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	if (faceCullingEnabled) glEnable(GL_CULL_FACE);
}

void GraphicsManager::RenderSprite(const SpriteRenderComponent& item)
{
	if (!item.isVisible || !item.texture || !item.shader || !item.spriteVAO)
	{
		return;
	}

	// Configure depth testing based on 2D/3D mode
	if (item.is3D) {
		// 3D sprite: enable depth testing
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}
	else {
		// 2D sprite: disable depth testing so render order determines what's on top
		glDisable(GL_DEPTH_TEST);
	}

	// Enable blending for sprite transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	// Activate shader
	item.shader->Activate();

	// Set sprite-specific uniforms
	glm::vec4 spriteColor = glm::vec4(item.color.ConvertToGLM(), item.alpha);
	item.shader->setVec4("spriteColor", spriteColor);
	item.shader->setVec2("uvOffset", item.uvOffset);
	item.shader->setVec2("uvScale", item.uvScale);
	item.shader->setInt("fillMode", item.fillMode);
	if (item.fillMode >= 1 && item.fillMode <= 3) {
		float fillAmount = (item.fillMaxValue > 0.0f)
			? glm::clamp(item.fillValue / item.fillMaxValue, 0.0f, 1.0f)
			: 0.0f;
		item.shader->setFloat("fillAmount", fillAmount);
		item.shader->setInt("fillDirection", item.fillDirection);
		item.shader->setFloat("fillGlow", item.fillGlow);
		item.shader->setFloat("fillBackground", item.fillBackground);
	}

	// Per-entity bloom emission
	item.shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		item.shader->setVec3("bloomColor", item.bloomColor);
	}

	// Set up matrices based on rendering mode
	if (item.is3D)
	{
		// 3D world space sprite (billboard)
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, item.position.ConvertToGLM());

		// Optional: Make sprite face camera (billboard effect)
		if (currentCamera && item.enableBillboard)
		{
			// Create rotation matrix to face camera
			glm::vec3 forward = glm::normalize(currentCamera->Position - item.position.ConvertToGLM());
			glm::vec3 up = currentCamera->Up;
			glm::vec3 right = glm::normalize(glm::cross(forward, up));
			up = glm::cross(right, forward);

			glm::mat4 billboardMatrix = glm::mat4(
				glm::vec4(right, 0.0f),
				glm::vec4(up, 0.0f),
				glm::vec4(-forward, 0.0f),
				glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
			);
			modelMatrix = modelMatrix * billboardMatrix;
		}

		// Apply rotation if specified
		if (item.rotation != 0.0f)
		{
			modelMatrix = glm::rotate(modelMatrix, glm::radians(item.rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		}

		// Apply scale first
		modelMatrix = glm::scale(modelMatrix, item.scale.ConvertToGLM());

		// Center the sprite AFTER scaling: offset by half the scaled size
		// The quad is 0,0 to 1,1, so after scaling it's 0,0 to scale.x,scale.y
		// Offset by -scale/2 to center it
		modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.5f, -0.5f, 0.0f));

		Setup3DSpriteMatrices(*item.shader, modelMatrix);
	}
	else
	{
		// 2D screen space sprite
		// When rendering for editor in 2D mode, use the editor camera's projection (pixel-based orthographic)
		// Otherwise, use the standard window-based projection for game/runtime
		if (IsRenderingForEditor() && Is2DMode()) {
			// Use the editor camera's view/projection matrices (already set up)
			// Render the sprite like a 3D sprite but in 2D space
			glm::mat4 modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, item.position.ConvertToGLM());

			// Apply rotation
			if (item.rotation != 0.0f) {
				modelMatrix = glm::rotate(modelMatrix, glm::radians(item.rotation), glm::vec3(0.0f, 0.0f, 1.0f));
			}

			// Apply scale first
			modelMatrix = glm::scale(modelMatrix, item.scale.ConvertToGLM());

			// Center the sprite AFTER scaling
			modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.5f, -0.5f, 0.0f));

			Setup3DSpriteMatrices(*item.shader, modelMatrix);
		} else {
			// Normal 2D screen-space rendering for game/runtime (uses window pixel coordinates)
			Setup2DSpriteMatrices(*item.shader, item.position.ConvertToGLM(), item.scale.ConvertToGLM(), item.rotation);
		}
	}

	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	item.texture->Bind(0);
	item.shader->setInt("spriteTexture", 0);

	item.spriteVAO->Bind();
	//item.spriteEBO->Bind();

#ifndef NDEBUG
	GLint ebo = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
	if (ebo == 0) {
		ENGINE_LOG_ERROR("VAO " + std::to_string(item.spriteVAO->ID) + " has no EBO bound!");
	}
#endif

	// The SpriteSystem should have already bound the VAO, so just draw
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	item.spriteVAO->Unbind();
	item.spriteEBO->Unbind();
	// Unbind texture
	item.texture->Unbind(0);

	// Disable blending
	glDisable(GL_BLEND);
	// Restore depth testing for 3D objects
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	if (faceCullingEnabled) glEnable(GL_CULL_FACE);
}

void GraphicsManager::Setup2DSpriteMatrices(Shader& shader, const glm::vec3& position, const glm::vec3& scale, float rotation)
{
	// Use target game resolution for 2D projection
	// This ensures Scene Panel and Game Panel show sprites at consistent positions
	int gameWidth = targetGameWidth;
	int gameHeight = targetGameHeight;

	// Use screen-space projection where (0,0) is at bottom-left corner
	// This matches traditional 2D game coordinate systems
	glm::mat4 projection = glm::ortho(0.0f, (float)gameWidth, 0.0f, (float)gameHeight);

	// Create model matrix
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position);

	// Apply rotation around the center of the sprite
	if (rotation != 0.0f)
	{
		model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
	}

	// Apply scale first
	model = glm::scale(model, scale);

	// Center the sprite AFTER scaling: the quad is 0,0 to 1,1, so offset by -0.5,-0.5
	// This makes the position represent the center instead of the corner
	model = glm::translate(model, glm::vec3(-0.5f, -0.5f, 0.0f));
	shader.setMat4("projection", projection);
	shader.setMat4("model", model);
	shader.setMat4("view", glm::mat4(1.0f)); // Identity matrix for 2D
}

void GraphicsManager::Setup3DSpriteMatrices(Shader& shader, const glm::mat4& modelMatrix)
{
	SetupMatrices(shader, modelMatrix);
}

ViewportDimensions GraphicsManager::GetCurrentViewport() const
{
	ViewportDimensions vp;
	vp.width = (viewportWidth > 0) ? viewportWidth : RunTimeVar::window.width;
	vp.height = (viewportHeight > 0) ? viewportHeight : RunTimeVar::window.height;

	// Ensure minimum dimensions
	if (vp.width <= 0) vp.width = 1;
	if (vp.height <= 0) vp.height = 1;

	// Calculate and clamp aspect ratio
	vp.aspectRatio = (float)vp.width / (float)vp.height;
	if (vp.aspectRatio < 0.001f) vp.aspectRatio = 0.001f;
	if (vp.aspectRatio > 1000.0f) vp.aspectRatio = 1000.0f;

	return vp;
}

glm::mat4 GraphicsManager::CreateTransformMatrix(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale)
{
	Vector3D position = { pos.x, pos.y, pos.z };
	Vector3D rotation = { rot.x, rot.y, rot.z };
	Vector3D scaleVec = { scale.x, scale.y, scale.z };

	Matrix4x4 modelMatrix = TransformSystem::CalculateModelMatrix(position, scaleVec, rotation);
	return modelMatrix.ConvertToGLM();
}

void GraphicsManager::InitCameraUBO()
{
	glGenBuffers(1, &m_cameraUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBOData), nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GraphicsManager::UploadCameraUBO(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos)
{
	CameraUBOData data;
	data.view = view;
	data.projection = projection;
	data.cameraPos = camPos;
	glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBOData), &data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GraphicsManager::InitializeSkybox()
{
	float skyboxVertices[] = {
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);

	std::string skyboxShaderPath = ResourceManager::GetPlatformShaderPath("skybox");
	skyboxShader = ResourceManager::GetInstance().GetResource<Shader>(skyboxShaderPath);
	if (!skyboxShader) {
		//std::cout << "[GraphicsManager] WARNING: Failed to load skybox shader from: " << skyboxShaderPath << std::endl;
	} else {
		//std::cout << "[GraphicsManager] Skybox shader loaded successfully - ID: " << skyboxShader->ID << std::endl;
	}

	//std::cout << "[GraphicsManager] Skybox initialized - VAO: " << skyboxVAO << ", VBO: " << skyboxVBO << std::endl;
}

void GraphicsManager::RunDepthPrepass(const glm::mat4& view, const glm::mat4& projection)
{
	if (!m_depthPrepassShader || !currentCamera) return;

	// Write depth only — no colour output needed
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	m_depthPrepassShader->Activate();
	m_depthPrepassShader->setMat4("view", view);
	m_depthPrepassShader->setMat4("projection", projection);
	m_depthPrepassShader->setBool("isAnimated", false);
	m_depthPrepassShader->setBool("hasDiffuseMap", false);

	// --- Pass 1: instanced opaque batches ---
	InstancingManager::GetInstance().RenderBatchesDepthPrepass(view, projection, *m_depthPrepassShader);

	// --- Pass 2: non-instanced opaque objects (e.g. animated meshes) ---
	// These were excluded from instancing by IsInstanceable(), so we handle them here.
	m_depthPrepassShader->setBool("useInstancing", false);

	for (const auto& renderItem : renderQueue)
	{
		if (!renderItem || renderItem->GetRenderKind() != RenderComponentKind::Model) continue;
		const ModelRenderComponent* modelItem = static_cast<const ModelRenderComponent*>(renderItem.get());
		if (!modelItem->isVisible || !modelItem->model) continue;

		// Skip transparent / fading objects — they need correct alpha blending, not prepass depth
		bool isTransparent = (modelItem->distanceFadeOpacity < 1.0f) ||
			(modelItem->material && modelItem->material->GetOpacity() < 1.0f);
		if (isTransparent) continue;

		// Skip objects that instancing already handled
		bool handledByInstancing = InstancingManager::GetInstance().IsEnabled() &&
			!modelItem->HasAnimation() &&
			modelItem->model->mBoneInfoMap.empty();
		if (handledByInstancing) continue;

		glm::mat4 modelMatrix = modelItem->transform.ConvertToGLM();

		// Frustum cull (same tolerance as main pass)
		if (frustumCullingEnabled)
		{
			AABB worldBBox = modelItem->model->GetBoundingBox().Transform(modelMatrix);
			if (!viewFrustum.IsBoxVisible(worldBBox, 0.5f)) continue;
		}

		m_depthPrepassShader->setMat4("model", modelMatrix);

		// Handle skeletal animation
		bool animated = modelItem->HasAnimation();
		m_depthPrepassShader->setBool("isAnimated", animated);
		if (animated && modelItem->animator)
		{
			const auto& transforms = modelItem->mFinalBoneMatrices;
			if (!transforms.empty())
				m_depthPrepassShader->setMat4Array("finalBonesMatrices[0]", transforms.data(), static_cast<GLsizei>(transforms.size()));
		}

		modelItem->model->DrawDepthOnly();
	}

	// Re-enable colour writes for the main colour pass
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void GraphicsManager::RenderSceneForShadows(Shader& depthShader)
{
	int count = 0;

	if (m_shadowFarPlane > 0.0f)
	{
		// ---------------------------------------------------------------
		// POINT LIGHT SHADOW PASS
		// PC:      Iterate ECS directly so objects just outside the camera
		//          frustum still cast shadows on visible geometry.
		// Android: Use the render queue (camera-frustum-culled) to keep
		//          fill cost low — pop-in is less noticeable on mobile.
		// ---------------------------------------------------------------
#ifndef ANDROID
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (!ecsManager.modelSystem)
			return;

		for (const auto& entity : ecsManager.modelSystem->entities)
		{
			if (!ecsManager.IsEntityActiveInHierarchy(entity))
				continue;

			auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
			if (!modelComp.isVisible || !modelComp.model)
				continue;

			glm::mat4 modelMatrix = ecsManager.GetComponent<Transform>(entity).worldMatrix.ConvertToGLM();

			AABB worldBBox = modelComp.model->GetBoundingBox().Transform(modelMatrix);
			float sqDist = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				float v = m_shadowLightPos[i];
				if (v < worldBBox.min[i]) sqDist += (worldBBox.min[i] - v) * (worldBBox.min[i] - v);
				if (v > worldBBox.max[i]) sqDist += (v - worldBBox.max[i]) * (v - worldBBox.max[i]);
			}
			if (sqDist > m_shadowFarPlane * m_shadowFarPlane)
				continue;

			count++;
			depthShader.setMat4("model", modelMatrix);
			depthShader.setBool("isAnimated", modelComp.HasAnimation());
			if (modelComp.HasAnimation() && modelComp.animator)
			{
				const auto& transforms = modelComp.mFinalBoneMatrices;
				if (!transforms.empty())
					depthShader.setMat4Array("finalBonesMatrices[0]", transforms.data(), static_cast<GLsizei>(transforms.size()));
			}
			modelComp.model->DrawDepthOnly();
		}
#else
		for (const auto& renderItem : renderQueue)
		{
			if (!renderItem || renderItem->GetRenderKind() != RenderComponentKind::Model)
				continue;

			const ModelRenderComponent* modelItem = static_cast<const ModelRenderComponent*>(renderItem.get());
			if (!modelItem->isVisible || !modelItem->model)
				continue;

			glm::mat4 modelMatrix = modelItem->transform.ConvertToGLM();

			AABB worldBBox = modelItem->model->GetBoundingBox().Transform(modelMatrix);
			float sqDist = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				float v = m_shadowLightPos[i];
				if (v < worldBBox.min[i]) sqDist += (worldBBox.min[i] - v) * (worldBBox.min[i] - v);
				if (v > worldBBox.max[i]) sqDist += (v - worldBBox.max[i]) * (v - worldBBox.max[i]);
			}
			if (sqDist > m_shadowFarPlane * m_shadowFarPlane)
				continue;

			count++;
			depthShader.setMat4("model", modelMatrix);
			depthShader.setBool("isAnimated", modelItem->HasAnimation());
			if (modelItem->HasAnimation() && modelItem->animator)
			{
				const auto& transforms = modelItem->mFinalBoneMatrices;
				if (!transforms.empty())
					depthShader.setMat4Array("finalBonesMatrices[0]", transforms.data(), static_cast<GLsizei>(transforms.size()));
			}
			modelItem->model->DrawDepthOnly();
		}
#endif
	}
	else
	{
		// ---------------------------------------------------------------
		// DIRECTIONAL SHADOW PASS (both platforms)
		// Use the camera-frustum-culled render queue. The directional shadow
		// map covers the camera view area, so objects outside the frustum
		// can't contribute to the visible shadow anyway.
		// ---------------------------------------------------------------
		for (const auto& renderItem : renderQueue)
		{
			if (!renderItem || renderItem->GetRenderKind() != RenderComponentKind::Model)
				continue;

			const ModelRenderComponent* modelItem = static_cast<const ModelRenderComponent*>(renderItem.get());
			if (!modelItem->isVisible || !modelItem->model)
				continue;

			count++;
			glm::mat4 modelMatrix = modelItem->transform.ConvertToGLM();
			depthShader.setMat4("model", modelMatrix);
			depthShader.setBool("isAnimated", modelItem->HasAnimation());
			if (modelItem->HasAnimation() && modelItem->animator)
			{
				const auto& transforms = modelItem->mFinalBoneMatrices;
				if (!transforms.empty())
					depthShader.setMat4Array("finalBonesMatrices[0]", transforms.data(), static_cast<GLsizei>(transforms.size()));
			}
			modelItem->model->DrawDepthOnly();
		}
	}

	// Also render instanced batches — they bypass the renderQueue so they'd otherwise
	// cast no shadows. The depth shader is already active and has light matrices set.
	if (InstancingManager::GetInstance().IsEnabled())
	{
		depthShader.setBool("useInstancing", true);
		depthShader.setBool("isAnimated", false);
		InstancingManager::GetInstance().RenderBatchesDepthOnly(glm::mat4(1.0f));
		depthShader.setBool("useInstancing", false);
	}

	// Debug
	//static bool once = false;
	//if (!once) {
	//	std::cout << "[Shadow Pass] Rendered " << count << " objects to shadow map" << std::endl;
	//	once = true;
	//}
}

void GraphicsManager::RenderSkybox()
{
	if (!currentCamera || !skyboxShader || skyboxVAO == 0) {
		//if (!checkedOnce) {
		//	std::cout << "[GraphicsManager] Skybox render skipped - camera: " << (currentCamera != nullptr)
		//		<< ", shader: " << (skyboxShader != nullptr) << ", VAO: " << skyboxVAO << std::endl;
		//	checkedOnce = true;
		//}
		return;
	}

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (!ecsManager.cameraSystem) {
		//if (!checkedOnce) {
		//	std::cout << "[GraphicsManager] Skybox render skipped - no camera system" << std::endl;
		//	checkedOnce = true;
		//}
		return;
	}

	Entity activeCameraEntity = ecsManager.cameraSystem->GetActiveCameraEntity();
	if (activeCameraEntity == UINT32_MAX || !ecsManager.HasComponent<CameraComponent>(activeCameraEntity)) {
		//if (!checkedOnce) {
		//	std::cout << "[GraphicsManager] Skybox render skipped - no active camera entity" << std::endl;
		//	checkedOnce = true;
		//}
		return;
	}

	auto& cameraComp = ecsManager.GetComponent<CameraComponent>(activeCameraEntity);
	if (!cameraComp.skyboxTexture) {
		//if (!checkedOnce) {
		//	std::cout << "[GraphicsManager] Skybox render skipped - no skybox texture assigned" << std::endl;
		//	checkedOnce = true;
		//}
		return;
	}

	//static bool logged = false;
	//if (!logged) {
	//	std::cout << "[GraphicsManager] Rendering skybox - Texture ID: " << cameraComp.skyboxTexture->ID
	//		<< ", Viewport: " << viewportWidth << "x" << viewportHeight << std::endl;
	//	logged = true;
	//}

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	skyboxShader->Activate();

	if (!skyboxShader->UsesCameraBlock()) {
		glm::mat4 view = glm::mat4(glm::mat3(currentCamera->GetViewMatrix()));

		float aspectRatio = (float)viewportWidth / (float)viewportHeight;
		glm::mat4 projection = glm::perspective(
			glm::radians(currentCamera->Zoom),
			aspectRatio,
			0.1f, m_farPlane
		);

		skyboxShader->setMat4("view", view);
		skyboxShader->setMat4("projection", projection);
	}

	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, cameraComp.skyboxTexture->ID);
	skyboxShader->setInt("skyboxTexture", 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDepthMask(GL_TRUE);
	if (faceCullingEnabled) glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
}

void GraphicsManager::SetFaceCulling(bool enabled)
{
	faceCullingEnabled = enabled;
	if (enabled)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}

void GraphicsManager::SetCullMode(CullMode mode)
{
	cullMode = mode;
	GLenum glMode = GL_BACK;
	switch (mode)
	{
	case CullMode::BACK: glMode = GL_BACK; break;
	case CullMode::FRONT: glMode = GL_FRONT; break;
	case CullMode::FRONT_AND_BACK: glMode = GL_FRONT_AND_BACK; break;
	}
	glCullFace(glMode);
}

void GraphicsManager::SetFrontFace(FrontFace face)
{
	frontFace = face;
	GLenum glFace = GL_CCW;
	if (face == FrontFace::CW) glFace = GL_CW;
	glFrontFace(glFace);
}

void GraphicsManager::RenderModelOptimized(const ModelRenderComponent& item)
{
	if (!item.isVisible || !item.model || !item.shader) {
		return;
	}

	m_sortingStats.totalObjects++;

	// Frustum culling (your existing code)
	glm::mat4 modelMatrix = item.transform.ConvertToGLM();

	if (frustumCullingEnabled && currentCamera) {
		AABB worldBBox = item.model->GetBoundingBox().Transform(modelMatrix);
		if (!viewFrustum.IsBoxVisible(worldBBox, 0.5f)) {
			return;
		}
	}

	// =========================================================================
	// OPTIMIZED STATE MANAGEMENT - only switch if different
	// =========================================================================

	Shader* shader = item.shader.get();
	Material* material = item.material.get();

	// Switch shader only if different
	if (shader != m_currentShader) {
		PROFILE_SCOPED("GM::ShaderSwitch+Lighting");
		shader->Activate();
		m_currentShader = shader;
		// Uniform and sampler state belongs to the shader program. Force the
		// current material to bind when switching programs even if its pointer is
		// the same as the previous draw.
		m_currentMaterial = nullptr;
		m_sortingStats.shaderSwitches++;
		shader->setBool("useInstancing", false);

		// Set view/projection (only need to do this on shader switch)
		SetupMatrices(*shader, modelMatrix, true);

		// Apply lighting (only on shader switch)
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (ecsManager.lightingSystem) {
			ecsManager.lightingSystem->ApplyLighting(*shader);
			ecsManager.lightingSystem->ApplyShadows(*shader);
		}

		// Environment reflections (skybox bound to texture unit 12)
		shader->setBool("hasEnvMap", envReflectionActive);
		if (envReflectionActive) {
			shader->setInt("envMap", 12);
			shader->setFloat("envReflectionIntensity", envReflectionIntensityValue);
		}
	}
	else {
		// Same shader - just update model matrix
		shader->setMat4("model", modelMatrix);
		glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));
		shader->setMat3("normalMatrixCPU", normalMatrix);
	}

	// Per-entity bloom emission (must set per-model to avoid stale values)
	shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		shader->setVec3("bloomColor", item.bloomColor);
	} else {
		shader->setVec3("bloomColor", glm::vec3(0.0f));
	}

	// Entity-level materials are applied here. Models using their imported
	// per-mesh materials continue the same state cache inside DrawFast.
	if (material && material != m_currentMaterial) {
		PROFILE_SCOPED("GM::MaterialBind");
		material->ApplyToShader(*shader);
		m_currentMaterial = material;
		m_sortingStats.materialSwitches++;
	}

	// Pass fade opacity to shader — multiplied into final alpha for smooth blending
	shader->setFloat("u_distanceFadeOpacity", item.distanceFadeOpacity);

	// Per-entity brightness boost (e.g. player stands out against environment)
	shader->setFloat("brightnessBoost", item.brightnessBoost);

	// Draw the model
	{
		PROFILE_SCOPED("GM::ModelDraw");
		if (item.depthOffset)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(item.depthOffsetFactor, item.depthOffsetUnits);
		}

		item.model->DrawFast(*shader, item.material, item, m_currentMaterial, item.animator);

		if (item.depthOffset)
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
	}

	m_sortingStats.drawCalls++;
}

void GraphicsManager::RenderFogVolume(const FogVolumeComponent& item)
{
	if (!item.isVisible || !item.fogShader || !item.fogVAO) 
	{
		return;
	}

	// --- Blending setup ---
	glDisable(GL_DEPTH_TEST);       // Depth handled in shader via depth texture
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);           // Render back faces only for volumetric ray-box effect
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	item.fogShader->Activate();

	// --- Transform (uses worldTransform set by FogSystem) ---
	glm::mat4 modelMatrix = item.worldTransform.ConvertToGLM();
	item.fogShader->setMat4("model", modelMatrix);
	item.fogShader->setMat4("modelInverse", glm::inverse(modelMatrix));

	// --- Camera matrices ---
	const float nearP = 0.1f;
	const float farP  = m_farPlane;
	if (currentCamera)
	{
		float aspectRatio = currentFrameViewport.aspectRatio;
		glm::mat4 view = currentCamera->GetViewMatrix();
		glm::mat4 projection = glm::perspective(
			glm::radians(currentCamera->Zoom),
			aspectRatio,
			nearP, farP
		);
		if (!item.fogShader->UsesCameraBlock()) {
			item.fogShader->setMat4("view", view);
			item.fogShader->setMat4("projection", projection);
			item.fogShader->setVec3("cameraPos", currentCamera->Position);
		}
		item.fogShader->setMat4("inverseView", glm::inverse(view));
		item.fogShader->setMat4("inverseProjection", glm::inverse(projection));
		item.fogShader->setFloat("nearPlane", nearP);
		item.fogShader->setFloat("farPlane",  farP);
		item.fogShader->setVec2("viewportSize",
			glm::vec2(static_cast<float>(currentFrameViewport.width),
			          static_cast<float>(currentFrameViewport.height)));
	}

	// --- Scene depth texture for soft intersection with solid geometry ---
	unsigned int depthTex = PostProcessingManager::GetInstance().GetHDRDepthTexture();
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	item.fogShader->setInt("depthTexture", 1);

	// --- Fog properties (all from FogVolumeComponent) ---
	item.fogShader->setInt("fogShape", static_cast<int>(item.shape));
	item.fogShader->setVec3("fogColor", item.fogColor.ConvertToGLM());
	item.fogShader->setFloat("density", item.density);
	item.fogShader->setFloat("opacity", item.opacity);

	// --- Time for noise animation ---
	static float fogTime = 0.0f;
	fogTime += static_cast<float>(TimeManager::GetDeltaTime());
	item.fogShader->setFloat("time", fogTime);

	item.fogShader->setFloat("scrollSpeedX", item.scrollSpeedX);
	item.fogShader->setFloat("scrollSpeedY", item.scrollSpeedY);
	item.fogShader->setFloat("noiseScale", item.noiseScale);
	item.fogShader->setFloat("noiseStrength", item.noiseStrength);
	item.fogShader->setFloat("warpStrength", item.warpStrength);

	// --- Height fade ---
	item.fogShader->setBool("useHeightFade", item.useHeightFade);
	item.fogShader->setFloat("heightFadeStart", item.heightFadeStart);
	item.fogShader->setFloat("heightFadeEnd", item.heightFadeEnd);

	// --- Edge softness ---
	item.fogShader->setFloat("edgeSoftness", item.edgeSoftness);

	// --- Noise texture ---
	bool hasNoiseMap = (item.noiseTexture != nullptr);
	item.fogShader->setBool("hasNoiseMap", hasNoiseMap);
	item.fogShader->setInt("noiseTextureMappingAxis", item.noiseTextureMappingAxis);
	if (hasNoiseMap)
	{
		glActiveTexture(GL_TEXTURE0);
		item.noiseTexture->Bind(0);
		item.fogShader->setInt("noiseMap", 0);
	}

	// --- Color/material texture ---
	bool hasColorMap = (item.colorTexture != nullptr);
	item.fogShader->setBool("hasColorMap", hasColorMap);
	item.fogShader->setFloat("colorTextureIntensity", item.colorTextureIntensity);
	item.fogShader->setFloat("colorTextureScale", item.colorTextureScale);
	if (hasColorMap)
	{
		glActiveTexture(GL_TEXTURE2);
		item.colorTexture->Bind(2);
		item.fogShader->setInt("colorMap", 2);
	}

	// --- Draw ---
	item.fogVAO->Bind();
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	item.fogVAO->Unbind();

	// --- Restore state ---
	if (hasColorMap) {
		item.colorTexture->Unbind(2);
	}
	if (hasNoiseMap) {
		item.noiseTexture->Unbind(0);
	}
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glCullFace(GL_BACK);
	if (faceCullingEnabled) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}
