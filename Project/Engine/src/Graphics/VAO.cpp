#include "pch.h"

#include "Graphics/VAO.h"
#ifdef ANDROID
#include <android/log.h>
#endif

VAO::VAO()
{
	//glGenVertexArrays(1, &ID);
}

void VAO::LinkAttrib(VBO VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset)
{
	VBO.Bind();
#ifdef ANDROID
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error before glVertexAttribPointer for layout %u: 0x%x", layout, error);
	}
#endif

	glVertexAttribPointer(layout, numComponents, type, GL_FALSE, stride, offset);

#ifdef ANDROID
	error = glGetError();
	if (error != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error in glVertexAttribPointer(layout=%u, numComponents=%u, stride=%ld, offset=%p): 0x%x",
			layout, numComponents, stride, offset, error);
	}
#endif

	glEnableVertexAttribArray(layout);

#ifdef ANDROID
	error = glGetError();
	if (error != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error in glEnableVertexAttribArray for layout %u: 0x%x", layout, error);
	}
#endif

	VBO.Unbind();
}

void VAO::Bind()
{
	if (ID == 0) {
#ifdef ANDROID
		// Check if we have an active OpenGL context before generating VAO
		EGLDisplay display = eglGetCurrentDisplay();
		EGLContext context = eglGetCurrentContext();
		if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Trying to generate VAO without active OpenGL context!");
			return;
		}
#endif
		glGenVertexArrays(1, &ID);  // Temp workaround for refactoring - pulled out Mesh that uses VAO, but need default constructor, but
		// genVertex will crash if glfw isn't init yet, so we will init only when binding
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "Generated VAO ID: %u", ID);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error generating VAO: 0x%x", error);
		}
#endif
	}

	if (ID != 0) {
		glBindVertexArray(ID);
#ifdef ANDROID
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error binding VAO %u: 0x%x", ID, error);
		}
#endif
	}
}

void VAO::Unbind()
{
	glBindVertexArray(0);
}

void VAO::Delete()
{
	glDeleteVertexArrays(1, &ID);
}
