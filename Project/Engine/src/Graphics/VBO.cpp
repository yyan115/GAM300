#include "pch.h"

#include "Graphics/VBO.h"
#ifdef ANDROID
#include <android/log.h>
#endif

// Constructor for static mesh data
VBO::VBO(std::vector<Vertex>& vertices) : vertices(vertices)
{
	// Don't generate buffer here - defer until first bind when we have OpenGL context
}

void VBO::setupBuffer()
{
#ifdef ANDROID
	// Check if we have an active OpenGL context before generating buffer
	EGLDisplay display = eglGetCurrentDisplay();
	EGLContext context = eglGetCurrentContext();
	if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Trying to generate VBO without active OpenGL context!");
		return;
	}
#endif

	glGenBuffers(1, &ID);
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	// glBufferData is a function specifically targeted to copy user-defined data into the currently bound buffer.
	// Its first argument is the type of the buffer we want to copy data into: the vertex buffer object currently bound to the GL_ARRAY_BUFFER target.
	// The second argument specifies the size of the data (in bytes) we want to pass to the buffer; a simple sizeof of the vertex data suffices.
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Generated VBO ID: %u with %zu vertices", ID, vertices.size());
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error generating VBO: 0x%x", error);
	}
#endif
}

void VBO::Bind()
{
	if (ID == 0 && !vertices.empty()) {
		setupBuffer();
	}

	if (ID != 0) {
		glBindBuffer(GL_ARRAY_BUFFER, ID);
#ifdef ANDROID
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error binding VBO %u: 0x%x", ID, error);
		}
#endif
	}
}

void VBO::Unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Delete()
{
	if (ID != 0) {
		glDeleteBuffers(1, &ID);
		ID = 0;
	}
}

// Constructor for dynamic data
VBO::VBO(size_t size, GLenum usage)
{
	glGenBuffers(1, &ID);
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferData(GL_ARRAY_BUFFER, size, nullptr, usage);
	initialized = true;
}

void VBO::UpdateData(const void* data, size_t size, size_t offset)
{
	Bind();
	glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
}

void VBO::InitializeBuffer(size_t size, GLenum usage)
{
	if (!initialized) 
	{
		glGenBuffers(1, &ID);
		initialized = true;
	}
	Bind();
	glBufferData(GL_ARRAY_BUFFER, size, nullptr, usage);
}
