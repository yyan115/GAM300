#include "pch.h"
#include "Graphics/Fog/FogNoiseCache.hpp"

#include "Graphics/ShaderClass.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
// Only cache generation changes these states. The normal volume draw retains
// the renderer's original blending, viewport, depth and culling behavior.
struct BuildState {
    int framebuffer = 0;
    int viewport[4]{};
    int vertexArray = 0;
    int unpackBuffer = 0;
    GLboolean colorMask[4]{};
    GLboolean blend = GL_FALSE;
    static constexpr std::array<GLenum, 5> capabilities = {
        GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_RASTERIZER_DISCARD
    };
    std::array<GLboolean, capabilities.size()> enabled{};

    BuildState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
#ifdef ANDROID
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        blend = glIsEnabled(GL_BLEND);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#else
        glGetBooleani_v(GL_COLOR_WRITEMASK, 0, colorMask);
        blend = glIsEnabledi(GL_BLEND, 0);
        glDisablei(GL_BLEND, 0);
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        for (std::size_t i = 0; i < capabilities.size(); ++i) {
            enabled[i] = glIsEnabled(capabilities[i]);
            glDisable(capabilities[i]);
        }
    }
    ~BuildState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBindVertexArray(vertexArray);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
#ifdef ANDROID
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        if (blend) glEnable(GL_BLEND);
#else
        glColorMaski(0, colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        if (blend) glEnablei(GL_BLEND, 0);
#endif
        for (std::size_t i = 0; i < capabilities.size(); ++i) {
            if (enabled[i]) glEnable(capabilities[i]);
        }
    }
};
}

bool FogNoiseCache::BuildDescriptor(float time, float scrollSpeedX, float scrollSpeedY,
                                   float noiseScale, float warpStrength, Descriptor& result)
{
    for (float value : {time, scrollSpeedX, scrollSpeedY, noiseScale, warpStrength}) {
        if (!std::isfinite(value)) return false;
    }
    const std::array<float, 3> scroll = {
        time * scrollSpeedX, time * scrollSpeedY, time * scrollSpeedX * 0.5f
    };
    const float lowerScale = std::min(0.0f, noiseScale);
    const float upperScale = std::max(0.0f, noiseScale);
    // The two warp octaves have amplitudes 0.5 and 0.25, with hash values in [0,1].
    const float displacement = std::max(0.0f, warpStrength) * 0.75f;
    constexpr float offsets[3][3] = {
        {1.7f, 9.2f, 3.4f}, {8.3f, 2.8f, 5.1f}, {4.5f, 6.1f, 1.9f}
    };
    int slot = 0;
    for (int region = 0; region < 9; ++region) {
        const int octave = region < 6 ? region % 2 : region - 6;
        const float frequency = static_cast<float>(1 << octave);
        int cells = 1;
        for (int axis = 0; axis < 3; ++axis) {
            const float offset = region < 6 ? offsets[region / 2][axis] : 0.0f;
            const double lower = (static_cast<double>(scroll[axis]) + lowerScale + offset) * frequency;
            const double upper = (static_cast<double>(scroll[axis]) + upperScale + offset +
                                  (region < 6 ? 0.0f : displacement)) * frequency;
            // Keep integer corners exactly representable as float. The original
            // shader handles larger, nonfinite or excessively broad domains.
            if (!std::isfinite(lower) || !std::isfinite(upper) ||
                std::abs(lower) > 8000000.0 || std::abs(upper) > 8000000.0) return false;
            const int origin = static_cast<int>(std::floor(lower)) - 1;
            const int size = static_cast<int>(std::floor(upper)) + 2 - origin;
            if (size <= 0 || size > 128 || cells > MAX_CELLS / size) return false;
            result.origins[region * 4 + axis] = origin;
            result.sizes[region * 3 + axis] = size;
            cells *= size;
        }
        if (slot > MAX_CELLS * 2 - cells * 2) return false;
        result.origins[region * 4 + 3] = slot;
        slot += cells * 2;
    }
    result.height = (slot + CACHE_WIDTH - 1) / CACHE_WIDTH;
    return result.height > 0;
}

void FogNoiseCache::DeleteEntry(Entry& entry)
{
    if (entry.texture) glDeleteTextures(1, &entry.texture);
    if (entry.framebuffer) glDeleteFramebuffers(1, &entry.framebuffer);
    entry = Entry{};
}

FogNoiseCache::Entry* FogNoiseCache::GetEntry(unsigned int volumeVAO)
{
    auto found = m_entries.find(volumeVAO);
    if (found != m_entries.end()) return &found->second;
    if (m_entries.size() >= MAX_VOLUMES) {
        auto oldest = m_entries.end();
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->second.lastUsed < m_frame &&
                (oldest == m_entries.end() || it->second.lastUsed < oldest->second.lastUsed)) oldest = it;
        }
        // Do not evict a volume already used by this frame. Extra volumes still
        // render with the original noise function, without allocation churn.
        if (oldest == m_entries.end()) return nullptr;
        DeleteEntry(oldest->second);
        m_entries.erase(oldest);
    }
    return &m_entries.try_emplace(volumeVAO).first->second;
}

