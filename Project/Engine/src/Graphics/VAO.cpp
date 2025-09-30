#include "pch.h"

#include "Graphics/VAO.h"
#ifdef ANDROID
#include <android/log.h>
#endif

VAO::VAO()
{
	//glGenVertexArrays(1, &ID);
}

VAO::~VAO()
{
	if (ID != 0) 
	{
		Delete();
	}
}

void VAO::LinkAttrib(VBO VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset)
{
	VBO.Bind();
	glVertexAttribPointer(layout, numComponents, type, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(layout);
	VBO.Unbind();
}

void VAO::Bind()
{
	if (ID == 0) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] About to generate new VAO - checking OpenGL context");

		// Check if we have an active OpenGL context before generating VAO
		EGLDisplay display = eglGetCurrentDisplay();
		EGLContext context = eglGetCurrentContext();
		if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Trying to generate VAO without active OpenGL context!");
			return;
		}
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] OpenGL context valid, calling glGenVertexArrays");
#endif
		glGenVertexArrays(1, &ID);  // Temp workaround for refactoring - pulled out Mesh that uses VAO, but need default constructor, but
		// genVertex will crash if glfw isn't init yet, so we will init only when binding
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] glGenVertexArrays completed, Generated VAO ID: %u", ID);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[VAO] Error generating VAO: 0x%x", error);
			return;
		}
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] VAO generation successful, no OpenGL errors");
#endif
	}

	if (ID != 0) {
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] About to bind VAO ID: %u", ID);
#endif
		glBindVertexArray(ID);
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] glBindVertexArray completed for VAO ID: %u", ID);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[VAO] Error binding VAO %u: 0x%x", ID, error);
		} /*else {
			__android_log_print(ANDROID_LOG_INFO, "GAM300", "[VAO] VAO %u bound successfully", ID);
		}*/
#endif
	} else {
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[VAO] Cannot bind VAO - ID is 0 (generation failed)");
//#endif
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
