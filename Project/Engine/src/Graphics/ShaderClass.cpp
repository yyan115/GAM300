#include "pch.h"

#include <filesystem>
#include "Graphics/ShaderClass.h"

#ifdef ANDROID
#include <android/asset_manager.h>
#include <android/log.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif
#include <Utilities/FileUtilities.hpp>
#include <Asset Manager/AssetManager.hpp>
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

//std::string get_file_contents(const char* filename)
//{
//#ifdef ANDROID
//	// On Android, load from assets
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Loading asset: %s", filename);
//
//	auto* platform = WindowManager::GetPlatform();
//	if (!platform) {
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Platform not available for asset loading");
//		throw std::runtime_error("Platform not available");
//	}
//
//	AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
//	AAssetManager* assetManager = androidPlatform->GetAssetManager();
//
//	if (assetManager) {
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "AssetManager is valid, attempting to open: %s", filename);
//		// Try to load from Android assets
//		AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);
//		if (asset) {
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "Asset opened successfully: %s", filename);
//			size_t length = AAsset_getLength(asset);
//			const char* buffer = (const char*)AAsset_getBuffer(asset);
//			if (buffer) {
//				std::string contents(buffer, length);
//				AAsset_close(asset);
//				__android_log_print(ANDROID_LOG_INFO, "GAM300", "Successfully loaded asset: %s (%zu bytes)", filename, length);
//				return contents;
//			}
//			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to get buffer for asset: %s", filename);
//			AAsset_close(asset);
//		} else {
//			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to open asset: %s", filename);
//		}
//		__android_log_print(ANDROID_LOG_WARN, "GAM300", "Failed to load asset: %s, trying regular file", filename);
//	} else {
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "AssetManager is null!");
//	}
//#endif
//
//	std::ifstream in(filename, std::ios::binary);
//	if (in)
//	{
//		std::string contents;
//		in.seekg(0, std::ios::end);
//		contents.resize(in.tellg());
//		in.seekg(0, std::ios::beg);
//		in.read(&contents[0], contents.size());
//		in.close();
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "Successfully loaded file: %s", filename);
//#endif
//		return(contents);
//	}
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to load file: %s", filename);
//#endif
//	std::string error_msg = "Failed to open file: " + std::string(filename);
//	if (errno != 0) {
//#ifdef _WIN32
//		char error_buffer[256];
//		strerror_s(error_buffer, sizeof(error_buffer), errno);
//		error_msg += " (Error: " + std::string(error_buffer) + ")";
//#else
//		error_msg += " (Error: " + std::string(strerror(errno)) + ")";
//#endif
//	}
//	throw std::runtime_error(error_msg);
//}

std::string get_file_contents(const char* filename)
{
	auto* platform = WindowManager::GetPlatform();
	if (!platform) {
		throw std::runtime_error("Platform not available");
	}

	ENGINE_LOG_INFO("get_file_contents filename: " + std::string{ filename });
	std::vector<uint8_t> buffer = platform->ReadAsset(filename);
	if (!buffer.empty()) {
		std::string contents(buffer.size(), '\0');
		std::memcpy(&contents[0], buffer.data(), buffer.size());
		return contents;
	}

	return "";
}

bool Shader::SetupShader(const std::string& path) {
#ifdef __ANDROID__
	// Ensure OpenGL context is current for Android
	auto platform = WindowManager::GetPlatform();
	if (platform) {
		if (!platform->MakeContextCurrent()) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to make OpenGL context current for shader compilation");
			return false;
		}
	}
#endif

	ENGINE_LOG_INFO("SetupShader path: " + path);

	std::filesystem::path p(path);
	std::string genericPath = (p.parent_path() / p.stem()).generic_string();
	std::string vertexFile = genericPath + ".vert";
	std::string fragmentFile = genericPath + ".frag";

#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Loading shader files: %s and %s", vertexFile.c_str(), fragmentFile.c_str());
#endif

	// Read vertexFile and fragmentFile and store the strings
	std::string vertexCode = get_file_contents(vertexFile.c_str());
	std::string fragmentCode = get_file_contents(fragmentFile.c_str());

	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	// Create Vertex Shader Object and get its reference
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	// Attach Vertex Shader source to the Vertex Shader Object
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Compiling vertex shader with %d chars", (int)strlen(vertexSource));
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Vertex shader source (first 200 chars): %.200s", vertexSource);
#endif
	// Compile the Vertex Shader into machine code
	glCompileShader(vertexShader);

	// Force flush any pending OpenGL commands
	glFlush();

	// check for shader compile errors
	GLint success = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Vertex shader compile status: %d (GL_TRUE=%d)", success, GL_TRUE);