void FogNoiseCache::SetShader(Shader& shader)
{
    if (m_shaderProgram == shader.ID) return;
    m_shaderProgram = shader.ID;
    m_uniforms.build = glGetUniformLocation(shader.ID, "noiseCacheBuild");
    m_uniforms.enabled = glGetUniformLocation(shader.ID, "noiseCacheEnabled");
    m_uniforms.sampler = glGetUniformLocation(shader.ID, "noiseCornerCache");
    m_uniforms.width = glGetUniformLocation(shader.ID, "noiseCacheWidth");
    m_uniforms.origins = glGetUniformLocation(shader.ID, "noiseCacheOrigins[0]");
    m_uniforms.sizes = glGetUniformLocation(shader.ID, "noiseCacheSizes[0]");
    m_supported = m_uniforms.build >= 0 && m_uniforms.enabled >= 0 &&
                  m_uniforms.sampler >= 0 && m_uniforms.width >= 0 &&
                  m_uniforms.origins >= 0 && m_uniforms.sizes >= 0;
    // A reloaded shader may change the hash function as well as uniform locations.
    for (auto& [key, entry] : m_entries) entry.valid = false;
}

bool FogNoiseCache::Generate(Entry& entry, const Descriptor& descriptor)
{
    BuildState restore;
    if (!entry.texture) {
        glGenTextures(1, &entry.texture);
        glGenFramebuffers(1, &entry.framebuffer);
    }
    glBindTexture(GL_TEXTURE_2D, entry.texture);
    if (entry.capacityHeight < descriptor.height) {
        int capacity = 1;
        while (capacity < descriptor.height) capacity *= 2;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, CACHE_WIDTH, capacity, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        entry.capacityHeight = capacity;
    }
    // The cache must not be sampled while it is the draw attachment.
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, entry.framebuffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entry.texture, 0);
    const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);
    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    if (!m_emptyVAO) glGenVertexArrays(1, &m_emptyVAO);
    glBindVertexArray(m_emptyVAO);
    glViewport(0, 0, CACHE_WIDTH, descriptor.height);
    glUniform1i(m_uniforms.build, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glUniform1i(m_uniforms.build, 0);
    entry.descriptor = descriptor;
    entry.valid = true;
    return true;
}

bool FogNoiseCache::Bind(Shader& shader, unsigned int volumeVAO, float time,
                        float scrollSpeedX, float scrollSpeedY, float noiseScale, float warpStrength)
{
    if (m_bound) Unbind();
    if (!shader.ID) return false;
    SetShader(shader);
    glUniform1i(m_uniforms.enabled, 0);
    glUniform1i(m_uniforms.build, 0);
    if (!m_supported) return false;
#ifdef ANDROID
    if (!m_floatTargetChecked) {
        int major = 0, minor = 0, extensions = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        m_floatTargetSupported = major > 3 || (major == 3 && minor >= 2);
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensions);
        for (int i = 0; !m_floatTargetSupported && i < extensions; ++i) {
            const char* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            m_floatTargetSupported = name && std::strcmp(name, "GL_EXT_color_buffer_float") == 0;
        }
        m_floatTargetChecked = true;
    }
    if (!m_floatTargetSupported) return false;
#endif
    Descriptor descriptor;
    if (!BuildDescriptor(time, scrollSpeedX, scrollSpeedY, noiseScale, warpStrength, descriptor)) return false;
    Entry* entry = GetEntry(volumeVAO);
    if (!entry) return false;
    entry->lastUsed = m_frame;

    glGetIntegerv(GL_ACTIVE_TEXTURE, &m_previousActiveTexture);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_previousTexture);
#ifdef ANDROID
    glGetIntegerv(GL_SAMPLER_BINDING, &m_previousSampler);
#else
    glGetIntegeri_v(GL_SAMPLER_BINDING, 3, &m_previousSampler);
#endif
    glBindSampler(3, 0);
    m_bound = true;
    glUniform1i(m_uniforms.sampler, 3);
    glUniform1i(m_uniforms.width, CACHE_WIDTH);
    glUniform4iv(m_uniforms.origins, 9, descriptor.origins.data());
    glUniform3iv(m_uniforms.sizes, 9, descriptor.sizes.data());
    if ((!entry->valid || !(entry->descriptor == descriptor)) && !Generate(*entry, descriptor)) {
        Unbind();
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, entry->texture);
    glUniform1i(m_uniforms.enabled, 1);
    glActiveTexture(m_previousActiveTexture);
    return true;
}

void FogNoiseCache::Unbind()
{
    if (!m_bound) return;
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_previousTexture);
    glBindSampler(3, m_previousSampler);
    glActiveTexture(m_previousActiveTexture);
    m_bound = false;
}

void FogNoiseCache::Shutdown()
{
    Unbind();
    for (auto& [key, entry] : m_entries) DeleteEntry(entry);
    m_entries.clear();
    if (m_emptyVAO) glDeleteVertexArrays(1, &m_emptyVAO);
    m_emptyVAO = 0;
    m_shaderProgram = 0;
    m_uniforms = Uniforms{};
    m_supported = false;
    m_frame = 0;
#ifdef ANDROID
    m_floatTargetChecked = false;
    m_floatTargetSupported = false;
#endif
}
