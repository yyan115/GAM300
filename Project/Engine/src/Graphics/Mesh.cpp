#include "pch.h"

#include "Graphics/Mesh.h"
#include "WindowManager.hpp"
#include "Graphics/GraphicsManager.hpp"
#include <cassert>

#ifdef ANDROID
#include <android/log.h>
#include <glm/gtc/packing.hpp>
#endif
#include "Graphics/Instancing/InstanceBatch.hpp"

#ifdef ANDROID
namespace {
struct AndroidStaticVertex {
	glm::vec3 position;
	std::uint32_t normal;
	std::uint16_t texUV[2];
	std::uint32_t tangent;
};

struct AndroidSkinnedVertex {
	glm::vec3 position;
	std::uint32_t normal;
	std::uint16_t texUV[2];
	std::uint32_t tangent;
	std::uint8_t boneIDs[MaxBoneInfluences];
	std::uint16_t weights[MaxBoneInfluences];
};

static_assert(sizeof(AndroidStaticVertex) == 24);
static_assert(sizeof(AndroidSkinnedVertex) == 36);

std::uint32_t PackDirection(const glm::vec3& source)
{
	glm::vec3 direction = source;
	const float lengthSq = glm::dot(direction, direction);
	if (lengthSq > 1e-20f && std::isfinite(lengthSq)) {
		direction *= glm::inversesqrt(lengthSq);
	}
	else {
		direction = glm::vec3(0.0f);
	}

	// Signed normalized 10:10:10 is a standard mobile vertex format. The
	// shader normalizes again after interpolation, keeping angular error tiny.
	std::uint32_t packed = 0;
	for (int component = 0; component < 3; ++component) {
		const std::int32_t quantized = static_cast<std::int32_t>(std::lround(
			glm::clamp(direction[component], -1.0f, 1.0f) * 511.0f));
		packed |= (static_cast<std::uint32_t>(quantized) & 0x3FFu)
			<< (component * 10);
	}
	return packed;
}

std::uint16_t PackHalf(float value)
{
	return glm::packHalf1x16(glm::clamp(value, -65504.0f, 65504.0f));
}

std::uint16_t PackWeight(float value)
{
	return static_cast<std::uint16_t>(std::lround(
		glm::clamp(value, 0.0f, 1.0f) * 65535.0f));
}

bool HasBoneInfluences(const std::vector<Vertex>& vertices)
{
	for (const Vertex& vertex : vertices) {
		for (int influence = 0; influence < MaxBoneInfluences; ++influence) {
			if (vertex.mBoneIDs[influence] >= 0 &&
				vertex.mWeights[influence] > 0.0f) {
				return true;
			}
		}
	}
	return false;
}
}
#endif

#pragma region Reflection
REFL_REGISTER_START(Mesh)
	//REFL_REGISTER_PROPERTY(vertices)
	REFL_REGISTER_PROPERTY(indices)
	//REFL_REGISTER_PROPERTY(textures)
	//REFL_REGISTER_PROPERTY(material)
REFL_REGISTER_END;
#pragma endregion

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures) : vertices(vertices), indices(indices), textures(textures), vaoSetup(false)
{
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::shared_ptr<Material> mat) : vertices(vertices), indices(indices), material(mat), vaoSetup(false)
{
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Constructor with material - material pointer=%p", material.get());
//#endif
}

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, std::vector<std::shared_ptr<Texture>>& textures, std::shared_ptr<Material> mat) :
	vertices(vertices), indices(indices), textures(textures), material(mat), vaoSetup(false)
{
}

Mesh::Mesh(std::vector<Vertex>&& sourceVertices, std::vector<GLuint>&& sourceIndices, std::shared_ptr<Material> mat) :
	vertices(std::move(sourceVertices)),
	indices(std::move(sourceIndices)),
	material(std::move(mat)),
	vaoSetup(false)
{
}

Mesh::~Mesh()
{
	vao.Delete();
	vertexVBO.Delete();
	ebo.Delete();
}

void Mesh::Prewarm()
{
	// If it hasn't been sent to the GPU yet, do it now!
	if (!vaoSetup) {
		setupMesh();
		vaoSetup = true;
	}
}

#ifdef ANDROID
void Mesh::ReleaseCPUVertexData()
{
	if (!vaoSetup || vertices.empty()) {
		return;
	}

	m_collisionPositions.clear();
	m_collisionPositions.reserve(vertices.size());
	for (const Vertex& vertex : vertices) {
		m_collisionPositions.push_back(vertex.position);
	}

	// clear() retains the large full-vertex allocation. Swapping releases it
	// after the compact collision positions have been populated.
	std::vector<Vertex>().swap(vertices);
}
#endif

