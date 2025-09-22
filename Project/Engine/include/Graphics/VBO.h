#pragma once
#include "OpenGL.h"
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;
};

// VBO stores multiple vertices on the GPU memory
class VBO {
public:
	GLuint ID{};
	VBO(std::vector<Vertex>& vertices);

	void Bind();
	void Unbind();
	void Delete();

	VBO(size_t size, GLenum usage = GL_DYNAMIC_DRAW);
	void UpdateData(const void* data, size_t size, size_t offset = 0);
	// New method to initialize buffer with specific size (for deferred init)
	void InitializeBuffer(size_t size, GLenum usage = GL_DYNAMIC_DRAW);

private:
	bool initialized = false;
};