#endif

	if (success != GL_TRUE)
	{
		GLint logLength = 0;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Vertex shader compilation failed! Log length: %d", logLength);
#endif

		if (logLength > 1) {
			std::vector<char> errorLog(logLength + 1);
			GLsizei actualLength = 0;
			glGetShaderInfoLog(vertexShader, logLength, &actualLength, &errorLog[0]);
			errorLog[actualLength] = '\0';

			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << &errorLog[0] << std::endl;
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Vertex shader compilation failed (length %d, actual %d): %s", logLength, actualLength, &errorLog[0]);
#endif
		} else {
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED - No error log available" << std::endl;
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Vertex shader compilation failed - No error log available");
			// Check OpenGL ES context version and capabilities
			const char* version = (const char*)glGetString(GL_VERSION);
			const char* glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
			GLenum error = glGetError();
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL version: %s", version ? version : "NULL");
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "GLSL version: %s", glslVersion ? glslVersion : "NULL");
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error state: %d", error);
#endif
		}
		return false;
	}

	// Create Fragment Shader Object and get its reference
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	// Attach Fragment Shader source to the Fragment Shader Object
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Compiling fragment shader with %d chars", (int)strlen(fragmentSource));
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Fragment shader source (first 300 chars): %.300s", fragmentSource);
#endif
	// Compile the Fragment Shader into machine code
	glCompileShader(fragmentShader);

	// Force flush any pending OpenGL commands
	glFlush();

	// check for shader compile errors
	GLint fragmentSuccess = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentSuccess);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Fragment shader compile status: %d (GL_TRUE=%d)", fragmentSuccess, GL_TRUE);
#endif

	if (fragmentSuccess != GL_TRUE)
	{
		GLint logLength = 0;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLength);
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Fragment shader compilation failed! Log length: %d", logLength);
#endif

		if (logLength > 1) {
			std::vector<char> errorLog(logLength + 1);
			GLsizei actualLength = 0;
			glGetShaderInfoLog(fragmentShader, logLength, &actualLength, &errorLog[0]);
			errorLog[actualLength] = '\0';

			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << &errorLog[0] << std::endl;
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Fragment shader compilation failed (length %d, actual %d): %s", logLength, actualLength, &errorLog[0]);
#endif
		} else {
			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED - No error log available" << std::endl;
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Fragment shader compilation failed - No error log available");
			GLenum error = glGetError();
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "OpenGL error state: %d", error);
#endif
		}
		return false;
	}

	// Create Shader Program Object and get its reference
	ID = glCreateProgram();

	// Enable the retrievable binary flag (for compiling of shader).
	glProgramParameteri(ID, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	// Attach the Vertex and Fragment Shaders to the Shader Program
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	// Wrap-up/Link all the shaders together into the Shader Program
	glLinkProgram(ID);
	// check for linking errors
	GLint linkSuccess = GL_FALSE;
	glGetProgramiv(ID, GL_LINK_STATUS, &linkSuccess);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Link status: %d (GL_TRUE=%d)", linkSuccess, GL_TRUE);
#endif
	if (linkSuccess != GL_TRUE) {
		GLint logLength = 0;
		glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 1) {
			std::vector<char> infoLog(logLength + 1);
			GLsizei actualLength = 0;
			glGetProgramInfoLog(ID, logLength, &actualLength, &infoLog[0]);
			infoLog[actualLength] = '\0';

			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << &infoLog[0] << std::endl;
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Shader program linking failed (length %d, actual %d): %s", logLength, actualLength, &infoLog[0]);
#endif
		} else {
#ifdef __ANDROID__
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Shader program linking failed - No error log available");
#endif
		}
		return false;
	}

	// Delete the now useless Vertex and Fragment Shader objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return true;
}

