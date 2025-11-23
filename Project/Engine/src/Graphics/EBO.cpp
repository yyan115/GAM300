#include "pch.h"

#include "Graphics/EBO.h"

#ifdef ANDROID
#include <android/log.h>
#include <EGL/egl.h>
#endif

EBO::EBO(std::vector<GLuint>& indices) : indices(indices), isSetup(false), ID(0)
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
	if (!isSetup && ID == 0) {
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Setting up EBO for first time - %zu indices", indices.size());

		// Validate indices before upload
		if (indices.empty()) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Cannot setup EBO with empty indices vector!");
			return;
		}
		if (indices.size() > 1000000) {  // Sanity check
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Indices count %zu seems corrupted, aborting EBO setup", indices.size());
			return;
		}

		// Check if we have an active OpenGL context
		EGLDisplay display = eglGetCurrentDisplay();
		EGLContext context = eglGetCurrentContext();
		if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Trying to setup EBO without active OpenGL context!");
			return;
		}
#endif
		glGenBuffers(1, &ID);
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Generated EBO ID: %u", ID);
#endif
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);

		GLsizeiptr bufferSize = indices.size() * sizeof(GLuint);
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Buffer size calculation: %zu indices * %zu bytes = %ld total", indices.size(), sizeof(GLuint), bufferSize);
#endif

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, bufferSize, indices.data(), GL_STATIC_DRAW);
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[EBO] Uploaded %zu indices to EBO ID: %u", indices.size(), ID);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[EBO] Error setting up EBO: 0x%x", error);
			return;
		}
#endif
		isSetup = true;
	} else if (ID != 0) {
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
		glDeleteBuffers(1, &ID);
		ID = 0;
		isSetup = false;
}
