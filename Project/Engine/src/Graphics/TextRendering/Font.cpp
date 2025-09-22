#include "pch.h"
#include "Graphics/TextRendering/Font.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#ifdef ANDROID
#include "Platform/AndroidPlatform.h"
#include <android/asset_manager.h>
#include <cstring>
#endif

Font::Font(unsigned int fontSize) : fontSize(fontSize), vaoSetupNeeded(false) {}

Font::~Font()
{
	Cleanup();
}

std::string Font::CompileToResource(const std::string& assetPath)
{
    return assetPath + ".font";
}

bool Font::LoadResource(const std::string& assetPath, unsigned int newFontSize)
{
    fontSize = newFontSize;
    fontAssetPath = assetPath;

    // Clean up existing data
    Cleanup();

    std::vector<unsigned char> fontData;

#ifdef ANDROID
    // Use Android AssetManager to load font data directly from APK
    auto* platform = WindowManager::GetPlatform();
    if (!platform) {
        std::cerr << "[Font] Platform not available" << std::endl;
        return false;
    }

    // Cast to AndroidPlatform to access AssetManager
    AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
    AAssetManager* assetManager = androidPlatform->GetAssetManager();

    if (!assetManager) {
        std::cerr << "[Font] AssetManager is null!" << std::endl;
        return false;
    }

    // Try to load from Android assets
    AAsset* asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        std::cerr << "[Font] Failed to open font asset: " << assetPath << std::endl;
        return false;
    }

    size_t length = AAsset_getLength(asset);
    const void* buffer = AAsset_getBuffer(asset);
    if (!buffer) {
        std::cerr << "[Font] Failed to get buffer for font asset: " << assetPath << std::endl;
        AAsset_close(asset);
        return false;
    }

    // Copy the data to our vector
    fontData.resize(length);
    std::memcpy(fontData.data(), buffer, length);
    AAsset_close(asset);

    std::cout << "[Font] Successfully loaded font from Android assets: " << assetPath << " (size: " << fontData.size() << " bytes)" << std::endl;
#else
    // Desktop: Try to load .font file first, fallback to .ttf
    std::filesystem::path assetPathFS(assetPath);
    std::string fontPath = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".font";

    // Try .font file first
    std::ifstream fontFile(fontPath, std::ios::binary | std::ios::ate);
    if (!fontFile.is_open()) {
        // Fallback to original .ttf file
        fontFile.open(assetPath, std::ios::binary | std::ios::ate);
        if (!fontFile.is_open()) {
            std::cerr << "[Font] Failed to open font asset: " << assetPath << std::endl;
            return false;
        }
        fontPath = assetPath;
    }

    std::streamsize size = fontFile.tellg();
    fontFile.seekg(0, std::ios::beg);

    fontData.resize(size);
    fontFile.read(reinterpret_cast<char*>(fontData.data()), size);
    fontFile.close();

    std::cout << "[Font] Successfully loaded font from desktop: " << fontPath << " (size: " << fontData.size() << " bytes)" << std::endl;
#endif

    // Initialize FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cerr << "[Font] Could not initialize FreeType Library" << std::endl;
        return false;
    }

    // Load font as face from memory
    FT_Face face;
#ifdef ANDROID
    if (FT_New_Memory_Face(ft, fontData.data(), fontData.size(), 0, &face))
#else
    if (FT_New_Memory_Face(ft, fontData.data(), fontData.size(), 0, &face))
#endif
    {
        std::cerr << "[Font] Failed to load font resource: " << assetPath << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }

    // Sets the font's width and height parameters
    // Setting the width to 0 lets the face dynamically calculate the width based on the given height
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 characters of ASCII set
    for (unsigned char c = 0; c < 128; c++)
    {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cerr << "[Font] Failed to load Glyph for character: " << c << std::endl;
            continue;
        }

        // Generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Set flag to defer VAO/VBO setup until render thread
    vaoSetupNeeded = true;

    std::cout << "[Font] Successfully loaded font resource: " << assetPath << " (size: " << fontSize << ")" << std::endl;
    return true;
}

void Font::SetFontSize(unsigned int newSize)
{
    if (newSize != fontSize && !fontAssetPath.empty())
    {
        LoadResource(fontAssetPath, newSize);
    }
}

const Character& Font::GetCharacter(char c) const
{
    auto it = Characters.find(c);
    if (it != Characters.end())
    {
        return it->second;
    }

    // Return space character as fallback
    static Character fallback = {};
    return fallback;
}

float Font::GetTextWidth(const std::string& text, float scale) const
{
    float width = 0.0f;
    for (char c : text)
    {
        const Character& ch = GetCharacter(c);
        width += (ch.advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
    }
    return width;
}

float Font::GetTextHeight(float scale) const
{
    float maxHeight = 0.0f;
    for (const auto& pair : Characters)
    {
        float height = pair.second.size.y * scale;
        if (height > maxHeight)
        {
            maxHeight = height;
        }
    }
    return maxHeight;
}

void Font::Cleanup()
{
    // Clean up textures
    for (auto& pair : Characters)
    {
        glDeleteTextures(1, &pair.second.textureID);
    }
    Characters.clear();

    // Clean up VAO/VBO
    if (textVAO)
    {
        textVAO->Delete();
        textVAO.reset();
    }
    if (textVBO)
    {
        textVBO->Delete();
        textVBO.reset();
    }
}

void Font::EnsureVAOSetup() const
{
    if (!vaoSetupNeeded || textVAO) {
        return; // Already set up or not needed
    }

#ifdef ANDROID
    // Check if we have an active OpenGL context before generating VAO/VBO
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
        std::cout << "[Font] No OpenGL context available for VAO/VBO setup, deferring..." << std::endl;
        return;
    }
#endif

    std::cout << "[Font] Setting up deferred VAO/VBO on render thread" << std::endl;

    // Set up VAO/VBO for text rendering using your extended classes
    textVAO = std::make_unique<VAO>();
    textVBO = std::make_unique<VBO>(sizeof(float) * 6 * 4, GL_DYNAMIC_DRAW);

#ifdef ANDROID
    std::cout << "[Font] Created VAO ID: " << (textVAO ? textVAO->ID : 0)
              << ", VBO ID: " << (textVBO ? textVBO->ID : 0) << std::endl;
#endif

    textVAO->Bind();
    textVBO->Bind();

    // Set up vertex attributes for text (vec4: x, y, u, v)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

#ifdef ANDROID
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "[Font] OpenGL error after vertex attribute setup: 0x" << std::hex << error << std::endl;
    }
#endif

    textVBO->Unbind();
    textVAO->Unbind();

    vaoSetupNeeded = false;
    std::cout << "[Font] VAO/VBO setup completed successfully" << std::endl;
}

std::shared_ptr<AssetMeta> Font::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData)
{
    (void)assetPath; (void)currentMetaData;
    return std::shared_ptr<AssetMeta>();
}