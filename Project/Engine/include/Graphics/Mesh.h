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

	Mesh() = default;
	Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures);
	Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::shared_ptr<Material> mat);
	Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures, std::shared_ptr<Material> mat);
	Mesh(std::vector<Vertex>&& vertices, std::vector<GLuint>&& indices, std::shared_ptr<Material> mat);

	ENGINE_API ~Mesh();
	void Prewarm(); // forces the setup to happen
	void DrawPrewarmTriangle();
	void Draw(Shader& shader, const Camera& camera);
	void DrawGeometryOnly();
	void DrawDepthOnly();

	Mesh(const Mesh& other)
		: vertices(other.vertices),
		indices(other.indices),
		textures(other.textures),
		material(other.material),
		vao(),
		vaoSetup(false),
		boundingBox(other.boundingBox)
#ifdef ANDROID
		, m_collisionPositions(other.m_collisionPositions)
#endif
	{
		//setupMesh();
	}

	Mesh& operator=(const Mesh& other) {
		if (this != &other) {
			// Clean up existing resources
			vao.Delete();
			vertexVBO.Delete();
			ebo.Delete();

			// Copy data
			vertices = other.vertices;
			indices = other.indices;
			textures = other.textures;
			material = other.material;
			vaoSetup = false;
			boundingBox = other.boundingBox;
#ifdef ANDROID
			m_collisionPositions = other.m_collisionPositions;
#endif

			ebo = EBO();
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
		vertexVBO(std::move(other.vertexVBO)),
		ebo(std::move(other.ebo)),
		vaoSetup(other.vaoSetup), 
		boundingBox(other.boundingBox)
#ifdef ANDROID
		, m_collisionPositions(std::move(other.m_collisionPositions))
#endif
		, m_instanceVBOId(other.m_instanceVBOId) {
		other.vaoSetup = false;
		other.m_instanceVBOId = 0;
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Move constructor - moved material pointer from %p to %p", other.material.get(), material.get());
#endif
	}

	AABB GetBoundingBox() const { return boundingBox; }
	std::size_t GetVertexCount() const noexcept
	{
#ifdef ANDROID
		return vertices.empty() ? m_collisionPositions.size() : vertices.size();
#else
		return vertices.size();
#endif
	}
	const glm::vec3& GetCollisionPosition(std::size_t index) const noexcept
	{
#ifdef ANDROID
		if (vertices.empty()) {
			return m_collisionPositions[index];
		}
#endif
		return vertices[index].position;
	}
#ifdef ANDROID
	// Once compact GPU vertices exist, retain only exact positions for possible
	// runtime mesh-collider creation instead of every render-only CPU attribute.
	void ReleaseCPUVertexData();
#endif

	// Call this after loading vertices (in ProcessMesh or setupMesh)
	void CalculateBoundingBox() 
	{
		const std::size_t vertexCount = GetVertexCount();
		if (vertexCount == 0)
		{
			boundingBox = AABB(glm::vec3(0.0f), glm::vec3(0.0f));
			return;
		}

		glm::vec3 min(FLT_MAX);
		glm::vec3 max(-FLT_MAX);

		for (std::size_t index = 0; index < vertexCount; ++index)
		{
			const glm::vec3& position = GetCollisionPosition(index);
			min = glm::min(min, position);
			max = glm::max(max, position);
		}

		boundingBox = AABB(min, max);
	}

	// Instanced rendering methods
	void DrawInstanced(Shader& shader, VBO& instanceVBO, GLsizei instanceCount, bool bindLegacyTextures);
	void DrawInstancedDepthOnly(VBO& instanceVBO, GLsizei instanceCount);
	void InvalidateInstanceAttributes() noexcept { m_instanceVBOId = 0; }

	VAO vao;

private:
	VBO vertexVBO;
	EBO ebo;
	bool vaoSetup = false;
	void setupMesh();
	AABB boundingBox;
#ifdef ANDROID
	std::vector<glm::vec3> m_collisionPositions;
#endif
	GLuint m_instanceVBOId = 0;
	void SetupInstanceAttributes(VBO& instanceVBO);
};
