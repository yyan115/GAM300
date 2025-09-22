#pragma once

#include "pch.h"

#include "OpenGL.h"

class EBO {
public:
	GLuint ID{};
	std::vector<GLuint> indices;
	EBO(std::vector<GLuint>& indices);

	void Bind();
	void Unbind();
	void Delete();
private:
	void setupBuffer();
};