std::string Shader::CompileToResource(const std::string& path, bool forAndroid) {
	//// Check if glGetProgramBinary is supported first.
	//GLint supported = 0;
	//glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &supported);
	//if (supported == 0) {
	//	std::cerr << "[SHADER]: Program binary not supported. Skipping binary cache.\n";
	//	binarySupported = false;
	//	return std::string{};
	//}

	binarySupported = true;
	std::filesystem::path p(path);
	if (!SetupShader(path)) {
		std::cerr << "[SHADER]: Shader compilation failed. Aborting resource compilation.\n";
		return std::string{};
	}

	// Retrieve the binary code of the compiled shader.
	glGetProgramiv(ID, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

	binaryData.resize(binaryLength);
	glGetProgramBinary(ID, binaryLength, nullptr, &binaryFormat, binaryData.data());

	// Save the binary code to a file.
	std::string shaderPath{}; 
	
	if (!forAndroid)
		shaderPath = (p.parent_path() / p.stem()).generic_string() + ".shader";
	else
		shaderPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / p.parent_path() / p.stem()).generic_string() + ".shader";

	// Ensure parent directories exist
	p = shaderPath;
	std::filesystem::create_directories(p.parent_path());
	std::ofstream shaderFile(shaderPath, std::ios::binary);
	if (shaderFile.is_open()) {
		// Write the binary format to the file.
		shaderFile.write(reinterpret_cast<const char*>(&binaryFormat), sizeof(binaryFormat));
		// Write the binary length to the file.
		shaderFile.write(reinterpret_cast<const char*>(&binaryLength), sizeof(binaryLength));
		// Write the binary code to the file.
		shaderFile.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
		shaderFile.close();

		if (forAndroid) {
			// Copy the .vert and .frag source files as well for fallback.
			try {
				std::string vertPath = path + ".vert";
				std::string androidVertPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / vertPath).generic_string();
				std::string fragPath = path + ".frag";
				std::string androidFragPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / fragPath).generic_string();
				std::filesystem::copy_file(vertPath, androidVertPath,
					std::filesystem::copy_options::overwrite_existing);
				std::filesystem::copy_file(fragPath, androidFragPath,
					std::filesystem::copy_options::overwrite_existing);
			}
			catch (const std::filesystem::filesystem_error& e) {
				std::cerr << "[Asset] Copy failed: " << e.what() << std::endl;
			}

			return shaderPath;
		}
	}

	if (!forAndroid) {
		// Save the binary code to the root project folder as well.
		p = (FileUtilities::GetSolutionRootDir() / shaderPath);
		shaderFile.open(p.generic_string(), std::ios::binary);
		if (shaderFile.is_open()) {
			// Write the binary format to the file.
			shaderFile.write(reinterpret_cast<const char*>(&binaryFormat), sizeof(binaryFormat));
			// Write the binary length to the file.
			shaderFile.write(reinterpret_cast<const char*>(&binaryLength), sizeof(binaryLength));
			// Write the binary code to the file.
			shaderFile.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
			shaderFile.close();
			return shaderPath;
		}
	}

	return std::string{};
}

