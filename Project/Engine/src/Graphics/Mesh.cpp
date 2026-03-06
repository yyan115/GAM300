#include "pch.h"

#include "Graphics/Mesh.h"
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <cassert>

#ifdef ANDROID
#include <android/log.h>
#endif
#include "Graphics/Instancing/InstanceBatch.hpp"

#pragma region Reflection
REFL_REGISTER_START(Mesh)
	//REFL_REGISTER_PROPERTY(vertices)
	REFL_REGISTER_PROPERTY(indices)
	//REFL_REGISTER_PROPERTY(textures)
	//REFL_REGISTER_PROPERTY(material)
REFL_REGISTER_END;
#pragma endregion

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures) : vertices(vertices), indices(indices), textures(textures), ebo(indices), vaoSetup(false)
{
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::shared_ptr<Material> mat) : vertices(vertices), indices(indices), material(mat), ebo(indices), vaoSetup(false)
{
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Constructor with material - material pointer=%p", material.get());
//#endif
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures, std::shared_ptr<Material> mat) :
	vertices(vertices), indices(indices), textures(textures), material(mat), ebo(indices), vaoSetup(false)
{
}

Mesh::~Mesh()
{
	vao.Delete();
	ebo.Delete();
}

void Mesh::DrawInstanced(Shader& shader, VBO& instanceVBO, GLsizei instanceCount)
{
	if (instanceCount == 0) {
		return;
	}

	if (!vaoSetup)
	{
		setupMesh();
		vaoSetup = true;
	}

	// Setup instance attributes if not already done
	SetupInstanceAttributes(instanceVBO);

	// Bind textures
	unsigned int diffuseNr = 1;
	unsigned int specularNr = 1;
	unsigned int normalNr = 1;
	unsigned int emissiveNr = 1;

	for (unsigned int i = 0; i < textures.size(); i++) 
	{
		glActiveTexture(GL_TEXTURE0 + i);

		std::string number;
		std::string name = textures[i]->GetType();

		if (name == "diffuse") 
		{
			number = std::to_string(diffuseNr++);
			shader.setBool("material.hasDiffuseMap", true);
			shader.setInt("material.diffuseMap", i);
		}
		else if (name == "specular") 
		{
			number = std::to_string(specularNr++);
			shader.setBool("material.hasSpecularMap", true);
			shader.setInt("material.specularMap", i);
		}
		else if (name == "normal") 
		{
			number = std::to_string(normalNr++);
			shader.setBool("material.hasNormalMap", true);
			shader.setInt("material.normalMap", i);
		}
		else if (name == "emissive") 
		{
			number = std::to_string(emissiveNr++);
			shader.setBool("material.hasEmissiveMap", true);
			shader.setInt("material.emissiveMap", i);
		}

		glBindTexture(GL_TEXTURE_2D, textures[i]->ID);
	}

	// Bind VAO and draw instanced
	vao.Bind();
	glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0, instanceCount);
	vao.Unbind();

	// Reset texture bindings
	glActiveTexture(GL_TEXTURE0);
}

void Mesh::DrawInstancedDepthOnly(VBO& instanceVBO, GLsizei instanceCount)
{
	if (instanceCount == 0) 
	{
		return;
	}

	if (!vaoSetup)
	{
		setupMesh();
		vaoSetup = true;
	}

	// Setup instance attributes if not already done
	SetupInstanceAttributes(instanceVBO);

	// Bind VAO and draw instanced (no textures or materials for depth pass)
	vao.Bind();
	glDrawElementsInstanced(GL_TRIANGLES,
		static_cast<GLsizei>(indices.size()),
		GL_UNSIGNED_INT,
		0,
		instanceCount);
	vao.Unbind();
}

void Mesh::setupMesh()
{
	// Generates Vertex Array Object and binds it
	vao.Bind();

	VBO vbo(vertices);
	vbo.Bind();

	ebo.Bind();

	// Position
	vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, position)); // Compiler knows the exact size of your Vertex struct (including any padding) no need 11 * sizeof(float)
	// Normal
	vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	// Color
	vao.LinkAttrib(vbo, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, color));
	// Texture
	vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, texUV));

	// Tangent (location = 4)
	vao.LinkAttrib(vbo, 4, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
	
	// Bone IDs
	vao.LinkAttribInt(vbo, 5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, mBoneIDs));

	// Weights
	vao.LinkAttrib(vbo, 6, 4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, mWeights));
	ENGINE_LOG_DEBUG("[MESH] sizeof(Vertex) = " + std::to_string(sizeof(Vertex)) + "\n");
	ENGINE_LOG_DEBUG("[MESH] offsetof = " + std::to_string(offsetof(Vertex, mBoneIDs)) + "\n");


	vbo.Unbind();
	vao.Unbind();
	ebo.Unbind();

	CalculateBoundingBox();
}

