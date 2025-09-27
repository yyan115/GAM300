#pragma once

#include "pch.h"

#include "OpenGL.h"

class EBO {
public:
	GLuint ID{};
	std::vector<GLuint> indices;
	bool isSetup;

	EBO(std::vector<GLuint>& indices);

	void Bind();
	void Unbind();
	void Delete();
};