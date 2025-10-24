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
#include "Logging.hpp"

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
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);      // Cull back-facing triangles
	glFrontFace(GL_CCW);      // Counter-clockwise winding = front face

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
	currentCamera = camera;
}

void GraphicsManager::SetViewportSize(int width, int height)
{
	viewportWidth = width;
	viewportHeight = height;
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
		int renderWidth = (viewportWidth > 0) ? viewportWidth : RunTimeVar::window.width;
		int renderHeight = (viewportHeight > 0) ? viewportHeight : RunTimeVar::window.height;

		if (renderWidth <= 0) renderWidth = 1;
		if (renderHeight <= 0) renderHeight = 1;

		float aspectRatio = (float)renderWidth / (float)renderHeight;
		if (aspectRatio < 0.001f) aspectRatio = 0.001f;
		if (aspectRatio > 1000.0f) aspectRatio = 1000.0f;

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

	// Sort render queue by render order (lower numbers render first)
	std::sort(renderQueue.begin(), renderQueue.end(),
		[](const std::unique_ptr<IRenderComponent>& a, const std::unique_ptr<IRenderComponent>& b) {
			return a->renderOrder < b->renderOrder;
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
		// Use viewport dimensions if set (for editor/scene panel), otherwise fallback to window dimensions
		int renderWidth = (viewportWidth > 0) ? viewportWidth : RunTimeVar::window.width;
		int renderHeight = (viewportHeight > 0) ? viewportHeight : RunTimeVar::window.height;

		// Prevent division by zero and ensure minimum dimensions
		if (renderWidth <= 0) renderWidth = 1;
		if (renderHeight <= 0) renderHeight = 1;

		float aspectRatio = (float)renderWidth / (float)renderHeight;

		// Clamp aspect ratio to reasonable bounds to prevent assertion errors
		if (aspectRatio < 0.001f) aspectRatio = 0.001f;
		if (aspectRatio > 1000.0f) aspectRatio = 1000.0f;

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

	// Enable depth testing for 3D text
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE); // Don't write to depth buffer (allow text to overlay)

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
			glm::mat4 modelMatrix = glm::mat4(1.0f);
			modelMatrix = glm::translate(modelMatrix, item.position.ConvertToGLM());
			modelMatrix = glm::scale(modelMatrix, glm::vec3(item.scale, item.scale, 1.0f));
			SetupMatrices(*item.shader, modelMatrix);
		} else {
			// Normal 2D screen-space rendering for game/runtime (uses window pixel coordinates)
			Setup2DTextMatrices(*item.shader, item.position.ConvertToGLM(), item.scale);
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
	// 3D text uses Transform scale (already in model matrix), 2D text uses item.scale
	float worldScaleFactor = item.is3D ? 0.01f : 1.0f;
	float finalScale = item.is3D ? worldScaleFactor : (item.scale * worldScaleFactor);

	// Calculate starting position based on alignment
	if (item.alignment == TextRenderComponent::Alignment::CENTER)
	{
		x = -item.font->GetTextWidth(item.text, finalScale) / 2.0f;
	}

	else if (item.alignment == TextRenderComponent::Alignment::RIGHT)
	{
		x = -item.font->GetTextWidth(item.text, finalScale);
	}

	// Iterate through all characters
	for (char c : item.text)
	{
		const Character& ch = item.font->GetCharacter(c);
		if (ch.textureID == 0) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "Character '" , c , "' has no texture!\n");
			continue;
		}

		float xpos = x + ch.bearing.x * finalScale;
		float ypos = y - (ch.size.y - ch.bearing.y) * finalScale;

		float w = ch.size.x * finalScale;
		float h = ch.size.y * finalScale;

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
		x += (ch.advance >> 6) * finalScale; // Bitshift by 6 to get value in pixels (2^6 = 64)
	}

	fontVAO->Unbind();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE); // Restore depth writing
}

void GraphicsManager::Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scale)
{
	glm::mat4 projection = glm::ortho(0.0f, (float)WindowManager::GetWindowWidth(), 0.0f, (float)WindowManager::GetWindowHeight());
	glm::mat4 view = glm::mat4(1.0f); // Identity matrix for 2D

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position);  // Use position as-is
	model = glm::scale(model, glm::vec3(scale, scale, 1.0f));

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
	glEnable(GL_CULL_FACE);
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
	glEnable(GL_CULL_FACE);
}

void GraphicsManager::RenderSprite(const SpriteRenderComponent& item)
{
	if (!item.isVisible || !item.texture || !item.shader || !item.spriteVAO)
	{
		return;
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
	glEnable(GL_CULL_FACE);
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

glm::mat4 GraphicsManager::CreateTransformMatrix(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale)
{
	Vector3D position = { pos.x, pos.y, pos.z };
	Vector3D rotation = { rot.x, rot.y, rot.z };
	Vector3D scaleVec = { scale.x, scale.y, scale.z };

	Matrix4x4 modelMatrix = TransformSystem::CalculateModelMatrix(position, scaleVec, rotation);
	return modelMatrix.ConvertToGLM();
}

