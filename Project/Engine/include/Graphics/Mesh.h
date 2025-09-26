#pragma once

#include "VAO.h"
#include "EBO.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.hpp"
#include "Engine.h"

#include "Reflection/ReflectionBase.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif

class Mesh {
public:
	REFL_SERIALIZABLE
	std::vector<Vertex> vertices; 
	std::vector<GLuint> indices; 
	std::vector<std::shared_ptr<Texture>> textures;
	std::shared_ptr<Material> material;

	Mesh() : vaoSetup(false), ebo(indices) {};
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
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Move constructor - moved material pointer from %p to %p", other.material.get(), material.get());
#endif
	}

private:
	VAO vao;
	EBO ebo;
	bool vaoSetup;
	void setupMesh();
};