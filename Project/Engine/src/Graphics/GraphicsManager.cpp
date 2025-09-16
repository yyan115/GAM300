#include "pch.h"
#include "Graphics/GraphicsManager.hpp"
#include "WindowManager.hpp"

GraphicsManager& GraphicsManager::GetInstance()
{
	static GraphicsManager instance;
	return instance;
}

bool GraphicsManager::Initialize(int window_width, int window_height)
{
	return false;
}

void GraphicsManager::Shutdown()
{
	renderQueue.clear();
	currentCamera = nullptr;
	std::cout << "[GraphicsManager] Shutdown" << std::endl;
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
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GraphicsManager::SetCamera(Camera* camera)
{
	currentCamera = camera;
}

void GraphicsManager::Submit(std::unique_ptr<IRenderComponent> renderItem)
{
	if (renderItem && renderItem->isVisible)
	{
		renderQueue.push_back(std::move(renderItem));
	}
}

void GraphicsManager::SubmitModel(std::shared_ptr<Model> model, std::shared_ptr<Shader> shader, const Matrix4x4& transform)
{
	if (model && shader) 
	{
		glm::mat4 glmTransform = ConvertMatrix4x4ToGLM(transform);
		auto renderItem = std::make_unique<ModelRenderComponent>(model, shader);
		renderItem->transform = glmTransform;
		Submit(std::move(renderItem));
	}
}

void GraphicsManager::Render()
{
	if (!currentCamera) 
	{
		std::cerr << "[GraphicsManager] Warning: No camera set for rendering!" << std::endl;
		return;
	}

	// Sort render queue by render order (lower numbers render first)
	std::sort(renderQueue.begin(), renderQueue.end(),
		[](const std::unique_ptr<IRenderComponent>& a, const std::unique_ptr<IRenderComponent>& b) {
			return a->renderOrder < b->renderOrder;
		});

	// Render all items in the queue
	for (const auto& renderItem : renderQueue) 
	{
		// Cast to ModelRenderItem since that's what we have for now
		// Later you can add a switch statement for different types
		const ModelRenderComponent* modelItem = dynamic_cast<const ModelRenderComponent*>(renderItem.get());
		if (modelItem) 
		{
			RenderModel(*modelItem);
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
	SetupMatrices(*item.shader, item.transform);

	// Apply lighting
	ApplyLighting(*item.shader);

	// Draw the model
	item.model->Draw(*item.shader, *currentCamera);
}

void GraphicsManager::ApplyLighting(Shader& shader)
{
	// Moved directly from your ModelSystem::applyLighting method
	LightManager& lightManager = LightManager::getInstance();

	// Apply directional light
	const auto& dirLight = lightManager.getDirectionalLight();
	shader.setVec3("dirLight.direction", dirLight.direction);
	shader.setVec3("dirLight.ambient", dirLight.ambient);
	shader.setVec3("dirLight.diffuse", dirLight.diffuse);
	shader.setVec3("dirLight.specular", dirLight.specular);

	// Apply point lights
	const auto& pointLights = lightManager.getPointLights();
	for (size_t i = 0; i < pointLights.size() && i < 4; i++) 
	{
		std::string base = "pointLights[" + std::to_string(i) + "]";
		shader.setVec3(base + ".position", pointLights[i].position);
		shader.setVec3(base + ".ambient", pointLights[i].ambient);
		shader.setVec3(base + ".diffuse", pointLights[i].diffuse);
		shader.setVec3(base + ".specular", pointLights[i].specular);
		shader.setFloat(base + ".constant", pointLights[i].constant);
		shader.setFloat(base + ".linear", pointLights[i].linear);
		shader.setFloat(base + ".quadratic", pointLights[i].quadratic);
	}

	// Apply spotlight
	if (lightManager.isSpotLightEnabled() && currentCamera) 
	{
		const auto& spotLight = lightManager.getSpotLight();
		shader.setVec3("spotLight.position", currentCamera->Position);
		shader.setVec3("spotLight.direction", currentCamera->Front);
		shader.setVec3("spotLight.ambient", spotLight.ambient);
		shader.setVec3("spotLight.diffuse", spotLight.diffuse);
		shader.setVec3("spotLight.specular", spotLight.specular);
		shader.setFloat("spotLight.constant", spotLight.constant);
		shader.setFloat("spotLight.linear", spotLight.linear);
		shader.setFloat("spotLight.quadratic", spotLight.quadratic);
		shader.setFloat("spotLight.cutOff", spotLight.cutOff);
		shader.setFloat("spotLight.outerCutOff", spotLight.outerCutOff);
	}
}

void GraphicsManager::SetupMatrices(Shader& shader, const glm::mat4& modelMatrix)
{
	shader.setMat4("model", modelMatrix);

	if (currentCamera) 
	{
		glm::mat4 view = currentCamera->GetViewMatrix();
		shader.setMat4("view", view);

		// Get window dimensions with safety checks
		int windowWidth = WindowManager::GetWindowWidth();
		int windowHeight = WindowManager::GetWindowHeight();

		// Prevent division by zero and ensure minimum dimensions
		if (windowWidth <= 0) windowWidth = 1;
		if (windowHeight <= 0) windowHeight = 1;

		float aspectRatio = (float)windowWidth / (float)windowHeight;

		// Clamp aspect ratio to reasonable bounds to prevent assertion errors
		if (aspectRatio < 0.001f) aspectRatio = 0.001f;
		if (aspectRatio > 1000.0f) aspectRatio = 1000.0f;

		glm::mat4 projection = glm::perspective(
			glm::radians(currentCamera->Zoom),
			aspectRatio,
			0.1f, 100.0f
		);
		shader.setMat4("projection", projection);

		shader.setVec3("cameraPos", currentCamera->Position);
	}
}

glm::mat4 GraphicsManager::ConvertMatrix4x4ToGLM(const Matrix4x4& m)
{
	Matrix4x4 transposed = m.Transposed();
	glm::mat4 converted(
		transposed[0][0], transposed[0][1], transposed[0][2], transposed[0][3],
		transposed[1][0], transposed[1][1], transposed[1][2], transposed[1][3],
		transposed[2][0], transposed[2][1], transposed[2][2], transposed[2][3],
		transposed[3][0], transposed[3][1], transposed[3][2], transposed[3][3]);
	return converted;
}

