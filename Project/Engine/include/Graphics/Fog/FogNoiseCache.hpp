#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

class Shader;

// GPU-generated hash corners for the existing fog noise function. Values and
// interpolation stay unchanged; unsupported or oversized domains use the shader
// fallback. Storage is bounded independently of scene lifetime.
class FogNoiseCache {
public:
    FogNoiseCache() = default;
    FogNoiseCache(const FogNoiseCache&) = delete;
    FogNoiseCache& operator=(const FogNoiseCache&) = delete;

    void BeginFrame() { ++m_frame; }
    void Shutdown();

    // The fog shader and volume VAO must already be active. Restore the texture
    // binding with Unbind() after the volume draw when this returns true.
    bool Bind(Shader& shader, unsigned int volumeVAO, float time,
              float scrollSpeedX, float scrollSpeedY, float noiseScale, float warpStrength);
    void Unbind();

private:
    static constexpr int CACHE_WIDTH = 256; // Matches fixed shader texel addressing.
    static constexpr int MAX_CELLS = 65536;
    static constexpr std::size_t MAX_VOLUMES = 16;

    struct Descriptor {
        std::array<int, 36> origins{}; // xyz integer origin, w first texel
        std::array<int, 27> sizes{};
        int height = 0;
        bool operator==(const Descriptor&) const = default;
    };
    struct Entry {
        unsigned int texture = 0;
        unsigned int framebuffer = 0;
        int capacityHeight = 0;
        Descriptor descriptor;
        std::uint64_t lastUsed = 0;
        bool valid = false;
    };
    struct Uniforms {
        int build = -1;
        int enabled = -1;
        int sampler = -1;
        int width = -1;
        int origins = -1;
        int sizes = -1;
    };

    static bool BuildDescriptor(float time, float scrollSpeedX, float scrollSpeedY,
                                float noiseScale, float warpStrength, Descriptor& result);
    static void DeleteEntry(Entry& entry);
    Entry* GetEntry(unsigned int volumeVAO);
    bool Generate(Entry& entry, const Descriptor& descriptor);
    void SetShader(Shader& shader);

    std::unordered_map<unsigned int, Entry> m_entries;
    unsigned int m_shaderProgram = 0;
    unsigned int m_emptyVAO = 0;
    std::uint64_t m_frame = 0;
    Uniforms m_uniforms;
    bool m_supported = false;
#ifdef ANDROID
    bool m_floatTargetChecked = false;
    bool m_floatTargetSupported = false;
#endif
    bool m_bound = false;
    int m_previousTexture = 0;
    int m_previousSampler = 0;
    int m_previousActiveTexture = 0;
};
