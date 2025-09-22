#include "pch.h"

#include "Graphics/EBO.h"
#ifdef ANDROID
#include <android/log.h>
#endif

EBO::EBO(std::vector<GLuint>& indices) : indices(indices)
{
	// Don't generate buffer here - defer until first bind when we have OpenGL context
}

void EBO::setupBuffer()
{
#ifdef ANDROID
	// Check if we have an active OpenGL context before generating buffer
	EGLDisplay display = eglGetCurrentDisplay();
	EGLContext context = eglGetCurrentContext();
	if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Trying to generate EBO without active OpenGL context!");
		return;
	}
#endif

	glGenBuffers(1, &ID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Generated EBO ID: %u with %zu indices", ID, indices.size());
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error generating EBO: 0x%x", error);
	}
#endif
}

void EBO::Bind()
{
	if (ID == 0) {
		setupBuffer();
	}

	if (ID != 0) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
#ifdef ANDROID
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error binding EBO %u: 0x%x", ID, error);
		}
#endif
	}
}

void EBO::Unbind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::Delete()
{
	if (ID != 0) {
		glDeleteBuffers(1, &ID);
		ID = 0;
	}
}
