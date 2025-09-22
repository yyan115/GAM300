#pragma once

#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.hpp"
#include "Engine.h"

class Mesh {
public:
	std::vector<Vertex> vertices; 
	std::vector<GLuint> indices; 
	std::vector<std::shared_ptr<Texture>> textures;
	std::shared_ptr<Material> material;

	Mesh() : ebo(indices), vaoSetup(false) {};
	Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures);
	Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::shared_ptr<Material> mat);
	Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures, std::shared_ptr<Material> mat);

	ENGINE_API ~Mesh();
	void Draw(Shader& shader, const Camera& camera);

	Mesh(const Mesh& other) = delete;  // Prevent copying
	Mesh& operator=(const Mesh& other) = delete;  // Prevent assignment

	Mesh(Mesh&& other) noexcept
		: vertices(std::move(other.vertices)),
		indices(std::move(other.indices)),
		textures(std::move(other.textures)),
		material(std::move(other.material)),
		vao(std::move(other.vao)),
		ebo(std::move(other.ebo)),
		vaoSetup(other.vaoSetup) {
		other.vaoSetup = false;
	}

private:
	VAO vao;
	VBO vbo;
	EBO ebo;
	bool vaoSetup;
	void setupMesh();
};