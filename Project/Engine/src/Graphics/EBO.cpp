#include "pch.h"

#include "Graphics/EBO.h"

#ifdef ANDROID
#include <android/log.h>
#include <EGL/egl.h>
#endif

EBO::EBO(std::vector<GLuint>& sourceIndices)
	: ID(0), indices(sourceIndices), isSetup(false)
{
	// Don't create OpenGL buffers here - defer until first bind when we have context
}

EBO::~EBO()
{
	if (ID != 0) 
	{
		Delete();
	}
}

void EBO::Bind()
{
	Bind(indices);
}

void EBO::Bind(
	const std::vector<GLuint>& sourceIndices,
	bool prefer16Bit)
{
	if (!isSetup && ID == 0) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Setting up EBO for first time - %zu indices", sourceIndices.size());

		// Validate indices before upload
		if (sourceIndices.empty()) {
			//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Cannot setup EBO with empty indices vector!");
			return;
		}

		// Check if we have an active OpenGL context
		EGLDisplay display = eglGetCurrentDisplay();
		EGLContext context = eglGetCurrentContext();
		if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
			//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Trying to setup EBO without active OpenGL context!");
			return;
		}
#else
		// On desktop (Windows/Linux), check if GLFW context is current
		if (glfwGetCurrentContext() == NULL) {
			return; // Context not current, skip setup
		}
#endif
		glGenBuffers(1, &ID);
		if (ID == 0) {
			return;
		}
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Generated EBO ID: %u", ID);
#endif
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);

		const bool use16Bit =
			prefer16Bit &&
			*std::max_element(sourceIndices.begin(), sourceIndices.end()) <=
				static_cast<GLuint>(std::numeric_limits<GLushort>::max());
		const std::size_t indexSize =
			use16Bit ? sizeof(GLushort) : sizeof(GLuint);
		if (sourceIndices.size() >
			static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max()) /
				indexSize) {
			glDeleteBuffers(1, &ID);
			ID = 0;
			return;
		}
		const GLsizeiptr bufferSize = static_cast<GLsizeiptr>(
			sourceIndices.size() * indexSize);
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Buffer size calculation: %zu indices * %zu bytes = %ld total", sourceIndices.size(), sizeof(GLuint), bufferSize);
#endif

		if (use16Bit) {
			std::vector<GLushort> compactIndices;
			compactIndices.reserve(sourceIndices.size());
			for (GLuint index : sourceIndices) {
				compactIndices.push_back(static_cast<GLushort>(index));
			}
			glBufferData(
				GL_ELEMENT_ARRAY_BUFFER,
				bufferSize,
				compactIndices.data(),
				GL_STATIC_DRAW);
			indexType = GL_UNSIGNED_SHORT;
		}
		else {
			glBufferData(
				GL_ELEMENT_ARRAY_BUFFER,
				bufferSize,
				sourceIndices.data(),
				GL_STATIC_DRAW);
			indexType = GL_UNSIGNED_INT;
		}
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Uploaded %zu indices to EBO ID: %u", sourceIndices.size(), ID);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Error setting up EBO: 0x%x", error);
			glDeleteBuffers(1, &ID);
			ID = 0;
			indexType = GL_UNSIGNED_INT;
			return;
		}
#endif
		isSetup = true;
	}
	else if (ID != 0) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
	}
}

void EBO::Unbind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::Delete()
{
#ifdef ANDROID
	// Check if we have an active OpenGL context before deleting
	EGLDisplay display = eglGetCurrentDisplay();
	EGLContext context = eglGetCurrentContext();
	if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
		return; // Context not current, skip deletion
	}
#else
	// On desktop (Windows/Linux), check if GLFW context is current
	if (glfwGetCurrentContext() == NULL) {
		return; // Context not current, skip deletion
	}
#endif
	if (ID != 0 && glIsBuffer(ID)) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &ID);
		ID = 0;  // Prevent future invalid deletions
		indexType = GL_UNSIGNED_INT;
	}
}
