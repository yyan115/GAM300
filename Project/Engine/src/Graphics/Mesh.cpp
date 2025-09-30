#include "pch.h"

#include "Graphics/Mesh.h"
#include "WindowManager.hpp"
#include <cassert>

#ifdef ANDROID
#include <android/log.h>
#endif

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

void Mesh::setupMesh()
{
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Starting setupMesh - vertices=%zu, indices=%zu", vertices.size(), indices.size());
//#endif

	// Generates Vertex Array Object and binds it
	vao.Bind();
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] VAO bound - VAO.ID=%u", vao.ID);
//#endif

	VBO vbo(vertices);
	vbo.Bind();
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] VBO created and bound - VBO.ID=%u", vbo.ID);
//#endif

	ebo.Bind();
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] EBO bound - EBO.ID=%u", ebo.ID);
//#endif

	// Position
	vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0); // Compiler knows the exact size of your Vertex struct (including any padding) no need 11 * sizeof(float)
	// Normal
	vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	// Color
	vao.LinkAttrib(vbo, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	// Texture
	vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Vertex attributes linked successfully");
//#endif

	vbo.Unbind();
	vao.Unbind();
	ebo.Unbind();

//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] setupMesh completed successfully");
//#endif
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
	if (!vaoSetup) {
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Setting up mesh VAO/VBO for first time");
//#endif
		setupMesh();
		vaoSetup = true;
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] VAO/VBO setup completed - VAO.ID=%u, EBO.ID=%u", vao.ID, ebo.ID);
//#endif
	}

//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] About to activate shader - shader.ID=%u", shader.ID);
//#endif
	shader.Activate();
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Shader activated successfully");
//#endif

//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] About to bind VAO - VAO.ID=%u", vao.ID);
//#endif
	vao.Bind();
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] VAO bound successfully");
//#endif

	// Set camera matrices
	glm::mat4 view = camera.GetViewMatrix();
	shader.setMat4("view", view);

	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)WindowManager::GetViewportWidth() / (float)WindowManager::GetViewportHeight(), 0.1f, 100.0f);
	shader.setMat4("projection", projection);
	shader.setVec3("cameraPos", camera.Position);

	// Apply material if available
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] About to apply material - material=%p", material.get());
//#endif
	if (!material)
	{
		// Create default material if none exists
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] No material found, creating default material");
//#endif
		material = Material::CreateDefault();
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Created default material for rendering: %p", material.get());
//#endif
	}

	if (material)
	{
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Applying material to shader");
//#endif
		//material->debugPrintProperties();
		material->ApplyToShader(shader);
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Material applied successfully");
//#endif
	}
	else
	{
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Material creation failed, using fallback texture system");
//#endif
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

//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] About to call glDrawElements with %zu indices", indices.size());
//#endif

	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

	//std::cout << "drawn mesh\n";
	
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] glDrawElements called successfully");
//#endif

#ifdef __ANDROID__
GLenum err2;
while ((err2 = glGetError()) != GL_NO_ERROR) {
    __android_log_print(ANDROID_LOG_ERROR, "GAM300", "GL Error after DrawElements: 0x%x (count=%zu, VAO=%u)",
                       err2, indices.size(), vao.ID);
}

//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Mesh::Draw completed successfully - drew %zu triangles", indices.size() / 3);
#endif
}

