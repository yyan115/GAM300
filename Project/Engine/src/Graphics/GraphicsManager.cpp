#include "pch.h"
#include "Graphics/GraphicsManager.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#include <cstring>

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
	glm::mat3 ComputeNormalMatrix(const glm::mat4& modelMatrix)
	{
		const glm::mat3 model3(modelMatrix);
		const float scaleX2 = glm::dot(model3[0], model3[0]);
		const float scaleY2 = glm::dot(model3[1], model3[1]);
		const float scaleZ2 = glm::dot(model3[2], model3[2]);
		const float tolerance = glm::max(1e-6f, scaleX2 * 1e-4f);

		if (glm::abs(scaleX2 - scaleY2) < tolerance &&
			glm::abs(scaleX2 - scaleZ2) < tolerance &&
			glm::abs(glm::dot(model3[0], model3[1])) < tolerance &&
			glm::abs(glm::dot(model3[0], model3[2])) < tolerance &&
			glm::abs(glm::dot(model3[1], model3[2])) < tolerance &&
			scaleX2 > 1e-8f) {
			// Shader outputs are normalized, so uniform scale magnitude is irrelevant.
			return model3 * glm::inversesqrt(scaleX2);
		}

		return glm::transpose(glm::inverse(model3));
	}

	bool NeedsBlending(const ModelRenderComponent& model)
	{
		if (model.distanceFadeOpacity < 1.0f) {
			return true;
		}
		if (model.material) {
			return model.material->GetOpacity() < 1.0f;
		}
		if (model.model) {
			for (const auto& mesh : model.model->meshes) {
				if (mesh.material && mesh.material->GetOpacity() < 1.0f) {
					return true;
				}
			}
		}
		return false;
	}

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
#ifdef ANDROID
	InitBonePaletteUBO();
#endif

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
	m_hasLastCameraUBOData = false;

#ifdef ANDROID
	if (m_bonePaletteUBO != 0) {
		glDeleteBuffers(1, &m_bonePaletteUBO);
		m_bonePaletteUBO = 0;
	}
	m_bonePaletteScratch.clear();
	m_bonePaletteUploadSize = 0;
	m_boundBonePaletteOffset = std::numeric_limits<std::size_t>::max();
#endif

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
	Shader::ResetActiveProgramCache();
	VAO::ResetBindingCache();
	ResetBloomFlag();

	// Reset state tracking
	m_currentShader = nullptr;
	m_currentMaterial = nullptr;
	m_sortingStats.Reset();
	m_bloomTargetPrepared = false;
	m_bloomOutputEnabled = false;
#ifdef ANDROID
	m_boundBonePaletteOffset = std::numeric_limits<std::size_t>::max();
#endif

	if (InstancingManager::GetInstance().IsEnabled())
	{
		InstancingManager::GetInstance().BeginFrame();
		// Both game and editor draw paths update this same Frustum object after
		// selecting their camera and before systems submit instances.
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

void GraphicsManager::Submit(IRenderComponent* renderItem)
{
	std::lock_guard<std::mutex> lock(renderQueueMutex);
	if (renderItem && renderItem->isVisible)
	{
		renderQueue.push_back(renderItem);
	}
}

void GraphicsManager::InvalidateRenderStateCache() noexcept
{
	m_cachedRenderState = {};
	m_texture2DUnit0Known = false;
}

void GraphicsManager::SetDepthTestCached(bool enabled)
{
	const std::int8_t desired = enabled ? 1 : 0;
	if (m_cachedRenderState.depthTest == desired) return;

	if (enabled) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);
	m_cachedRenderState.depthTest = desired;
}

void GraphicsManager::SetDepthWriteCached(bool enabled)
{
	const std::int8_t desired = enabled ? 1 : 0;
	if (m_cachedRenderState.depthWrite == desired) return;

	glDepthMask(enabled ? GL_TRUE : GL_FALSE);
	m_cachedRenderState.depthWrite = desired;
}

void GraphicsManager::SetBlendCached(bool enabled)
{
	const std::int8_t desired = enabled ? 1 : 0;
	if (m_cachedRenderState.blend == desired) return;

	if (enabled) glEnable(GL_BLEND);
	else glDisable(GL_BLEND);
	m_cachedRenderState.blend = desired;
}

