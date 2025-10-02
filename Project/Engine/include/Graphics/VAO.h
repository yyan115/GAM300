#pragma once
#include "VBO.h"

// VAO stores the state/configuration of how vertex attributes are read from the VBO(s). To remember how vertex data is laid out, so you dont have to re-specify attribute pointers every time.
// Unless the data layout is different, we only need one VAO. VAO is used to tell the GPU how to read the memory stored.
// E.g. The first 3 is position, next 3 color etc. so VAO remembers this layout so the GPU knows how to read it each time.
class VAO {
public:
	GLuint ID{};

	VAO();
	~VAO();

	void LinkAttrib(VBO VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset);
	void LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset, GLuint divisor);
	void Bind();
	void Unbind();
	void Delete();

	VAO(VAO&& other) noexcept : ID(other.ID) {
		other.ID = 0; // Prevent the moved-from object from deleting the VAO
	}
};