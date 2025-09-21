#include "pch.h"

#include "Graphics/Mesh.h"
#include "WindowManager.hpp"
#include <cassert>


Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures) : vertices(vertices), indices(indices), textures(textures)
{
	setupMesh();
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::shared_ptr<Material> mat) : vertices(vertices), indices(indices), material(mat)
{
	setupMesh();
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures, std::shared_ptr<Material> mat) :
	vertices(vertices), indices(indices), textures(textures), material(mat)
{
	setupMesh();
}

Mesh::~Mesh()
{
	vao.Delete();
}

void Mesh::setupMesh()
{
	// Generates Vertex Array Object and binds it
	vao.Bind();

	VBO vbo(vertices);
	vbo.Bind();

	EBO ebo(indices);

	// Position
	vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0); // Compiler knows the exact size of your Vertex struct (including any padding) no need 11 * sizeof(float)
	// Normal
	vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	// Color
	vao.LinkAttrib(vbo, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	// Texture
	vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	vao.Unbind();
	vbo.Unbind();
	ebo.Unbind();
}

void Mesh::Draw(Shader& shader, const Camera& camera)
{
	shader.Activate();
	vao.Bind();

	// Set camera matrices
	glm::mat4 view = camera.GetViewMatrix();
	shader.setMat4("view", view);

	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)WindowManager::GetViewportWidth() / (float)WindowManager::GetViewportHeight(), 0.1f, 100.0f);
	shader.setMat4("projection", projection);
	shader.setVec3("cameraPos", camera.Position);

	// Apply material if available
	if (material)
	{
		//material->debugPrintProperties();
		material->ApplyToShader(shader);
	}
	else
	{
		// Fallback to old texture system for backward compatibility
		unsigned int textureUnit = 0;
		unsigned int numDiffuse = 0, numSpecular = 0;

		for (unsigned int i = 0; i < textures.size() && textureUnit < 16; i++)
		{
			if (!textures[i]) continue;

			std::string num;
			std::string type = textures[i]->type;

			if (type == "diffuse") {
				num = std::to_string(numDiffuse++);
			}
			else if (type == "specular") {
				num = std::to_string(numSpecular++);
			}

			glActiveTexture(GL_TEXTURE0 + textureUnit);
			textures[i]->Bind(textureUnit);
			shader.setInt(("material." + type + num).c_str(), textureUnit);
			textureUnit++;
		}
	}

	//assert(glIsProgram(shader.ID));
	//assert(glIsVertexArray(vao.ID));
	//assert(glIsTexture(textures[0]->ID));

	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
}