void GraphicsManager::SetCullFaceCached(bool enabled)
{
	const std::int8_t desired = enabled ? 1 : 0;
	if (m_cachedRenderState.cullFace == desired) return;

	if (enabled) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);
	m_cachedRenderState.cullFace = desired;
}

void GraphicsManager::SetBlendFunctionCached(GLenum source, GLenum destination)
{
	if (m_cachedRenderState.blendFunctionKnown &&
		m_cachedRenderState.blendSource == source &&
		m_cachedRenderState.blendDestination == destination) {
		return;
	}

	glBlendFunc(source, destination);
	m_cachedRenderState.blendSource = source;
	m_cachedRenderState.blendDestination = destination;
	m_cachedRenderState.blendFunctionKnown = true;
}

void GraphicsManager::BindTexture2DUnit0Cached(GLuint texture)
{
	if (m_texture2DUnit0Known && m_cachedTexture2DUnit0 == texture) {
		return;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	m_cachedTexture2DUnit0 = texture;
	m_texture2DUnit0Known = true;
}

void GraphicsManager::SetBloomOutputEnabled(bool enabled)
{
	const bool desired = m_bloomTargetPrepared && enabled;
	if (m_bloomOutputEnabled == desired) {
		return;
	}

	auto& postProcessing = PostProcessingManager::GetInstance();
	if (desired) {
		postProcessing.EnableBloomMRT();
	}
	else {
		postProcessing.DisableBloomMRT();
	}
	m_bloomOutputEnabled = desired;
}

void GraphicsManager::RestoreDefaultRenderState()
{
	SetDepthTestCached(true);
	SetDepthWriteCached(true);
	SetBlendCached(false);
	SetCullFaceCached(faceCullingEnabled);
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

	// BeginFrame reset the bloom flag before the parallel render-preparation
	// systems populated it. Do not clear it here before the post-process check.

	currentFrameViewport = GetCurrentViewport();

	// Compute view/projection once for the whole frame and upload to Camera UBO.
	// All shaders that declare CameraBlock automatically receive these values.
	m_frameView = currentCamera->GetViewMatrix();
	m_frameProjection = glm::perspective(
		glm::radians(currentCamera->Zoom),
		currentFrameViewport.aspectRatio,
		0.1f, m_farPlane
	);
	const glm::vec3 cameraRight = glm::cross(currentCamera->Front, currentCamera->Up);
	const float cameraRightLengthSq = glm::dot(cameraRight, cameraRight);
	m_frameCameraRight = cameraRightLengthSq > 1e-8f
		? cameraRight * glm::inversesqrt(cameraRightLengthSq)
		: glm::vec3(1.0f, 0.0f, 0.0f);
	m_frameCameraUp = currentCamera->Up;

	const glm::mat4& frameView = m_frameView;
	const glm::mat4& frameProjection = m_frameProjection;
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

#ifdef ANDROID
	{
		PROFILE_SCOPED("GM::BonePalettes");
		PrepareBonePalettes();
	}
#endif

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
				deferredQueue.push_back(item);
				item = nullptr;
				continue;
			}
			if (item->GetRenderKind() == RenderComponentKind::Model)
			{
				m_modelRenderItems.push_back(item);
			}
			else
			{
				m_otherRenderItems.push_back(item);
			}
		}
	}
	// Enable MRT so bloom-capable shaders can write to the bloom emission texture
	{
		PROFILE_SCOPED("GM::EnableBloomMRT");
		auto& postProcessing = PostProcessingManager::GetInstance();
		BloomEffect* bloom = postProcessing.GetBloomEffect();
		const bool needsBloomMRT =
			bloom &&
			bloom->IsEnabled() &&
			bloom->GetIntensity() > 0.01f &&
			HasBloomEmissionThisFrame();

		m_bloomTargetPrepared = needsBloomMRT;
		m_bloomOutputEnabled = false;
		if (needsBloomMRT) {
			postProcessing.PrepareBloomMRT();
		}
		else {
			postProcessing.DisableBloomMRT();
		}
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
			entry.layer = NeedsBlending(*model)
				? RenderLayer::Type::LAYER_TRANSPARENT
				: RenderLayer::Type::LAYER_OPAQUE;

			const Vector3D pos = Matrix4x4::ExtractTranslation(model->transform);
			const float dx = pos.x - camPos.x;
			const float dy = pos.y - camPos.y;
			const float dz = pos.z - camPos.z;
			entry.distanceSq = dx * dx + dy * dy + dz * dz;

			if (entry.layer == RenderLayer::Type::LAYER_OPAQUE)
			{
				entry.bloomOutput = m_bloomTargetPrepared &&
					model->bloomIntensity > 0.01f;
				// Front-to-back: group into ~5-unit buckets so objects at similar
				// depths still batch by state (reduces shader/material switches).
#ifdef ANDROID
				entry.depthBucket = static_cast<int>(
					glm::sqrt(entry.distanceSq) * 0.2f);
#else
				// Desktop already has a depth prepass, so state grouping is more
				// useful than front-to-back ordering for the color pass.
				entry.depthBucket = 0;
#endif
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
#ifdef ANDROID
					const uint64_t shaderA =
						a.stateKey.key & RenderSortKey::SHADER_MASK;
					const uint64_t shaderB =
						b.stateKey.key & RenderSortKey::SHADER_MASK;
					if (shaderA != shaderB)
						return shaderA < shaderB;
					if (a.bloomOutput != b.bloomOutput)
						return !a.bloomOutput;
#endif
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
		bool skyboxRendered = false;
		for (IRenderComponent* item : m_modelRenderItems)
		{
			ModelRenderComponent* modelItem = static_cast<ModelRenderComponent*>(item);
			// ModelSystem submits only the non-instanced fallback path here.
			const bool isTransparent = NeedsBlending(*modelItem);

			// Draw the skybox after opaque geometry has populated depth, but
			// before the first blended object. Its expensive spherical texture
			// lookup then runs only for uncovered background pixels.
			if (isTransparent && !skyboxRendered)
			{
				PROFILE_SCOPED("GM::Skybox");
				SetBloomOutputEnabled(false);
				RenderSkybox();
				InvalidateRenderStateCache();
				skyboxRendered = true;
			}

			SetBloomOutputEnabled(modelItem->bloomIntensity > 0.01f);

			// Enable alpha blending when material opacity or distance fade opacity < 1.
			const bool needsBlend = isTransparent;
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

		if (!skyboxRendered)
		{
			PROFILE_SCOPED("GM::Skybox");
			SetBloomOutputEnabled(false);
			RenderSkybox();
			InvalidateRenderStateCache();
		}
	}

	// =========================================================================
	// Render other items (sprites, text, particles, debug)
	// =========================================================================
	{
		PROFILE_SCOPED("GM::OtherItemsRender");
		InvalidateRenderStateCache();
		for (IRenderComponent* item : m_otherRenderItems) {
			switch (item->GetRenderKind())
			{
			case RenderComponentKind::Text:
				SetBloomOutputEnabled(item->bloomIntensity > 0.01f);
				RenderText(*static_cast<TextRenderComponent*>(item));
				break;
			case RenderComponentKind::TextRenderItem:
				SetBloomOutputEnabled(item->bloomIntensity > 0.01f);
				RenderText(*static_cast<TextRenderItem*>(item));
				break;
			case RenderComponentKind::Sprite:
				SetBloomOutputEnabled(item->bloomIntensity > 0.01f);
				RenderSprite(*static_cast<SpriteRenderComponent*>(item));
				break;
			case RenderComponentKind::SpriteRenderItem:
				SetBloomOutputEnabled(item->bloomIntensity > 0.01f);
				RenderSprite(*static_cast<SpriteRenderItem*>(item));
				break;
			case RenderComponentKind::DebugDraw:
				SetBloomOutputEnabled(false);
				RestoreDefaultRenderState();
				RenderDebugDraw(*static_cast<DebugDrawComponent*>(item));
				InvalidateRenderStateCache();
				break;
			case RenderComponentKind::Particle:
				SetBloomOutputEnabled(item->bloomIntensity > 0.01f);
				RenderParticles(*static_cast<ParticleComponent*>(item));
				break;
			case RenderComponentKind::ParticleRenderItem:
				SetBloomOutputEnabled(item->bloomIntensity > 0.01f);
				RenderParticles(*static_cast<ParticleRenderItem*>(item));
				break;
			case RenderComponentKind::Fog:
				SetBloomOutputEnabled(false);
#ifndef ANDROID
				RestoreDefaultRenderState();
				RenderFogVolume(*static_cast<FogVolumeComponent*>(item));
				InvalidateRenderStateCache();
#endif
				break;
			default:
				break;
			}
		}
		RestoreDefaultRenderState();
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
		SetBloomOutputEnabled(false);
		m_bloomTargetPrepared = false;
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
			m_deferredModelRenderItems.push_back(item);
		else
			m_deferredOtherRenderItems.push_back(item);
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

	InvalidateRenderStateCache();
	for (IRenderComponent* item : m_deferredOtherRenderItems)
	{
		switch (item->GetRenderKind())
		{
		case RenderComponentKind::Text:
			RenderText(*static_cast<TextRenderComponent*>(item));
			break;
		case RenderComponentKind::TextRenderItem:
			RenderText(*static_cast<TextRenderItem*>(item));
			break;
		case RenderComponentKind::Sprite:
			RenderSprite(*static_cast<SpriteRenderComponent*>(item));
			break;
		case RenderComponentKind::SpriteRenderItem:
			RenderSprite(*static_cast<SpriteRenderItem*>(item));
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
	RestoreDefaultRenderState();

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
	SetupMatrices(*item.shader, modelMatrix, true);

	// Per-entity bloom emission
	item.shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		item.shader->setVec3("bloomColor", item.bloomColor);
	}

	// Per-entity brightness boost
	item.shader->setFloat("brightnessBoost", item.brightnessBoost);

	// Per-entity fade opacity
	item.shader->setFloat("u_distanceFadeOpacity", item.distanceFadeOpacity);

#ifdef __ANDROID__
	item.shader->setInt(
		"u_lightMask", static_cast<int>(item.lightMask));
#endif

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
	{
#ifdef ANDROID
		BindBonePalette(item);
#endif
		item.model->Draw(*item.shader, *currentCamera, item.material, item, item.animator);
	}
	else
	{
#ifdef ANDROID
		if (!item.model->mBoneInfoMap.empty() &&
			!item.GetRenderBoneMatrices().empty()) {
			BindBonePalette(item);
		}
#endif
		item.model->Draw(*item.shader, *currentCamera, item.material, item);
	}

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
		shader.setMat3("normalMatrixCPU", ComputeNormalMatrix(modelMatrix));
	}

	if (currentCamera && !shader.UsesCameraBlock())
	{
		// In 2D editor mode, use orthographic projection with screen-space coordinates
		if (IsRenderingForEditor() && Is2DMode()) {
			const int renderWidth = currentFrameViewport.width;
			const int renderHeight = currentFrameViewport.height;
			const glm::mat4 view(1.0f);

			// Use identity view matrix for 2D (camera doesn't rotate)
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

			const glm::mat4 projection =
				glm::ortho(left, right, bottom, top, -1000.0f, 1000.0f);
			shader.setMat4("view", view);
			shader.setMat4("projection", projection);
		}
		else {
			shader.setMat4("view", m_frameView);
			shader.setMat4("projection", m_frameProjection);
		}

		shader.setVec3("cameraPos", currentCamera->Position);
	}
}

void GraphicsManager::RenderText(const TextRenderComponent& item)
{
	const TextRenderItem snapshot(item);
	RenderText(snapshot);
}

void GraphicsManager::RenderText(const TextRenderItem& item)
{
	if (!item.isVisible || !item.font || !item.shader || !item.text ||
		!item.wrappedLines || item.text->empty())
	{
		return;
	}
	const std::string& text = *item.text;
	const std::vector<std::string>& wrappedLines = *item.wrappedLines;

	if (!item.is3D && IsRenderingForEditor() && !Is2DMode())
	{
		return;
	}

	// Configure depth testing based on 2D/3D mode
	if (item.is3D) {
		SetDepthTestCached(true);
		SetDepthWriteCached(false);
	}
	else {
		SetDepthTestCached(false);
		SetDepthWriteCached(true);
	}

	// Enable blending for text transparency
	SetBlendCached(true);
	SetBlendFunctionCached(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	SetCullFaceCached(faceCullingEnabled);

	// Activate shader and set uniforms
	item.shader->Activate();
	glm::vec4 textColorWithAlpha = glm::vec4(item.color.ConvertToGLM(), item.alpha);
	item.shader->setVec4("textColor", textColorWithAlpha);

	// Per-entity bloom emission
#ifndef ANDROID
	item.shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		item.shader->setVec3("bloomColor", item.bloomColor);
	}
#endif

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
	item.shader->setInt("text", 0);
	VAO* fontVAO = item.font->GetVAO();
	VBO* fontVBO = item.font->GetVBO();

	if (!fontVAO || !fontVBO)
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GraphicsManager] Font VAO/VBO not initialized!\n");
		return;
	}

	fontVAO->Bind();
	m_textVertexScratch.clear();
	m_textVertexScratch.reserve(text.size() * 6);

	// Calculate scale factors
	float worldScaleFactor = item.is3D ? 0.01f : 1.0f;
	float scaleX = item.is3D ? worldScaleFactor : (item.transformScale.x * worldScaleFactor);
	float scaleY = item.is3D ? worldScaleFactor : (item.transformScale.y * worldScaleFactor);

	// Get line height for multi-line rendering
	float lineHeight = item.font->GetTextHeight(scaleY) * item.lineSpacing;

	// Use pre-computed wrapped lines without allocating a temporary vector for
	// the normal single-line fallback.
	const bool useSingleLineFallback = wrappedLines.empty();
	const std::size_t lineCount =
		useSingleLineFallback ? 1 : wrappedLines.size();

	// Starting Y position (top of text block)
	float startY = 0.0f;

	// Render each line
	for (size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
	{
		const std::string& line =
			useSingleLineFallback ? text : wrappedLines[lineIndex];

		// Calculate X starting position based on alignment for this line
		float x = 0.0f;
		if (item.alignment == TextRenderComponent::Alignment::CENTER)
		{
			x = -item.font->GetTextWidth(line, scaleX) / 2.0f;
		}
		else if (item.alignment == TextRenderComponent::Alignment::RIGHT)
		{
			x = -item.font->GetTextWidth(line, scaleX);
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

			if (w > 0.0f && h > 0.0f) {
				const float u0 = ch.uvMin.x;
				const float v0 = ch.uvMin.y;
				const float u1 = ch.uvMax.x;
				const float v1 = ch.uvMax.y;

				m_textVertexScratch.emplace_back(xpos,     ypos + h, u0, v0);
				m_textVertexScratch.emplace_back(xpos,     ypos,     u0, v1);
				m_textVertexScratch.emplace_back(xpos + w, ypos,     u1, v1);
				m_textVertexScratch.emplace_back(xpos,     ypos + h, u0, v0);
				m_textVertexScratch.emplace_back(xpos + w, ypos,     u1, v1);
				m_textVertexScratch.emplace_back(xpos + w, ypos + h, u1, v0);
			}

			x += (ch.advance >> 6) * scaleX;
		}
	}

	if (!m_textVertexScratch.empty()) {
		item.font->EnsureTextVertexCapacity(m_textVertexScratch.size());
		fontVBO->Bind();
		BindTexture2DUnit0Cached(item.font->GetAtlasTexture());
		fontVBO->UpdateBoundData(
			m_textVertexScratch.data(),
			m_textVertexScratch.size() * sizeof(glm::vec4));
		PROFILE_COUNT("GL::DrawCalls", 1);
		glDrawArrays(
			GL_TRIANGLES,
			0,
			static_cast<GLsizei>(m_textVertexScratch.size()));
	}

}

void GraphicsManager::Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scaleX, float scaleY)
{
	// Use target game resolution for 2D projection
	// This ensures Scene Panel and Game Panel show text at consistent positions
	static const glm::mat4 identity(1.0f);

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position);  // Use position as-is
	model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

	shader.setMat4("projection", GetScreenProjection());
	shader.setMat4("view", identity);
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
			PROFILE_COUNT("GL::DrawCalls", 1);
			glDrawArrays(GL_LINES, 0, indexCount);
		}
		else
		{
			PROFILE_COUNT("GL::DrawCalls", 1);
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
		item.particles.size(),
		item.additiveBlending,
		glm::vec4(item.startColor.ConvertToGLM(), item.startColorAlpha),
		glm::vec4(item.endColor.ConvertToGLM(), item.endColorAlpha),
		item.startSize,
		item.endSize);
}

