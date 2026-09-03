#pragma once

#include "pch.h"

#include "OpenGL.h"

class EBO {
public:
	GLuint ID{};
	std::vector<GLuint> indices;
	bool isSetup = false;
	GLenum indexType = GL_UNSIGNED_INT;

	EBO() noexcept = default;
	EBO(std::vector<GLuint>& indices);

	// Copy constructor
	EBO(const EBO& other)
		: ID(0),
		indices(other.indices),
		isSetup(false),
		indexType(GL_UNSIGNED_INT) {}

	EBO(EBO&& other) noexcept
		: ID(other.ID),
		indices(std::move(other.indices)),
		isSetup(other.isSetup),
		indexType(other.indexType)
	{
		other.ID = 0;
		other.isSetup = false;
		other.indexType = GL_UNSIGNED_INT;
	}

	~EBO();

	// Copy assignment
	EBO& operator=(const EBO& other) {
		if (this != &other) {
			Delete();
			indices = other.indices;
			isSetup = false;
			ID = 0;
			indexType = GL_UNSIGNED_INT;
		}
		return *this;
	}

	EBO& operator=(EBO&& other) noexcept {
		if (this != &other) {
			Delete();
			ID = other.ID;
			indices = std::move(other.indices);
			isSetup = other.isSetup;
			indexType = other.indexType;
			other.ID = 0;
			other.isSetup = false;
			other.indexType = GL_UNSIGNED_INT;
		}
		return *this;
	}

	void Bind();
	void Bind(
		const std::vector<GLuint>& sourceIndices,
		bool prefer16Bit = false);
	void Unbind();
	void Delete();
	GLenum GetIndexType() const noexcept { return indexType; }
};
