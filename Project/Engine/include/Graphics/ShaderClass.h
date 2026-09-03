#pragma once

#include "pch.h"

#include "OpenGL.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include <array>
#include <cstdint>
#include <string_view>
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

std::string get_file_contents(const char* filename);

class ENGINE_API Shader : public IAsset {
public:
    GLuint ID{};

    Shader() = default;
    Shader(std::shared_ptr<AssetMeta> shaderMeta);

    std::string CompileToResource(const std::string& path, bool forAndroid = false) override;
    bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
    bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
    std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;

	void Activate();
    static void ResetActiveProgramCache() noexcept { s_activeProgram = 0; }
	void Delete();

    void setBool(std::string_view name, GLboolean value);
    void setInt(std::string_view name, int value);
    void setIntArray(std::string_view name, const GLint* values, GLint count);
    void setFloat(std::string_view name, GLfloat value);
    void setVec2(std::string_view name, const glm::vec2& value);
    void setVec2(std::string_view name, float x, float y);
    void setVec3(std::string_view name, const glm::vec3& value);
    void setVec3(std::string_view name, float x, float y, float z);
    void setVec4(std::string_view name, const glm::vec4& value);
    void setVec4(std::string_view name, float x, float y, float z, float w);
    void setMat2(std::string_view name, const glm::mat2& mat);
    void setMat3(std::string_view name, const glm::mat3& mat);
    void setMat4(std::string_view name, const glm::mat4& mat);

    // Upload an array of matrices in a single GL call — use this for bone matrices
    // instead of calling setMat4 in a loop (avoids 100 hash lookups + 100 GL calls).
    void setMat4Array(std::string_view firstName, const glm::mat4* matrices, GLsizei count);

	    bool UsesCameraBlock() const noexcept { return m_usesCameraBlock; }
	    bool UsesLightingBlock() const noexcept { return m_usesLightingBlock; }
	    bool UsesBonesBlock() const noexcept { return m_usesBonesBlock; }
	    bool UsesMaterialBlock() const noexcept { return m_usesMaterialBlock; }
    bool HasStaticShadowUniforms() const noexcept { return m_hasStaticShadowUniforms; }
    void MarkStaticShadowUniformsSet() noexcept { m_hasStaticShadowUniforms = true; }

    //void clearUniformCache();

private:
    struct TransparentStringHash {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    std::unordered_map<std::string, GLint, TransparentStringHash, std::equal_to<>> m_uniformCache;
    GLint getUniformLocation(std::string_view name);

    enum class UniformValueType : std::uint8_t {
        Int,
        IntArray,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat3,
        Mat4
    };

    struct CachedUniformValue {
        std::array<std::uint8_t, sizeof(glm::mat4)> bytes{};
        std::uint8_t size = 0;
        UniformValueType type = UniformValueType::Int;
        bool valid = false;
    };

    bool ShouldUploadUniform(
        GLint location,
        UniformValueType type,
        const void* data,
        std::size_t size);

    // Uniform values are program state and survive glUseProgram switches.
    // Direct indexing by location avoids a second string/hash lookup per setter.
    std::vector<CachedUniformValue> m_uniformValueCache;
    inline static thread_local GLuint s_activeProgram = 0;

    // Store shader binary data (precompiled shader).
    GLint binaryLength{};
	std::vector<uint8_t> binaryData;
    GLenum binaryFormat{};
    bool binarySupported = true;

	    bool m_usesCameraBlock = false;
	    bool m_usesLightingBlock = false;
	    bool m_usesBonesBlock = false;
	    bool m_usesMaterialBlock = false;
    bool m_hasStaticShadowUniforms = false;

    bool SetupShader(const std::string& path);
    void BindKnownUniformBlocks();
};
