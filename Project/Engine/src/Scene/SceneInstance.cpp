#include "pch.h"
#include <Scene/SceneInstance.hpp>
#ifdef ANDROID
#include <android/log.h>
#endif

// Minimal stub implementation to get build working
// TODO: Implement proper scene functionality

void TestScene::Initialize() {
	// TODO: Implement scene initialization
}

void TestScene::Update(double dt) {
	(void)dt; // Suppress unused parameter warning
	// TODO: Implement scene update
}

void TestScene::Draw() {
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "TestScene::Draw() called");
#endif

	// Simple triangle test for Android debugging
	static GLuint vao = 0, vbo = 0, shaderProgram = 0;
	if (vao == 0) {
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up triangle test in TestScene...");
#endif
		// Simple triangle vertices
		float vertices[] = {
			 0.0f,  0.5f, 0.0f,  // top
			-0.5f, -0.5f, 0.0f,  // left
			 0.5f, -0.5f, 0.0f   // right
		};

		// Generate and bind VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// Generate and bind VBO
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		// Set vertex attribute pointers
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// Simple vertex shader
		const char* vertexShaderSource = R"(
#version 300 es
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";

		// Simple fragment shader
		const char* fragmentShaderSource = R"(
#version 300 es
precision mediump float;
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red triangle
}
)";

		// Create and compile shaders
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
		glCompileShader(vertexShader);

		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
		glCompileShader(fragmentShader);

		// Create shader program
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);

		// Clean up shaders
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "Triangle test setup complete");
#endif
	}

	// Render triangle
	glUseProgram(shaderProgram);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

#ifdef ANDROID
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error in TestScene::Draw(): %d", error);
	}
#endif
}

void TestScene::Exit() {
	// TODO: Implement scene exit
}

void TestScene::processInput() {
	// TODO: Implement input processing
}

void TestScene::DrawLightCubes() {
	// TODO: Implement light cube drawing
}

