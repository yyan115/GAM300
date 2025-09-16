#pragma once
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/Font.hpp"

class TestScene {
public:
	void Initialize();
	void Update();
	void Exit();
	void processInput();

	void DrawLightCubes();

	const unsigned int SCR_WIDTH = 800;
	const unsigned int SCR_HEIGHT = 600;

	// camera
	Camera camera{ glm::vec3(0.0f, 0.0f, 3.0f) };
	float lastX = SCR_WIDTH / 2.0f;
	float lastY = SCR_HEIGHT / 2.0f;
	bool firstMouse = true;

	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	std::vector<Vertex> lightVertices = {
		// Back face (4 vertices: 0-3)
		{{-0.1f, -0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 0
		{{ 0.1f, -0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 1
		{{ 0.1f,  0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 2
		{{-0.1f,  0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 3

		// Front face (4 vertices: 4-7)
		{{-0.1f, -0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 4
		{{ 0.1f, -0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 5
		{{ 0.1f,  0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 6
		{{-0.1f,  0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 7

		// Left face (4 vertices: 8-11)
		{{-0.1f,  0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 8
		{{-0.1f,  0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 9
		{{-0.1f, -0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 10
		{{-0.1f, -0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 11

		// Right face (4 vertices: 12-15)
		{{ 0.1f,  0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 12
		{{ 0.1f,  0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 13
		{{ 0.1f, -0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 14
		{{ 0.1f, -0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 15

		// Bottom face (4 vertices: 16-19)
		{{-0.1f, -0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 16
		{{ 0.1f, -0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 17
		{{ 0.1f, -0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 18
		{{-0.1f, -0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 19

		// Top face (4 vertices: 20-23)
		{{-0.1f,  0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 20
		{{ 0.1f,  0.1f, -0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 21
		{{ 0.1f,  0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // 22
		{{-0.1f,  0.1f,  0.1f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}  // 23
	};

	std::vector<GLuint> lightIndices = {
		// Back face
		0, 1, 2,   2, 3, 0,
		// Front face
		4, 5, 6,   6, 7, 4,
		// Left face
		8, 9, 10,  10, 11, 8,
		// Right face
		12, 13, 14, 14, 15, 12,
		// Bottom face
		16, 17, 18, 18, 19, 16,
		// Top face
		20, 21, 22, 22, 23, 20
	};

	std::shared_ptr<Model> backpackModel;
	std::shared_ptr<Shader> shader;
	//----------------LIGHT-------------------
	std::shared_ptr<Shader> lightShader;
	std::shared_ptr<Mesh> lightCubeMesh;

	// Text rendering members
	std::shared_ptr<Font> testFont;
	std::shared_ptr<Shader> textShader;
};