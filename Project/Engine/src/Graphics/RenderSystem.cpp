#include "pch.h"
#include "Graphics/LightManager.hpp"
#include "Graphics/RenderSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include <Graphics/Renderer.hpp>
#ifdef ANDROID
#include <android/log.h>
#endif


bool RenderSystem::Initialise(int window_width, int window_height)
{
    screenWidth = window_width;
    screenHeight = window_height;

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, screenWidth, screenHeight);
    std::cout << "[Renderer] Initialised" << std::endl;
	return true;
}

void RenderSystem::applyLighting(Shader& shader) 
{
    LightManager& lightManager = LightManager::getInstance();

    // Apply directional light
    const auto& dirLight = lightManager.getDirectionalLight();
    shader.setVec3("dirLight.direction", dirLight.direction);
    shader.setVec3("dirLight.ambient", dirLight.ambient);
    shader.setVec3("dirLight.diffuse", dirLight.diffuse);
    shader.setVec3("dirLight.specular", dirLight.specular);

    // Apply point lights
    const auto& pointLights = lightManager.getPointLights();
    for (size_t i = 0; i < pointLights.size() && i < 4; i++) {
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
    if (lightManager.isSpotLightEnabled() && currentCamera) {
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

//void RenderSystem::BeginFrame()
//{
//    renderQueue.clear();
//}

void RenderSystem::Clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

//void RenderSystem::Submit(std::shared_ptr<Model> model, const glm::mat4& transform, std::shared_ptr<Shader> shader)
//{
//    renderQueue.push_back({ model, transform, shader });
//}

void RenderSystem::SetCamera(Camera* camera)
{
    currentCamera = camera;
}

void RenderSystem::Render()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	for (const auto& entity : entities) {
		auto& renderer = ecsManager.GetComponent<Renderer>(entity);
		renderer.shader->Activate();
		glm::mat4 modelMatrix = ecsManager.GetComponent<Transform>(entity).model;
		renderer.shader->setMat4("model", modelMatrix);
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting model matrix for entity %u", entity);
#endif

		if (currentCamera)
		{
			// View, for camera
			// Camera explanation:
			// cameraPos      - The position of the camera in world space
			// cameraFront    - The direction the camera is facing (default is (0, 0, -1), into the screen)
			// cameraPos + cameraFront - The target point the camera is looking at
			// 
			// We use glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp) to generate the view matrix:
			// - The first argument is the camera position
			// - The second argument is the position we are looking at (target)
			// - The third argument is the up direction (usually (0, 1, 0))
			// 
			// Adding cameraFront to cameraPos gives us a point directly in front of the camera.
			// Technically the direction will never change, but lookAt asks for a target, hence in order to keep it in line with the direction we add the camera pos
			// If we dont add camera position, we will constantly be looking at that point when we want to move forward instead
			// This allows the camera to move and look in the right direction based on its position and orientation.

			glm::mat4 view = currentCamera->GetViewMatrix();
			glm::mat4 projection = glm::perspective(glm::radians(currentCamera->Zoom), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);

			renderer.shader->setMat4("view", view);
			renderer.shader->setMat4("projection", projection);
			renderer.shader->setVec3("cameraPos", currentCamera->Position);

			// Material
			renderer.shader->setFloat("material.shininess", 32.0f);

			applyLighting(*renderer.shader);
        }

		renderer.model->Draw(*renderer.shader, *currentCamera);
	}

   // for (const auto& item : renderQueue)
   // {
   //     item.Shader->Activate();
   //     item.Shader->setMat4("model", item.transform);
   //     
   //     if (currentCamera)
   //     {
			//// View, for camera
			//// Camera explanation:
			//// cameraPos      - The position of the camera in world space
			//// cameraFront    - The direction the camera is facing (default is (0, 0, -1), into the screen)
			//// cameraPos + cameraFront - The target point the camera is looking at
			//// 
			//// We use glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp) to generate the view matrix:
			//// - The first argument is the camera position
			//// - The second argument is the position we are looking at (target)
			//// - The third argument is the up direction (usually (0, 1, 0))
			//// 
			//// Adding cameraFront to cameraPos gives us a point directly in front of the camera.
			//// Technically the direction will never change, but lookAt asks for a target, hence in order to keep it in line with the direction we add the camera pos
			//// If we dont add camera position, we will constantly be looking at that point when we want to move forward instead
			//// This allows the camera to move and look in the right direction based on its position and orientation.

   //         glm::mat4 view = currentCamera->GetViewMatrix();
   //         glm::mat4 projection = glm::perspective(glm::radians(currentCamera->Zoom), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);

   //         item.Shader->setMat4("view", view);
   //         item.Shader->setMat4("projection", projection);
   //         item.Shader->setVec3("cameraPos", currentCamera->Position);

			//// Material
			//item.Shader->setFloat("material.shininess", 32.0f);

			//SetUpLighting(*item.Shader);
   //     }

   //     item.Model->Draw(*item.Shader, *currentCamera);
   // }
}

void RenderSystem::EndFrame()
{

}

void RenderSystem::Shutdown()
{
    //renderQueue.clear();
}