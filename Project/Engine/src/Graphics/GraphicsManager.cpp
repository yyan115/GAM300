#include "pch.h"
#include "Graphics/GraphicsManager.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

#ifdef ANDROID
#include <android/log.h>
#endif
#include <Transform/TransformSystem.hpp>
#include <ECS/ECSManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <ECS/SortingLayerManager.hpp>
#include "Logging.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Camera/CameraSystem.hpp"
#include "Asset Manager/ResourceManager.hpp"

GraphicsManager& GraphicsManager::GetInstance()
{
	static GraphicsManager instance;
	return instance;
}

bool GraphicsManager::Initialize(int window_width, int window_height)
{
	(void)window_width, window_height;
	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Enable face culling (backface culling)
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);      // Cull back-facing triangles
	//glFrontFace(GL_CCW);      // Counter-clockwise winding = front face

	// Initialize skybox
	InitializeSkybox();

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
}

void GraphicsManager::EndFrame()
{
	
}

void GraphicsManager::Clear(float r, float g, float b, float a)
{
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "GraphicsManager::Clear() called");

	// Ensure EGL context is current before making OpenGL calls
	auto* platform = WindowManager::GetPlatform();
	if (platform) {
		platform->MakeContextCurrent();
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "EGL context made current");
	}

	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call glClearColor");
#endif
	glClearColor(r, g, b, a);
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to call glClear");
#endif
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "GraphicsManager::Clear() completed");
#endif
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
	if (renderItem && renderItem->isVisible)
	{
		renderQueue.push_back(std::move(renderItem));
	}
}

void GraphicsManager::UpdateFrustum()
{
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
				0.1f, 100.0f
			);
		}

		glm::mat4 viewProjection = projection * view;
		viewFrustum.Update(viewProjection);
	}
}

void GraphicsManager::Render()
{
	if (auto* platform = WindowManager::GetPlatform()) {
		platform->MakeContextCurrent();
	}

	if (!currentCamera)
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[GraphicsManager] Warning: No camera set for rendering!\n");
		return;
	}

	if (frustumCullingEnabled)
	{
		cullingStats.Reset();
	}

	currentFrameViewport = GetCurrentViewport();

	// Render skybox first (before other objects)
	RenderSkybox();

	// Sort render queue - Unity-like sorting for 2D elements
	// For 2D elements: sort by sortingLayer then sortingOrder (higher = on top = renders later)
	// 3D objects render based on renderOrder
	std::sort(renderQueue.begin(), renderQueue.end(),
		[](const std::unique_ptr<IRenderComponent>& a, const std::unique_ptr<IRenderComponent>& b) {
			// Helper to check if component is 2D
			auto is2D = [](const IRenderComponent* comp) -> bool {
				if (auto text = dynamic_cast<const TextRenderComponent*>(comp)) {
					return !text->is3D;
				}
				if (auto sprite = dynamic_cast<const SpriteRenderComponent*>(comp)) {
					return !sprite->is3D;
				}
				return false;
			};

			// Helper to get sortingLayer ORDER (for rendering priority)
			auto getSortingLayer = [](const IRenderComponent* comp) -> int {
				int layerID = 0;
				if (auto text = dynamic_cast<const TextRenderComponent*>(comp)) {
					layerID = text->sortingLayer;
				}
				else if (auto sprite = dynamic_cast<const SpriteRenderComponent*>(comp)) {
					layerID = sprite->sortingLayer;
				}

				// Get the order from the sorting layer manager
				// Lower order = rendered first (behind), higher order = rendered last (on top)
				int order = SortingLayerManager::GetInstance().GetLayerOrder(layerID);
				if (order == -1) {
					// Invalid layer ID, use default (0)
					return 0;
				}
				return order;
			};

			// Helper to get sortingOrder
			auto getSortingOrder = [](const IRenderComponent* comp) -> int {
				if (auto text = dynamic_cast<const TextRenderComponent*>(comp)) {
					return text->sortingOrder;
				}
				if (auto sprite = dynamic_cast<const SpriteRenderComponent*>(comp)) {
					return sprite->sortingOrder;
				}
				return 0;
			};

			bool aIs2D = is2D(a.get());
			bool bIs2D = is2D(b.get());

			// If both are 2D or both are 3D, use appropriate sorting
			if (aIs2D && bIs2D) {
				// Both 2D - sort by sortingLayer then sortingOrder (Unity-like)
				int layerA = getSortingLayer(a.get());
				int layerB = getSortingLayer(b.get());
				if (layerA != layerB) {
					return layerA < layerB; // Higher layer = on top = renders later
				}
				int orderA = getSortingOrder(a.get());
				int orderB = getSortingOrder(b.get());
				return orderA < orderB; // Higher order = on top = renders later
			}
			else if (!aIs2D && !bIs2D) {
				// Both 3D - sort by renderOrder
				return a->renderOrder < b->renderOrder;
			}
			else {
				// Mixed 2D and 3D - render 3D first, then 2D on top
				return !aIs2D; // 3D (false) < 2D (true), so 3D renders first
			}
		});

	// Render all items in the queue
	for (const auto& renderItem : renderQueue) 
	{
		// Cast to different component types
		const ModelRenderComponent* modelItem = dynamic_cast<const ModelRenderComponent*>(renderItem.get());
		const TextRenderComponent* textItem = dynamic_cast<const TextRenderComponent*>(renderItem.get());
		const SpriteRenderComponent* spriteItem = dynamic_cast<const SpriteRenderComponent*>(renderItem.get());
		const DebugDrawComponent* debugItem = dynamic_cast<const DebugDrawComponent*>(renderItem.get());
		const ParticleComponent* particleItem = dynamic_cast<const ParticleComponent*>(renderItem.get());

		if (modelItem)
		{
//#ifdef ANDROID
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "RenderModel");
//#endif
			RenderModel(*modelItem);
		}
		else if (textItem)
		{
//#ifdef ANDROID
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "RenderText");
//#endif
			RenderText(*textItem);
		}
		else if (spriteItem)
		{
//#ifdef ANDROID
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "RenderSprite");
//#endif
			RenderSprite(*spriteItem);
		}
		else if (debugItem)
		{
//#ifdef ANDROID
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "RenderDebugDraw");
//#endif
			RenderDebugDraw(*debugItem);
		}
		else if (particleItem)
		{
//#ifdef ANDROID
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "RenderParticles");
//#endif
			RenderParticles(*particleItem);
		}
	}
}