void Mesh::DrawPrewarmTriangle()
{
	if (indices.size() < 3) {
		return;
	}

	Prewarm();
	vao.Bind();
	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawElements(GL_TRIANGLES, 3, ebo.GetIndexType(), nullptr);
}

void Mesh::DrawInstanced(Shader& shader, VBO& instanceVBO, GLsizei instanceCount, bool bindLegacyTextures)
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
	for (unsigned int i = 0; bindLegacyTextures && i < textures.size(); i++)
	{
		if (!textures[i]) {
			continue;
		}

		const std::string& name = textures[i]->GetType();

		if (name == "diffuse") 
		{
			shader.setBool("material.hasDiffuseMap", true);
			shader.setInt("material.diffuseMap", i);
		}
		else if (name == "specular") 
		{
			shader.setBool("material.hasSpecularMap", true);
			shader.setInt("material.specularMap", i);
		}
		else if (name == "normal") 
		{
			shader.setBool("material.hasNormalMap", true);
			shader.setInt("material.normalMap", i);
		}
		else if (name == "emissive") 
		{
			shader.setBool("material.hasEmissiveMap", true);
			shader.setInt("material.emissiveMap", i);
		}

		textures[i]->Bind(i);
	}

	// Bind VAO and draw instanced
	vao.Bind();
	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawElementsInstanced(
		GL_TRIANGLES,
		static_cast<GLsizei>(indices.size()),
		ebo.GetIndexType(),
		0,
		instanceCount);

	if (bindLegacyTextures) {
		glActiveTexture(GL_TEXTURE0);
	}
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
	PROFILE_COUNT("GL::DepthDrawCalls", 1);
	glDrawElementsInstanced(GL_TRIANGLES,
		static_cast<GLsizei>(indices.size()),
		ebo.GetIndexType(),
		0,
		instanceCount);
}

void Mesh::setupMesh()
{
	// Generates Vertex Array Object and binds it
	vao.Bind();

	// Mesh already owns this CPU index vector. Upload directly from it instead
	// of retaining an identical second copy inside EBO for the model lifetime.
#ifdef ANDROID
	ebo.Bind(indices, true);
#else
	ebo.Bind(indices);
#endif

#ifdef ANDROID
	// Mobile's built-in mesh shaders do not consume vertex colors. Use compact
	// direction/UV formats and omit skinning data entirely for static geometry.
	const bool hasBoneInfluences = HasBoneInfluences(vertices);
	if (hasBoneInfluences) {
		std::vector<AndroidSkinnedVertex> mobileVertices;
		mobileVertices.reserve(vertices.size());
		for (const Vertex& vertex : vertices) {
			AndroidSkinnedVertex mobileVertex{};
			mobileVertex.position = vertex.position;
			mobileVertex.normal = PackDirection(vertex.normal);
			mobileVertex.texUV[0] = PackHalf(vertex.texUV.x);
			mobileVertex.texUV[1] = PackHalf(vertex.texUV.y);
			mobileVertex.tangent = PackDirection(vertex.tangent);
			for (int influence = 0; influence < MaxBoneInfluences; ++influence) {
				const int boneID = vertex.mBoneIDs[influence];
				// The mobile shader supports 100 bones. Preserve valid IDs
				// exactly and map every invalid/unsupported ID to its sentinel.
				mobileVertex.boneIDs[influence] =
					boneID >= 0 && boneID < 100
					? static_cast<std::uint8_t>(boneID)
					: std::uint8_t{255};
				mobileVertex.weights[influence] =
					PackWeight(vertex.mWeights[influence]);
			}
			mobileVertices.push_back(mobileVertex);
		}

		vertexVBO = VBO(
			mobileVertices.data(),
			mobileVertices.size() * sizeof(AndroidSkinnedVertex),
			GL_STATIC_DRAW);

		vao.LinkAttrib(vertexVBO, 0, 3, GL_FLOAT, sizeof(AndroidSkinnedVertex),
			(void*)offsetof(AndroidSkinnedVertex, position));
		vao.LinkAttribNormalized(vertexVBO, 1, 4, GL_INT_2_10_10_10_REV, sizeof(AndroidSkinnedVertex),
			(void*)offsetof(AndroidSkinnedVertex, normal));
		vao.LinkAttrib(vertexVBO, 3, 2, GL_HALF_FLOAT, sizeof(AndroidSkinnedVertex),
			(void*)offsetof(AndroidSkinnedVertex, texUV));
		vao.LinkAttribNormalized(vertexVBO, 4, 4, GL_INT_2_10_10_10_REV, sizeof(AndroidSkinnedVertex),
			(void*)offsetof(AndroidSkinnedVertex, tangent));
		vao.LinkAttribInt(vertexVBO, 5, 4, GL_UNSIGNED_BYTE, sizeof(AndroidSkinnedVertex),
			(void*)offsetof(AndroidSkinnedVertex, boneIDs));
		vao.LinkAttribNormalized(vertexVBO, 6, 4, GL_UNSIGNED_SHORT, sizeof(AndroidSkinnedVertex),
			(void*)offsetof(AndroidSkinnedVertex, weights));
	}
	else {
		std::vector<AndroidStaticVertex> mobileVertices;
		mobileVertices.reserve(vertices.size());
		for (const Vertex& vertex : vertices) {
			AndroidStaticVertex mobileVertex{};
			mobileVertex.position = vertex.position;
			mobileVertex.normal = PackDirection(vertex.normal);
			mobileVertex.texUV[0] = PackHalf(vertex.texUV.x);
			mobileVertex.texUV[1] = PackHalf(vertex.texUV.y);
			mobileVertex.tangent = PackDirection(vertex.tangent);
			mobileVertices.push_back(mobileVertex);
		}

		vertexVBO = VBO(
			mobileVertices.data(),
			mobileVertices.size() * sizeof(AndroidStaticVertex),
			GL_STATIC_DRAW);

		vao.LinkAttrib(vertexVBO, 0, 3, GL_FLOAT, sizeof(AndroidStaticVertex),
			(void*)offsetof(AndroidStaticVertex, position));
		vao.LinkAttribNormalized(vertexVBO, 1, 4, GL_INT_2_10_10_10_REV, sizeof(AndroidStaticVertex),
			(void*)offsetof(AndroidStaticVertex, normal));
		vao.LinkAttrib(vertexVBO, 3, 2, GL_HALF_FLOAT, sizeof(AndroidStaticVertex),
			(void*)offsetof(AndroidStaticVertex, texUV));
		vao.LinkAttribNormalized(vertexVBO, 4, 4, GL_INT_2_10_10_10_REV, sizeof(AndroidStaticVertex),
			(void*)offsetof(AndroidStaticVertex, tangent));
	}
#else
	vertexVBO = VBO(vertices);

	// Position
	vao.LinkAttrib(vertexVBO, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, position)); // Compiler knows the exact size of your Vertex struct (including any padding) no need 11 * sizeof(float)
	// Normal
	vao.LinkAttrib(vertexVBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	// Color
	vao.LinkAttrib(vertexVBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, color));
	// Texture
	vao.LinkAttrib(vertexVBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, texUV));

	// Tangent (location = 4)
	vao.LinkAttrib(vertexVBO, 4, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
	
	// Bone IDs
	vao.LinkAttribInt(vertexVBO, 5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, mBoneIDs));

	// Weights
	vao.LinkAttrib(vertexVBO, 6, 4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, mWeights));
	ENGINE_LOG_DEBUG("[MESH] sizeof(Vertex) = " + std::to_string(sizeof(Vertex)) + "\n");
	ENGINE_LOG_DEBUG("[MESH] offsetof = " + std::to_string(offsetof(Vertex, mBoneIDs)) + "\n");