void GraphicsManager::RenderParticles(const ParticleRenderItem& item)
{
	RenderParticleInstances(
		item,
		item.particleTexture,
		item.particleShader,
		item.particleVAO,
		item.particleCount,
		item.additiveBlending,
		item.startColor,
		item.endColor,
		item.startSize,
		item.endSize);
}

void GraphicsManager::RenderParticleInstances(
	const IRenderComponent& renderState,
	const std::shared_ptr<Texture>& texture,
	const std::shared_ptr<Shader>& shader,
	VAO* vao,
	std::size_t particleCount,
	bool additiveBlending,
	const glm::vec4& startColor,
	const glm::vec4& endColor,
	float startSize,
	float endSize)
{
#if defined(ANDROID) && defined(GAM300_GL_VALIDATION)
	assert(eglGetCurrentContext() != EGL_NO_CONTEXT);
#endif
	if (!renderState.isVisible || particleCount == 0 || !shader || !vao) return;
	SetDepthTestCached(true);
	SetCullFaceCached(false);
	SetBlendCached(true);
	if (additiveBlending)
		SetBlendFunctionCached(GL_SRC_ALPHA, GL_ONE);              // Additive: glow/fire/magic
	else
		SetBlendFunctionCached(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // Standard alpha: physical/solid
	SetDepthWriteCached(false);

	shader->Activate();

	// Setup camera matrices ONCE for all particles
	if (currentCamera) {
		if (!shader->UsesCameraBlock()) {
			shader->setMat4("view", m_frameView);
			shader->setMat4("projection", m_frameProjection);
		}

		// Send camera vectors for billboard calculations in vertex shader
		shader->setVec3("cameraRight", m_frameCameraRight);
		shader->setVec3("cameraUp", m_frameCameraUp);
	}

#ifdef ANDROID
	shader->setVec4("particleStartColor", startColor);
	shader->setVec4("particleEndColor", endColor);
	shader->setFloat("particleStartSize", startSize);
	shader->setFloat("particleEndSize", endSize);
#else
	(void)startColor;
	(void)endColor;
	(void)startSize;
	(void)endSize;
#endif

	// Bind texture if available
	if (texture) {
		BindTexture2DUnit0Cached(texture->ID);
		shader->setInt("particleTexture", 0);
	}

	// Per-entity bloom emission
	shader->setFloat("bloomIntensity", renderState.bloomIntensity);
	if (renderState.bloomIntensity > 0.0f) {
		shader->setVec3("bloomColor", renderState.bloomColor);
	}

	// Draw ALL particles with ONE instanced draw call using indices
	vao->Bind();
#if defined(GAM300_GL_VALIDATION) || (!defined(NDEBUG) && !defined(ANDROID))
	GLint eboBinding = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eboBinding);
	assert(eboBinding != 0 && "VAO has no EBO bound after setup");
#endif

	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(particleCount));
}

