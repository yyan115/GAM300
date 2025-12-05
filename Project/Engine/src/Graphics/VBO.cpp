#include "pch.h"

#include "Graphics/VBO.h"

#pragma region Reflection
//REFL_REGISTER_START(Vertex)
//	REFL_REGISTER_PROPERTY(position)
//	REFL_REGISTER_PROPERTY(normal)
//	REFL_REGISTER_PROPERTY(color)
//	REFL_REGISTER_PROPERTY(texUV)
//REFL_REGISTER_END;
#pragma endregion

// Constructor for static mesh data
VBO::VBO(std::vector<Vertex>& vertices)
{
	glGenBuffers(1, &ID);
	if (ID == 0) {
		// glGenBuffers failed - no OpenGL context?
		return;
	}
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	// glBufferData is a function specifically targeted to copy user-defined data into the currently bound buffer.
	// Its first argument is the type of the buffer we want to copy data into: the vertex buffer object currently bound to the GL_ARRAY_BUFFER target.
	// The second argument specifies the size of the data (in bytes) we want to pass to the buffer; a simple sizeof of the vertex data suffices.
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
	initialized = true;
}

void VBO::Bind()
{
	if (ID == 0) return;  // Don't bind invalid buffer
	glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Delete()
{
	if (ID == 0) return;  // Nothing to delete
	glDeleteBuffers(1, &ID);
	ID = 0;
	initialized = false;
}

// Constructor for dynamic data
VBO::VBO(size_t size, GLenum usage)
{
	glGenBuffers(1, &ID);
	if (ID == 0) {
		// glGenBuffers failed - no OpenGL context?
		return;
	}
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferData(GL_ARRAY_BUFFER, size, nullptr, usage);
	initialized = true;
}

void VBO::UpdateData(const void* data, size_t size, size_t offset)
{
	if (ID == 0) {
		// Buffer not initialized - try to create it now
		glGenBuffers(1, &ID);
		if (ID == 0) {
			// Still failed - no OpenGL context?
			return;
		}
		glBindBuffer(GL_ARRAY_BUFFER, ID);
		glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		initialized = true;
	}
	Bind();
	glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
}

void VBO::InitializeBuffer(size_t size, GLenum usage)
{
	if (!initialized || ID == 0)
	{
		glGenBuffers(1, &ID);
		if (ID == 0) {
			// glGenBuffers failed - no OpenGL context?
			return;
		}
		initialized = true;
	}
	Bind();
	glBufferData(GL_ARRAY_BUFFER, size, nullptr, usage);
}
