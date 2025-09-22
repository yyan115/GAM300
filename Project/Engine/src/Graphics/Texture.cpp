#include "pch.h"

#include "Graphics/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Graphics/stb_image.h"
<<<<<<< Updated upstream
=======
#ifdef ANDROID
#include <GLI/gli.hpp>
#else
#include <gli/gli.hpp>
#endif
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
>>>>>>> Stashed changes

Texture::Texture() : ID(0), type(type), unit(0) {}

Texture::Texture(const char* image, const char* texType, GLuint slot, GLenum pixelType)
{
	// Assigns the type of the texture ot the texture object
	type = texType;
	unit = slot; // Store the slot, but don't bind if it's -1

	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;
	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load(true);
	// Reads the image from a file and stores it in bytes
	unsigned char* bytes = stbi_load(image, &widthImg, &heightImg, &numColCh, 0);

	// Get the image format and set it accordingly.
	GLenum format;
	switch (numColCh) {
		case 1:
			format = GL_RED;
			break;
		case 2:
			format = GL_RG;
			break;
		case 3:
			format = GL_RGB; 
			break;
		case 4:
			format = GL_RGBA;
			break;
		default:
			format = GL_RGB;
			break;
	}

	// Generates an OpenGL texture object
	glGenTextures(1, &ID);

	//// Assigns the texture to a Texture Unit
	//glActiveTexture(GL_TEXTURE0 + slot);
	//unit = slot;
	//glBindTexture(GL_TEXTURE_2D, ID);

	// Only bind to unit if slot is valid
	if (slot >= 0) 
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, ID);
	}

	// Configures the type of algorithm that is used to make the image smaller or bigger
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Configures the way the texture repeats (if it does at all)
<<<<<<< Updated upstream
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
=======
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
#ifndef ANDROID
	glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, &format.Swizzles[0]);
#endif
>>>>>>> Stashed changes

	// Extra lines in case you choose to use GL_CLAMP_TO_BORDER
	// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	// glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);

	// Assigns the image to the OpenGL Texture object
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, widthImg, heightImg, 0, format, pixelType, bytes);
	// Generates MipMaps
	glGenerateMipmap(GL_TEXTURE_2D);

	// Deletes the image data as it is already in the OpenGL Texture object
	stbi_image_free(bytes);

	// Unbinds the OpenGL Texture object so that it can't accidentally be modified
	glBindTexture(GL_TEXTURE_2D, 0);
}

bool Texture::LoadAsset(const std::string& path) {
	path;

	return true;
}

GLenum Texture::GetFormatFromExtension(const std::string& filepath) {
	std::string extension = filepath.substr(filepath.find_last_of('.'));

	if (extension == ".png" || extension == ".PNG")
	{
		return GL_RGBA;
	}
	else if (extension == ".jpg" || extension == ".jpeg" || extension == ".JPG" || extension == ".JPEG")
	{
		return GL_RGB;
	}
	else if (extension == ".bmp" || extension == ".BMP")
	{
		return GL_RGB;
	}
	else
	{
		// Default to RGB for unknown formats
		return GL_RGB;
	}
}

void Texture::texUnit(Shader& shader, const char* uniform, GLuint unit)
{
	// Shader needs to be activated before changing the value of a uniform
	shader.Activate();
	// Sets the value of the uniform
	shader.setInt(uniform, unit);
}

void Texture::Bind()
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, ID);
}

void Texture::Unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Delete()
{
	glDeleteTextures(1, &ID);
}