#endif

	vertexVBO.Unbind();
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

	// Instance model transform. Android packs affine translation into the .w
	// components of the three basis columns; desktop keeps a conventional mat4.

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

#ifndef ANDROID
	// Location 10: model matrix column 3
	glEnableVertexAttribArray(10);
	glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(3 * sizeof(glm::vec4)));
	glVertexAttribDivisor(10, 1);
#endif

	// Instance normal matrix: three tightly packed vec3 columns.
	const size_t normalMatrixOffset = offsetof(InstanceData, normalMatrixColumns);

	// Location 11: normal matrix column 0
	glEnableVertexAttribArray(11);
	#ifdef ANDROID
	glVertexAttribPointer(11, 3, GL_HALF_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)normalMatrixOffset);
	#else
	glVertexAttribPointer(11, 3, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)normalMatrixOffset);
	#endif
	glVertexAttribDivisor(11, 1);

	// Location 12: normal matrix column 1
	glEnableVertexAttribArray(12);
	#ifdef ANDROID
	glVertexAttribPointer(12, 3, GL_HALF_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(normalMatrixOffset + 3 * sizeof(std::uint16_t)));
	#else
	glVertexAttribPointer(12, 3, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(normalMatrixOffset + 3 * sizeof(float)));
	#endif
	glVertexAttribDivisor(12, 1);

	// Location 13: normal matrix column 2
	glEnableVertexAttribArray(13);
	#ifdef ANDROID
	glVertexAttribPointer(13, 3, GL_HALF_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(normalMatrixOffset + 6 * sizeof(std::uint16_t)));
	#else
	glVertexAttribPointer(13, 3, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)(normalMatrixOffset + 6 * sizeof(float)));
	#endif
	glVertexAttribDivisor(13, 1);

	// Instance bloom data (vec4: rgb=color, a=intensity)
	const size_t bloomOffset = offsetof(InstanceData, bloomData);

	// Location 14: bloom data
	glEnableVertexAttribArray(14);
	#ifdef ANDROID
	glVertexAttribPointer(14, 4, GL_HALF_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)bloomOffset);
	#else
	glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE,
		sizeof(InstanceData),
		(void*)bloomOffset);
	#endif
	glVertexAttribDivisor(14, 1);

	// Per-instance packed point/spot-light mask (locations 0-14 are already used).
	glEnableVertexAttribArray(15);
	glVertexAttribIPointer(15, 1, GL_UNSIGNED_INT,
		sizeof(InstanceData),
		(void*)offsetof(InstanceData, lightMask));
	glVertexAttribDivisor(15, 1);

	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	vao.Unbind();

	m_instanceVBOId = instanceVBO.ID;
}