void GraphicsManager::RenderModel(const ModelRenderComponent& item)
{
	if (!item.isVisible || !item.model || !item.shader) 
	{
		return;
	}

	// Count total objects when culling is enabled
	if (frustumCullingEnabled && currentCamera)
	{
		//cullingStats.totalObjects++;

		AABB modelBBox = item.model->GetBoundingBox();
		glm::mat4 modelMatrix = item.transform.ConvertToGLM();
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

	// Apply lighting
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager(); 
	if (ecsManager.lightingSystem) 
	{
		ecsManager.lightingSystem->ApplyLighting(*item.shader);
	}

	// Draw the model with entity material
	if (item.HasAnimation())
		item.model->Draw(*item.shader, *currentCamera, item.material, item.animator);
	else
		item.model->Draw(*item.shader, *currentCamera, item.material);

	//std::cout << "rendered model\n";
}

void GraphicsManager::SetupMatrices(Shader& shader, const glm::mat4& modelMatrix, bool includeNormalMatrix)
{
	shader.setMat4("model", modelMatrix);

	// Only calculate and send normal matrix if needed (for lit objects)
	if (includeNormalMatrix)
	{
		glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));
		shader.setMat3("normalMatrix", normalMatrix);
	}

	if (currentCamera)
	{
		int renderWidth = currentFrameViewport.width;
		int renderHeight = currentFrameViewport.height;
		float aspectRatio = currentFrameViewport.aspectRatio;

		glm::mat4 view;
		glm::mat4 projection;

		// In 2D editor mode, use orthographic projection with camera position as center
		if (IsRenderingForEditor() && Is2DMode()) {
			// Use identity view matrix for 2D (camera doesn't rotate)
			view = glm::mat4(1.0f);

			// Create orthographic projection centered on camera's XY position
			// The camera position represents the center of the view in pixel space
			// Apply OrthoZoomLevel to the viewport size (1.0 = normal, 0.5 = zoomed in 2x, 2.0 = zoomed out 2x)
			float viewWidth = renderWidth * currentCamera->OrthoZoomLevel;
			float viewHeight = renderHeight * currentCamera->OrthoZoomLevel;
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
				0.1f, 100.0f
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
		// 3D text: enable depth testing
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE); // Don't write to depth buffer (allow text to overlay)
	}
	else {
		// 2D text: disable depth testing so render order determines what's on top (Unity-style)
		glDisable(GL_DEPTH_TEST);
	}

	// Enable blending for text transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Activate shader and set uniforms
	item.shader->Activate();
	item.shader->setVec3("textColor", item.color.ConvertToGLM());

	// Set up matrices based on whether it's 2D or 3D text
	if (item.is3D)
	{
		// 3D text rendering - use normal 3D matrices
		// 3D text uses Transform component scale (in model matrix)
		glm::mat4 modelMatrix = item.transform.ConvertToGLM();
		SetupMatrices(*item.shader, modelMatrix);
	}
	else
	{
		// 2D screen space text rendering
		if (IsRenderingForEditor() && Is2DMode()) {
			// Use the editor camera's view/projection matrices
			// Don't apply scale to model matrix - we'll apply it per-character for proper axis control
			glm::mat4 modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, item.position.ConvertToGLM());
			SetupMatrices(*item.shader, modelMatrix);
		} else {
			// Normal 2D screen-space rendering for game/runtime (uses window pixel coordinates)
			// Don't apply scale to matrices - we'll apply it per-character for proper axis control
			Setup2DTextMatrices(*item.shader, item.position.ConvertToGLM(), 1.0f, 1.0f);
		}
	}

	// Bind VAO and render each character
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

	float x = 0.0f;
	float y = 0.0f;

	// For 3D text, scale down from pixels to world units (1 pixel = 0.01 units)
	// For 2D text, apply Transform scale per-axis for Unity-like behavior
	// Note: fontSize controls the font resolution (glyphs are loaded at that size)
	// Transform scale then scales those glyphs for final visual size
	float worldScaleFactor = item.is3D ? 0.01f : 1.0f;
	float scaleX = item.is3D ? worldScaleFactor : (item.transformScale.x * worldScaleFactor);
	float scaleY = item.is3D ? worldScaleFactor : (item.transformScale.y * worldScaleFactor);

	// Calculate starting position based on alignment
	if (item.alignment == TextRenderComponent::Alignment::CENTER)
	{
		x = -item.font->GetTextWidth(item.text, scaleX) / 2.0f;
	}
	else if (item.alignment == TextRenderComponent::Alignment::RIGHT)
	{
		x = -item.font->GetTextWidth(item.text, scaleX);
	}

	// Iterate through all characters
	for (char c : item.text)
	{
		const Character& ch = item.font->GetCharacter(c);
		if (ch.textureID == 0) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "Character '" , c , "' has no texture!\n");
			continue;
		}

		// Apply scaleX to horizontal metrics, scaleY to vertical metrics (Unity-like behavior)
		float xpos = x + ch.bearing.x * scaleX;
		float ypos = y - (ch.size.y - ch.bearing.y) * scaleY;

		float w = ch.size.x * scaleX;
		float h = ch.size.y * scaleY;

		// Update VBO for each character
		float vertices[6][4] = {
			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos,     ypos,       0.0f, 1.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },

			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },
			{ xpos + w, ypos + h,   1.0f, 0.0f }
		};

		// Render glyph texture over quad
		glBindTexture(GL_TEXTURE_2D, ch.textureID);

		// Update content of VBO memory using your extended VBO class
		fontVBO->UpdateData(vertices, sizeof(vertices));

		// Render quad
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
		x += (ch.advance >> 6) * scaleX; // Bitshift by 6 to get value in pixels (2^6 = 64), use scaleX for horizontal spacing
	}

	fontVAO->Unbind();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE); // Restore depth writing
	glEnable(GL_DEPTH_TEST); // Restore depth testing for 3D objects
}

