#pragma once
#include <memory>
#include "Model.h"

// struct RenderItem {
// 	std::shared_ptr<Model> Model;
// 	glm::mat4 transform;
// 	std::shared_ptr<Shader> Shader;
// };

// class Renderer {
// public:
// 	static Renderer& getInstance();

// 	bool Initialise(int window_width, int window_height);
// 	void applyLighting(Shader& shader);
// 	void Shutdown();

// 	void BeginFrame();
// 	void EndFrame();
// 	void Clear(float r = 0.2f, float g = 0.3f, float b = 0.3f, float a = 1.0f);

// 	void Submit(std::shared_ptr<Model> model, const glm::mat4& transform, std::shared_ptr<Shader> shader);
// 	void SetCamera(Camera* camera);
// 	void Render();

// 	// Delete copy constructor
// 	Renderer(const Renderer&) = delete;
// 	Renderer& operator=(const Renderer&) = delete;

// 	// Remove later
// 	const glm::vec3* getPointLightPositions() const { return pointLightPositions; }
// private:
// 	Renderer() = default;
// 	~Renderer() = default;

// 	std::vector<RenderItem> renderQueue;
// 	Camera* currentCamera = nullptr;
// 	int screenWidth = 800;
// 	int screenHeight = 600;

// 	// Remove later
// 	glm::vec3 pointLightPositions[4] = {
// 		glm::vec3(0.7f,  0.2f,  2.0f),
// 		glm::vec3(2.3f, -3.3f, -4.0f),
// 		glm::vec3(-4.0f,  2.0f, -12.0f),
// 		glm::vec3(0.0f,  0.0f, -3.0f)
// 	};
struct Renderer {
	std::shared_ptr<Model> model; // contains mesh and textures
	std::shared_ptr<Shader> shader; // shader to use for rendering
	//glm::mat4 transform; // model transformation matrix
	bool isVisible;
};