bool Shader::LoadResource(const std::string& resourcePath, const std::string& assetPath)
{
	assetPath;
	if (!binarySupported) {
		// Fallback to regular shader compilation if binary is not supported.
		if (!SetupShader(resourcePath)) {
			std::cerr << "[SHADER]: Shader compilation failed. Aborting load." << std::endl;
			return false;
		}

		return true;
	}

	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		std::cerr << "[SHADER] ERROR: Platform not available for asset discovery!" << std::endl;
		return false;
	}

	std::vector<uint8_t> fileData = platform->ReadAsset(resourcePath);
	if (!fileData.empty()) {
		size_t offset = 0;

		// Read binaryFormat (GLuint)
		if (offset + sizeof(binaryFormat) > fileData.size()) return false;
		std::memcpy(&binaryFormat, fileData.data() + offset, sizeof(binaryFormat));
		offset += sizeof(binaryFormat);

		// Read binaryLength (GLsizei)
		if (offset + sizeof(binaryLength) > fileData.size()) return false;
		std::memcpy(&binaryLength, fileData.data() + offset, sizeof(binaryLength));
		offset += sizeof(binaryLength);

		// Read binaryData
		if (offset + binaryLength > fileData.size()) return false;
		binaryData.resize(binaryLength);
		std::memcpy(binaryData.data(), fileData.data() + offset, binaryLength);
		offset += binaryLength;

		// Create a new shader program.
		ID = glCreateProgram();
		glProgramBinary(ID, binaryFormat, binaryData.data(), binaryLength);

		// Check if the program was successfully loaded.
		GLint status = 0;
		glGetProgramiv(ID, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
#ifndef ANDROID
			ENGINE_LOG_INFO("[SHADER]: Failed to load shader program from binary. Recompiling shader...");
			if (CompileToResource(assetPath).empty()) {
				ENGINE_LOG_INFO("[SHADER]: Recompilation failed. Aborting load.");
				return false;
			}
#else
			if (!SetupShader(assetPath)) {
				ENGINE_LOG_INFO("[SHADER]: Android shader setup failed. Aborting load.");
				return false;
			}
			else return true;
#endif
			return LoadResource(resourcePath);
		}

		return true;
	}
	//std::ifstream shaderFile(resourcePath, std::ios::binary);
	//if (shaderFile.is_open()) {
	//	// Read the binary format from the file.
	//	shaderFile.read(reinterpret_cast<char*>(&binaryFormat), sizeof(binaryFormat));
	//	// Read the binary length from the file.
	//	shaderFile.read(reinterpret_cast<char*>(&binaryLength), sizeof(binaryLength));
	//	binaryData.resize(binaryLength);
	//	// Read the binary code from the file.
	//	shaderFile.read(reinterpret_cast<char*>(binaryData.data()), binaryLength);

	//	// Create a new shader program.
	//	ID = glCreateProgram();
	//	glProgramBinary(ID, binaryFormat, binaryData.data(), binaryLength);

	//	// Check if the program was successfully loaded.
	//	GLint status = 0;
	//	glGetProgramiv(ID, GL_LINK_STATUS, &status);
	//	if (status == GL_FALSE) {
	//		std::cerr << "[SHADER]: Failed to load shader program from binary. Recompiling shader..." << std::endl;
	//		if (CompileToResource(resourcePath).empty()) {
	//			std::cerr << "[SHADER]: Recompilation failed. Aborting load." << std::endl;
	//			return false;
	//		}

	//		return LoadResource(resourcePath);
	//	}

	//	return true;
	//}
	else {
		std::cerr << "[SHADER]: Shader file not found: " << resourcePath << ", attempting to compile from source" << std::endl;
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_WARN, "GAM300", "[SHADER]: Shader file not found: %s, attempting to compile from source", resourcePath.c_str());
#endif
		// Fallback to regular shader compilation if binary file is not found
		if (!SetupShader(resourcePath)) {
			std::cerr << "[SHADER]: Shader compilation from source failed. Aborting load." << std::endl;
#ifdef ANDROID
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[SHADER]: Shader compilation from source failed. Aborting load.");
#endif
			return false;
		}
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[SHADER]: Successfully compiled shader from source: %s", assetPath.c_str());
#endif
		std::cout << "[SHADER]: Successfully compiled shader from source: " << resourcePath << std::endl;
		return true;
	}
}

bool Shader::ReloadResource(const std::string& resourcePath, const std::string& assetPath)
{
	return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Shader::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid)
{
	assetPath, currentMetaData, forAndroid;
	return std::shared_ptr<AssetMeta>();
}

void Shader::Activate()
{
	glUseProgram(ID);
}

void Shader::Delete()
{
	glDeleteProgram(ID);
}

void Shader::setBool(const std::string& name, GLboolean value)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform1i(location, (int)value);
	}
}

void Shader::setInt(const std::string& name, int value)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform1i(location, value);
	}
}

void Shader::setIntArray(const std::string& name, const GLint* values, GLint count)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform1iv(location, count, values);
	}
}

void Shader::setFloat(const std::string& name, GLfloat value)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform1f(location, value);
	}
}

void Shader::setVec2(const std::string& name, const glm::vec2& value)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform2fv(location, 1, &value[0]);
	}
}

void Shader::setVec2(const std::string& name, float x, float y)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform2f(location, x, y);
	}
}

void Shader::setVec3(const std::string& name, const glm::vec3& value)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform3fv(location, 1, &value[0]);
	}
}

void Shader::setVec3(const std::string& name, float x, float y, float z)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform3f(location, x, y, z);
	}
}

void Shader::setVec4(const std::string& name, const glm::vec4& value)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform4fv(location, 1, &value[0]);
	}
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniform4f(location, x, y, z, w);
	}
}

void Shader::setMat2(const std::string& name, const glm::mat2& mat)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniformMatrix2fv(location, 1, GL_FALSE, &mat[0][0]);
	}
}

void Shader::setMat3(const std::string& name, const glm::mat3& mat)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniformMatrix3fv(location, 1, GL_FALSE, &mat[0][0]);
	}
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat)
{
	GLint location = getUniformLocation(name);
	if (location != -1) {
		glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
	}
}

GLint Shader::getUniformLocation(const std::string& name)
{
	auto it = m_uniformCache.find(name);
	if (it != m_uniformCache.end())
	{
		return it->second;
	}

	GLint location = glGetUniformLocation(ID, name.c_str());
	m_uniformCache[name] = location;

	// Debug output for missing uniforms (can be removed later)
	if (location == -1)
	{
		std::cout << "Warning: Uniform '" << name << "' not found in shader ID: " << ID << std::endl;
	}

	return location;
}