void GraphicsManager::RenderSprite(const SpriteRenderComponent& item)
{
	const SpriteRenderItem snapshot(item);
	RenderSprite(snapshot);
}

void GraphicsManager::RenderSprite(const SpriteRenderItem& item)
{
	if (!item.isVisible || !item.texture || !item.shader || !item.spriteVAO)
	{
		return;
	}

	// Configure depth testing based on 2D/3D mode
	if (item.is3D) {
		// 3D sprite: enable depth testing
		SetDepthTestCached(true);
	}
	else {
		// 2D sprite: disable depth testing so render order determines what's on top
		SetDepthTestCached(false);
	}
	SetDepthWriteCached(true);

	// Enable blending for sprite transparency
	SetBlendCached(true);
	SetBlendFunctionCached(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	SetCullFaceCached(false);
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
#ifndef ANDROID
	item.shader->setFloat("bloomIntensity", item.bloomIntensity);
	if (item.bloomIntensity > 0.0f) {
		item.shader->setVec3("bloomColor", item.bloomColor);
	}
#endif

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
	BindTexture2DUnit0Cached(item.texture->ID);
	item.shader->setInt("spriteTexture", 0);

	item.spriteVAO->Bind();
	//item.spriteEBO->Bind();

#if defined(GAM300_GL_VALIDATION) || (!defined(NDEBUG) && !defined(ANDROID))
	GLint ebo = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
	if (ebo == 0) {
		ENGINE_LOG_ERROR("VAO " + std::to_string(item.spriteVAO->ID) + " has no EBO bound!");
	}
#endif

	// The SpriteSystem should have already bound the VAO, so just draw
	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void GraphicsManager::Setup2DSpriteMatrices(Shader& shader, const glm::vec3& position, const glm::vec3& scale, float rotation)
{
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
	static const glm::mat4 identity(1.0f);
	shader.setMat4("projection", GetScreenProjection());
	shader.setMat4("model", model);
	shader.setMat4("view", identity);
}

const glm::mat4& GraphicsManager::GetScreenProjection()
{
	if (m_screenProjectionWidth != targetGameWidth ||
		m_screenProjectionHeight != targetGameHeight) {
		const int width = std::max(targetGameWidth, 1);
		const int height = std::max(targetGameHeight, 1);
		m_screenProjection = glm::ortho(
			0.0f,
			static_cast<float>(width),
			0.0f,
			static_cast<float>(height));
		m_screenProjectionWidth = targetGameWidth;
		m_screenProjectionHeight = targetGameHeight;
	}
	return m_screenProjection;
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
	m_hasLastCameraUBOData = false;
	if (m_cameraUBO == 0) {
		glGenBuffers(1, &m_cameraUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBOData), nullptr, GL_DYNAMIC_DRAW);
	}
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GraphicsManager::UploadCameraUBO(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos)
{
	CameraUBOData data{};
	data.view = view;
	data.projection = projection;
	data.cameraPos = camPos;
	data.viewProjection = projection * view;
	if (m_hasLastCameraUBOData &&
		std::memcmp(&data, &m_lastCameraUBOData, sizeof(data)) == 0) {
		return;
	}

	glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBOData), &data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	m_lastCameraUBOData = data;
	m_hasLastCameraUBOData = true;
}

#ifdef ANDROID
void GraphicsManager::InitBonePaletteUBO()
{
	if (m_bonePaletteUBO == 0) {
		glGenBuffers(1, &m_bonePaletteUBO);
	}

	GLint alignment = 1;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
	const std::size_t safeAlignment =
		static_cast<std::size_t>(std::max(alignment, 1));
	m_bonePaletteStride =
		((kBonePaletteBytes + safeAlignment - 1) / safeAlignment) *
		safeAlignment;
}

void GraphicsManager::PrepareBonePalettes()
{
	m_bonePaletteScratch.clear();
	m_bonePaletteUploadSize = 0;
	m_boundBonePaletteOffset = std::numeric_limits<std::size_t>::max();

	if (m_bonePaletteUBO == 0) {
		return;
	}

	for (IRenderComponent* renderItem : renderQueue) {
		if (!renderItem ||
			renderItem->GetRenderKind() != RenderComponentKind::Model) {
			continue;
		}

		auto& modelItem = static_cast<ModelRenderComponent&>(*renderItem);
		modelItem.bonePaletteOffset =
			std::numeric_limits<std::size_t>::max();
		if (!modelItem.model || !modelItem.shader ||
			!modelItem.shader->UsesBonesBlock() ||
			modelItem.model->mBoneInfoMap.empty()) {
			continue;
		}

		const auto& matrices = modelItem.GetRenderBoneMatrices();
		if (matrices.empty()) {
			continue;
		}

		const std::size_t offset = m_bonePaletteScratch.size();
		m_bonePaletteScratch.resize(offset + m_bonePaletteStride);
		const std::size_t matrixCount = std::min(
			matrices.size(), kBonePaletteMatrixCount);
		std::uint8_t* palette =
			m_bonePaletteScratch.data() + offset;
		for (std::size_t matrixIndex = 0;
			 matrixIndex < matrixCount;
			 ++matrixIndex) {
			const glm::mat4& matrix = matrices[matrixIndex];
			const glm::vec4 affineColumns[kBonePaletteColumnsPerMatrix] = {
				glm::vec4(glm::vec3(matrix[0]), matrix[3][0]),
				glm::vec4(glm::vec3(matrix[1]), matrix[3][1]),
				glm::vec4(glm::vec3(matrix[2]), matrix[3][2])
			};
			std::memcpy(
				palette + matrixIndex * sizeof(affineColumns),
				affineColumns,
				sizeof(affineColumns));
		}
		modelItem.bonePaletteOffset = offset;
	}

	m_bonePaletteUploadSize = m_bonePaletteScratch.size();
	if (m_bonePaletteUploadSize == 0) {
		return;
	}

	glBindBuffer(GL_UNIFORM_BUFFER, m_bonePaletteUBO);
	// Re-specifying the streaming store orphans the previous frame without a
	// synchronization readback, then uploads every visible palette in one call.
	glBufferData(
		GL_UNIFORM_BUFFER,
		static_cast<GLsizeiptr>(m_bonePaletteUploadSize),
		m_bonePaletteScratch.data(),
		GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GraphicsManager::BindBonePalette(const ModelRenderComponent& item)
{
	if (!item.shader || !item.shader->UsesBonesBlock() ||
		item.bonePaletteOffset == std::numeric_limits<std::size_t>::max() ||
		item.bonePaletteOffset + kBonePaletteBytes >
			m_bonePaletteUploadSize) {
		return;
	}
	if (m_boundBonePaletteOffset == item.bonePaletteOffset) {
		return;
	}

	glBindBufferRange(
		GL_UNIFORM_BUFFER,
		2,
		m_bonePaletteUBO,
		static_cast<GLintptr>(item.bonePaletteOffset),
		static_cast<GLsizeiptr>(kBonePaletteBytes));
	m_boundBonePaletteOffset = item.bonePaletteOffset;
}
#endif

void GraphicsManager::InitializeSkybox()
{
	static constexpr float skyboxVertices[] = {
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

	if (skyboxVAO == 0 || skyboxVBO == 0) {
		if (skyboxVAO != 0) {
			glDeleteVertexArrays(1, &skyboxVAO);
			skyboxVAO = 0;
		}
		if (skyboxVBO != 0) {
			glDeleteBuffers(1, &skyboxVBO);
			skyboxVBO = 0;
		}

		glGenVertexArrays(1, &skyboxVAO);
		glGenBuffers(1, &skyboxVBO);
		VAO::BindID(skyboxVAO);
		glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		VAO::BindID(0);
	}

	if (!skyboxShader) {
		std::string skyboxShaderPath = ResourceManager::GetPlatformShaderPath("skybox");
		skyboxShader = ResourceManager::GetInstance().GetResource<Shader>(skyboxShaderPath);
	}
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
		const ModelRenderComponent* modelItem = static_cast<const ModelRenderComponent*>(renderItem);
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
			const auto& transforms = modelItem->GetRenderBoneMatrices();
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
				const auto& transforms = modelComp.GetRenderBoneMatrices();
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

			const ModelRenderComponent* modelItem = static_cast<const ModelRenderComponent*>(renderItem);
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
				const auto& transforms = modelItem->GetRenderBoneMatrices();
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

			const ModelRenderComponent* modelItem = static_cast<const ModelRenderComponent*>(renderItem);
			if (!modelItem->isVisible || !modelItem->model)
				continue;

			count++;
			glm::mat4 modelMatrix = modelItem->transform.ConvertToGLM();
			depthShader.setMat4("model", modelMatrix);
			depthShader.setBool("isAnimated", modelItem->HasAnimation());
			if (modelItem->HasAnimation() && modelItem->animator)
			{
				const auto& transforms = modelItem->GetRenderBoneMatrices();
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

	VAO::BindID(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, cameraComp.skyboxTexture->ID);
	skyboxShader->setInt("skyboxTexture", 0);
	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	VAO::BindID(0);
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

	glm::mat4 modelMatrix = item.transform.ConvertToGLM();
	// ModelSystem already culls before queue submission. Repeating the transformed
	// AABB test here doubled CPU culling work for every non-instanced model.

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
		shader->setMat3("normalMatrixCPU", ComputeNormalMatrix(modelMatrix));
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

#ifdef __ANDROID__
	shader->setInt(
		"u_lightMask", static_cast<int>(item.lightMask));
#endif

	// Draw the model
	{
		PROFILE_SCOPED("GM::ModelDraw");
		if (item.depthOffset)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(item.depthOffsetFactor, item.depthOffsetUnits);
		}

#ifdef ANDROID
		if (!item.model->mBoneInfoMap.empty() &&
			!item.GetRenderBoneMatrices().empty()) {
			BindBonePalette(item);
		}
#endif
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
	PROFILE_COUNT("GL::DrawCalls", 1);
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
