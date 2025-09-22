#include "pch.h"

#include "Graphics/Mesh.h"
#include "WindowManager.hpp"


Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures) : vertices(vertices), indices(indices), textures(textures), ebo(indices), vaoSetup(false)
{
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::shared_ptr<Material> mat) : vertices(vertices), indices(indices), material(mat), ebo(indices), vaoSetup(false)
{
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
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up mesh with VAO ID: %u", vao.ID);
#endif
	// Generates Vertex Array Object and binds it
	vao.Bind();

	// Initialize member VBO with vertices data
	vbo.vertices = vertices;
	vbo.Bind();

	ebo.Bind();

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to setup vertex attributes for VAO %u", vao.ID);
#endif

	// Position
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up position attribute (layout 0) for VAO %u", vao.ID);
#endif
	vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0); // Compiler knows the exact size of your Vertex struct (including any padding) no need 11 * sizeof(float)

	// Normal
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up normal attribute (layout 1) for VAO %u", vao.ID);
#endif
	vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));

	// Color
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up color attribute (layout 2) for VAO %u", vao.ID);
#endif
	vao.LinkAttrib(vbo, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));

	// Texture
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Setting up texture attribute (layout 3) for VAO %u", vao.ID);
#endif
	vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Finished setting up mesh for VAO %u", vao.ID);
#endif

	vbo.Unbind();
	vao.Unbind();
	ebo.Unbind();
}

void Mesh::Draw(Shader& shader, const Camera& camera)
{
	// Setup VAO on first draw when we have active OpenGL context
	if (!vaoSetup) {
		setupMesh();
		vaoSetup = true;
	}

	shader.Activate();
	vao.Bind();

	// Note: model matrix should already be set by the render system
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
		material->applyToShader(shader);
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
			textures[i]->Bind();
			shader.setInt(("material." + type + num).c_str(), textureUnit);
			textureUnit++;
		}
	}

	//assert(glIsProgram(shader.ID));
	//assert(glIsVertexArray(vao.ID));
	//assert(glIsTexture(textures[0]->ID));

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to draw VAO %u with %zu indices", vao.ID, indices.size());
	// Validate draw parameters
	if (indices.empty()) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "VAO %u has empty indices array!", vao.ID);
		return;
	}
	if (vertices.empty()) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "VAO %u has empty vertices array!", vao.ID);
		return;
	}
#endif

	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Drew VAO %u successfully", vao.ID);
#endif

#ifdef ANDROID
GLenum err;
while ((err = glGetError()) != GL_NO_ERROR) {
    __android_log_print(ANDROID_LOG_ERROR, "GAM300", "GL Error after DrawElements: 0x%x (count=%zu, VAO=%u)",
                       err, indices.size(), vao.ID);
}
#endif
}

