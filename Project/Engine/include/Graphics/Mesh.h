#pragma once

#include "VAO.h"
#include "EBO.h"
#include "Camera/Camera.hpp"
#include "Texture.h"
#include "Material.hpp"
#include "Engine.h"
#include "Frustum/Frustum.hpp"
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
	void Prewarm(); // forces the setup to happen
	void Draw(Shader& shader, const Camera& camera);
	void DrawGeometryOnly();
	void DrawDepthOnly();

	Mesh(const Mesh& other)
		: vertices(other.vertices),
		indices(other.indices),
		textures(other.textures),
		material(other.material),
		vao(),
		ebo(indices),
		vaoSetup(false),
		boundingBox(other.boundingBox) {
		//setupMesh();
	}

	Mesh& operator=(const Mesh& other) {
		if (this != &other) {
			// Clean up existing resources
			vao.Delete();
			ebo.Delete();

			// Copy data
			vertices = other.vertices;
			indices = other.indices;
			textures = other.textures;
			material = other.material;
			vaoSetup = false;
			boundingBox = other.boundingBox;

			// Reconstruct EBO with new indices
			ebo = EBO(indices);
			//setupMesh();
		}
		return *this;
	}

	Mesh(Mesh&& other) noexcept
		: vertices(std::move(other.vertices)),
		indices(std::move(other.indices)),
		textures(std::move(other.textures)),
		material(std::move(other.material)),
		vao(std::move(other.vao)),
		ebo(std::move(other.ebo)),
		vaoSetup(other.vaoSetup), 
		boundingBox(other.boundingBox) {
		other.vaoSetup = false;
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Move constructor - moved material pointer from %p to %p", other.material.get(), material.get());
#endif
	}

	AABB GetBoundingBox() const { return boundingBox; }

	// Call this after loading vertices (in ProcessMesh or setupMesh)
	void CalculateBoundingBox() 
	{
		if (vertices.empty()) 
		{
			boundingBox = AABB(glm::vec3(0.0f), glm::vec3(0.0f));
			return;
		}

		glm::vec3 min(FLT_MAX);
		glm::vec3 max(-FLT_MAX);

		for (const auto& vertex : vertices) 
		{
			min = glm::min(min, vertex.position);
			max = glm::max(max, vertex.position);
		}

		boundingBox = AABB(min, max);
	}

	// Instanced rendering methods
	void DrawInstanced(Shader& shader, VBO& instanceVBO, GLsizei instanceCount);
	void DrawInstancedDepthOnly(VBO& instanceVBO, GLsizei instanceCount);

	VAO vao;

private:
	EBO ebo;
	bool vaoSetup;
	void setupMesh();
	AABB boundingBox;
	GLuint m_instanceVBOId = 0;
	void SetupInstanceAttributes(VBO& instanceVBO);
};