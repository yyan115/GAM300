#pragma once

#include "pch.h"

#include "OpenGL.h"

class EBO {
public:
	GLuint ID{};
	std::vector<GLuint> indices;
	bool isSetup;

	EBO(std::vector<GLuint>& indices);

	// Copy constructor
	EBO(const EBO& other) : indices(other.indices), isSetup(false), ID(0) {}

	// Copy assignment
	EBO& operator=(const EBO& other) {
		if (this != &other) {
			Delete();
			indices = other.indices;
			isSetup = false;
			ID = 0;
		}
		return *this;
	}

	void Bind();
	void Unbind();
	void Delete();
};