void GraphicsManager::Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scaleX, float scaleY)
{
	glm::mat4 projection = glm::ortho(0.0f, (float)WindowManager::GetWindowWidth(), 0.0f, (float)WindowManager::GetWindowHeight());
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
	__android_log_print(ANDROID_LOG_INFO, "GraphicsManager", "Debug wireframe rendering not supported on Android");
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
	//glEnable(GL_CULL_FACE);
#endif
}

void GraphicsManager::RenderParticles(const ParticleComponent& item) {
#ifdef ANDROID
	assert(eglGetCurrentContext() != EGL_NO_CONTEXT);
#endif
	if (!item.isVisible || item.particles.empty() || !item.particleShader || !item.particleVAO) return;
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending
	glDepthMask(GL_FALSE);

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "item.particleShader->Activate");
#endif
	item.particleShader->Activate();

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setup camera matrices ONCE for all particles");
#endif
	// Setup camera matrices ONCE for all particles
	if (currentCamera) {
		glm::mat4 view = currentCamera->GetViewMatrix();
		item.particleShader->setMat4("view", view);

		float aspectRatio = (float)RunTimeVar::window.width / (float)RunTimeVar::window.height;
		glm::mat4 projection = glm::perspective(
			glm::radians(currentCamera->Zoom),
			aspectRatio,
			0.1f, 100.0f
		);
		item.particleShader->setMat4("projection", projection);

		// Send camera vectors for billboard calculations in vertex shader
		glm::vec3 cameraRight = glm::normalize(glm::cross(currentCamera->Front, currentCamera->Up));
		item.particleShader->setVec3("cameraRight", cameraRight);
		item.particleShader->setVec3("cameraUp", currentCamera->Up);
	}

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Bind texture if available");
#endif
	// Bind texture if available
	if (item.particleTexture) {
		glActiveTexture(GL_TEXTURE0);
		item.particleTexture->Bind(0);
		item.particleShader->setInt("particleTexture", 0);
	}

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Draw ALL particles with ONE instanced draw call using indices");
#endif
	// Draw ALL particles with ONE instanced draw call using indices
	item.particleVAO->Bind();
	if (item.quadEBO) item.quadEBO->Bind();  // explicitly ensure EBO is bound
	GLint eboBinding = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eboBinding);
	assert(eboBinding != 0 && "VAO has no EBO bound after setup");

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Binded");
#endif
	glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(item.particles.size()));
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "glDrawElementsInstanced");
#endif
	item.particleVAO->Unbind();
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Unbinded");
#endif
	//item.quadEBO->Unbind();

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	//glEnable(GL_CULL_FACE);
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
		// 2D sprite: disable depth testing so render order determines what's on top (Unity-style)
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

	GLint ebo = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
	if (ebo == 0) {
		ENGINE_LOG_ERROR("VAO %d has no EBO bound!" + std::to_string(item.spriteVAO->ID));
	}

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
	//glEnable(GL_CULL_FACE);
}