void Mesh::Draw(Shader& shader, const Camera& camera)
{
#ifdef __ANDROID__
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Starting Mesh::Draw - vertices.size=%zu, indices.size=%zu, shader.ID=%u", vertices.size(), indices.size(), shader.ID);

	// Basic validation
	if (GetVertexCount() == 0) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MESH] No vertices to draw, returning");
		return;
	}
	if (indices.empty()) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MESH] No indices to draw, returning");
		return;
	}
	if (shader.ID == 0) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MESH] Invalid shader ID, returning");
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

	if (!shader.UsesCameraBlock()) {
		// Legacy/custom shaders receive camera uniforms directly. Built-in shaders
		// consume the CameraBlock uploaded once by GraphicsManager.
		glm::mat4 view = camera.GetViewMatrix();
		shader.setMat4("view", view);

		int vpWidth, vpHeight;
		GraphicsManager::GetInstance().GetViewportSize(vpWidth, vpHeight);
		if (vpWidth <= 0) vpWidth = 1;
		if (vpHeight <= 0) vpHeight = 1;
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)vpWidth / (float)vpHeight, 0.1f, GraphicsManager::GetInstance().GetFarPlane());
		shader.setMat4("projection", projection);
		shader.setVec3("cameraPos", camera.Position);
	}

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
			const std::string& type = textures[i]->GetType();

			if (type == "diffuse") {
				num = std::to_string(numDiffuse++);
			}
			else if (type == "specular") {
				num = std::to_string(numSpecular++);
			}

			textures[i]->Bind(textureUnit);
			shader.setInt(("material." + type + num).c_str(), textureUnit);
			textureUnit++;
		}
	}

#if defined(__ANDROID__) && defined(GAM300_GL_VALIDATION)
	// Android driver validation is useful during crash investigation, but these
	// GL queries are synchronization points and must be explicitly enabled.
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "About to draw mesh: indices.size=%zu, VAO.ID=%u", indices.size(), vao.ID);

	// Check if we have valid indices
	if (indices.empty()) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Empty indices vector!");
		return;
	}

	// Check if VAO is valid
	if (vao.ID == 0 || !glIsVertexArray(vao.ID)) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Invalid VAO ID: %u", vao.ID);
		return;
	}

	// Check if EBO is valid
	if (ebo.ID == 0 || !glIsBuffer(ebo.ID)) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Invalid EBO ID: %u", ebo.ID);
		return;
	}

	// Check if shader program is valid
	if (shader.ID == 0 || !glIsProgram(shader.ID)) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "ERROR: Invalid shader ID: %u", shader.ID);
		return;
	}

	// Check for any OpenGL errors before drawing
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "GL Error before DrawElements: 0x%x", err);
		return;
	}
#endif


	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(indices.size()),
		ebo.GetIndexType(),
		0);


#if defined(__ANDROID__) && defined(GAM300_GL_VALIDATION)
	GLenum err2;
	while ((err2 = glGetError()) != GL_NO_ERROR) {
    //__android_log_print(ANDROID_LOG_ERROR, "GAM300", "GL Error after DrawElements: 0x%x (count=%zu, VAO=%u)",
    //                   err2, indices.size(), vao.ID);
}

//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MESH] Mesh::Draw completed successfully - drew %zu triangles", indices.size() / 3);
#endif
}

void Mesh::DrawGeometryOnly()
{
	if (!vaoSetup) {
		setupMesh();
		vaoSetup = true;
	}

	vao.Bind();
	PROFILE_COUNT("GL::DrawCalls", 1);
	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(indices.size()),
		ebo.GetIndexType(),
		0);
}

void Mesh::DrawDepthOnly()
{
	// Setup VAO on first draw if needed
	if (!vaoSetup) {
		setupMesh();
		vaoSetup = true;
	}

	vao.Bind();
	PROFILE_COUNT("GL::DepthDrawCalls", 1);
	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(indices.size()),
		ebo.GetIndexType(),
		0);
}
