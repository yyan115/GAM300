#pragma once

#include "pch.h"

#include "OpenGL.h"

class EBO {
public:
	GLuint ID{};
	EBO(std::vector<GLuint>& indices);

	void Bind();
	void Unbind();
	void Delete();
};