void GraphicsManager::Setup2DSpriteMatrices(Shader& shader, const glm::vec3& position, const glm::vec3& scale, float rotation)
{

	// Use orthographic projection for 2D sprites
	// Use viewport dimensions (render target size) instead of window dimensions
	GLint renderWidth = (RunTimeVar::window.viewportWidth > 0) ? RunTimeVar::window.viewportWidth : RunTimeVar::window.width;
	GLint renderHeight = (RunTimeVar::window.viewportHeight > 0) ? RunTimeVar::window.viewportHeight : RunTimeVar::window.height;
	glm::mat4 projection = glm::ortho(0.0f, (float)renderWidth,
		0.0f, (float)renderHeight);

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
		std::cout << "[GraphicsManager] WARNING: Failed to load skybox shader from: " << skyboxShaderPath << std::endl;
	} else {
		std::cout << "[GraphicsManager] Skybox shader loaded successfully - ID: " << skyboxShader->ID << std::endl;
	}

	std::cout << "[GraphicsManager] Skybox initialized - VAO: " << skyboxVAO << ", VBO: " << skyboxVBO << std::endl;
}

void GraphicsManager::RenderSkybox()
{
	static bool checkedOnce = false;

	if (!currentCamera || !skyboxShader || skyboxVAO == 0) {
		if (!checkedOnce) {
			std::cout << "[GraphicsManager] Skybox render skipped - camera: " << (currentCamera != nullptr)
				<< ", shader: " << (skyboxShader != nullptr) << ", VAO: " << skyboxVAO << std::endl;
			checkedOnce = true;
		}
		return;
	}

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (!ecsManager.cameraSystem) {
		if (!checkedOnce) {
			std::cout << "[GraphicsManager] Skybox render skipped - no camera system" << std::endl;
			checkedOnce = true;
		}
		return;
	}

	Entity activeCameraEntity = ecsManager.cameraSystem->GetActiveCameraEntity();
	if (activeCameraEntity == UINT32_MAX || !ecsManager.HasComponent<CameraComponent>(activeCameraEntity)) {
		if (!checkedOnce) {
			std::cout << "[GraphicsManager] Skybox render skipped - no active camera entity" << std::endl;
			checkedOnce = true;
		}
		return;
	}

	auto& cameraComp = ecsManager.GetComponent<CameraComponent>(activeCameraEntity);
	if (!cameraComp.skyboxTexture) {
		if (!checkedOnce) {
			std::cout << "[GraphicsManager] Skybox render skipped - no skybox texture assigned" << std::endl;
			checkedOnce = true;
		}
		return;
	}

	static bool logged = false;
	if (!logged) {
		std::cout << "[GraphicsManager] Rendering skybox - Texture ID: " << cameraComp.skyboxTexture->ID
			<< ", Viewport: " << viewportWidth << "x" << viewportHeight << std::endl;
		logged = true;
	}

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	skyboxShader->Activate();

	glm::mat4 view = glm::mat4(glm::mat3(currentCamera->GetViewMatrix()));

	float aspectRatio = (float)viewportWidth / (float)viewportHeight;
	glm::mat4 projection = glm::perspective(
		glm::radians(currentCamera->Zoom),
		aspectRatio,
		0.1f, 100.0f
	);

	skyboxShader->setMat4("view", view);
	skyboxShader->setMat4("projection", projection);

	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, cameraComp.skyboxTexture->ID);
	skyboxShader->setInt("skyboxTexture", 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDepthMask(GL_TRUE);
	//glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
}