void Mesh::SetupInstanceAttributes(VBO& instanceVBO)
{
	if (instanceVBO.ID == m_instanceVBOId)
	{
		return;  // Already set up for this VBO
	}

	// Bind the mesh's VAO
	vao.Bind();

	// Bind the instance buffer
	instanceVBO.Bind(); 

	// Instance model matrix (mat4 = 4 vec4s, locations 7-10)
	// The model matrix is 64 bytes (16 floats), sent as 4 vec4 attributes

	// Location 7: model matrix column 0
	glEnableVertexAttribArray(7);
	glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),  // Stride = size of InstanceData struct
		(void*)0);             // Offset = 0 (start of modelMatrix)
	glVertexAttribDivisor(7, 1);  // Advance once per instance

	// Location 8: model matrix column 1
	glEnableVertexAttribArray(8);
	glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(1 * sizeof(glm::vec4)));
	glVertexAttribDivisor(8, 1);

	// Location 9: model matrix column 2
	glEnableVertexAttribArray(9);
	glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(2 * sizeof(glm::vec4)));
	glVertexAttribDivisor(9, 1);

	// Location 10: model matrix column 3
	glEnableVertexAttribArray(10);
	glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(3 * sizeof(glm::vec4)));
	glVertexAttribDivisor(10, 1);

	// Instance normal matrix (mat4 stored for alignment, only mat3 used in shader)
	// Offset starts after modelMatrix (64 bytes = 4 vec4s)
	size_t normalMatrixOffset = sizeof(glm::mat4);  // 64 bytes

	// Location 11: normal matrix column 0
	glEnableVertexAttribArray(11);
	glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)normalMatrixOffset);
	glVertexAttribDivisor(11, 1);

	// Location 12: normal matrix column 1
	glEnableVertexAttribArray(12);
	glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(normalMatrixOffset + sizeof(glm::vec4)));
	glVertexAttribDivisor(12, 1);

	// Location 13: normal matrix column 2
	glEnableVertexAttribArray(13);
	glVertexAttribPointer(13, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(normalMatrixOffset + 2 * sizeof(glm::vec4)));
	glVertexAttribDivisor(13, 1);

	// Instance bloom data (vec4: rgb=color, a=intensity)
	size_t bloomOffset = sizeof(glm::mat4) * 2;  // After modelMatrix + normalMatrix

	// Location 14: bloom data
	glEnableVertexAttribArray(14);
	glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)bloomOffset);
	glVertexAttribDivisor(14, 1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	vao.Unbind();

	m_instanceVBOId = instanceVBO.ID;
}

void Mesh::Draw(Shader& shader, const Camera& camera)
{
#ifdef __ANDROID__
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Starting Mesh::Draw - vertices.size=%zu, indices.size=%zu, shader.ID=%u", vertices.size(), indices.size(), shader.ID);

	// Basic validation
	if (vertices.empty()) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MESH] No vertices to draw, returning");
		return;
	}
	if (indices.empty()) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MESH] No indices to draw, returning");
		return;
	}
	if (shader.ID == 0) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MESH] Invalid shader ID, returning");
		return;
	}
#endif

	// Setup VAO on first draw when we have active OpenGL context
	if (!vaoSetup) 
	{
		setupMesh();
		vaoSetup = true;
	}

	shader.Activate();
	vao.Bind();

	// Set camera matrices
	glm::mat4 view = camera.GetViewMatrix();
	shader.setMat4("view", view);

	// Use GraphicsManager's viewport dimensions for correct aspect ratio
	// This ensures Scene Panel and Game Panel use their own viewport dimensions
	int vpWidth, vpHeight;
	GraphicsManager::GetInstance().GetViewportSize(vpWidth, vpHeight);
	if (vpWidth <= 0) vpWidth = 1;
	if (vpHeight <= 0) vpHeight = 1;
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)vpWidth / (float)vpHeight, 0.1f, 100.0f);
	shader.setMat4("projection", projection);
	shader.setVec3("cameraPos", camera.Position);

	// Apply material if available
	if (!material)
	{
		// Create default material if none exists
		material = Material::CreateDefault();
	}

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
			std::string type = textures[i]->GetType();

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

#ifdef __ANDROID__
	// Add extensive debugging for Android crash
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to draw mesh: indices.size=%zu, VAO.ID=%u", indices.size(), vao.ID);

	// Check if we have valid indices
	if (indices.empty()) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Empty indices vector!");
		return;
	}

	// Check if VAO is valid
	if (vao.ID == 0 || !glIsVertexArray(vao.ID)) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Invalid VAO ID: %u", vao.ID);
		return;
	}

	// Check if EBO is valid
	if (ebo.ID == 0 || !glIsBuffer(ebo.ID)) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Invalid EBO ID: %u", ebo.ID);
		return;
	}

	// Check if shader program is valid
	if (shader.ID == 0 || !glIsProgram(shader.ID)) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Invalid shader ID: %u", shader.ID);
		return;
	}

	// Check for any OpenGL errors before drawing
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "GL Error before DrawElements: 0x%x", err);
		return;
	}
#endif


	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);


#ifdef __ANDROID__
GLenum err2;
while ((err2 = glGetError()) != GL_NO_ERROR) {
    __android_log_print(ANDROID_LOG_ERROR, "GAM300", "GL Error after DrawElements: 0x%x (count=%zu, VAO=%u)",
                       err2, indices.size(), vao.ID);
}

//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Mesh::Draw completed successfully - drew %zu triangles", indices.size() / 3);
#endif
}

void Mesh::DrawDepthOnly()
{
	// Setup VAO on first draw if needed
	if (!vaoSetup) {
		setupMesh();
		vaoSetup = true;
	}

	vao.Bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
	vao.Unbind();
}

