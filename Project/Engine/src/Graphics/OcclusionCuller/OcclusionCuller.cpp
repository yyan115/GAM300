#include "pch.h"

#ifndef ANDROID

#include "Graphics/OcclusionCuller/OcclusionCuller.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// Inline GLSL source
// ─────────────────────────────────────────────────────────────────────────────
static const char* kVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProjection;
uniform vec3 bboxMin;
uniform vec3 bboxMax;
void main()
{
    // Map the unit-cube vertex [-0.5, 0.5] to the world-space AABB.
    vec3 worldPos = bboxMin + (aPos + vec3(0.5)) * (bboxMax - bboxMin);
    gl_Position = viewProjection * vec4(worldPos, 1.0);
}
)";

static const char* kFragSrc = R"(
#version 330 core
void main() {}
)";

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "[OcclusionCuller] Shader compile error: " << log << '\n';
    }
    return shader;
}

// ─────────────────────────────────────────────────────────────────────────────
// OcclusionCuller
// ─────────────────────────────────────────────────────────────────────────────
void OcclusionCuller::Initialize()
{
    if (m_initialized) return;

    m_program = CompileProgram();
    if (m_program)
    {
        m_vpLoc      = glGetUniformLocation(m_program, "viewProjection");
        m_bboxMinLoc = glGetUniformLocation(m_program, "bboxMin");
        m_bboxMaxLoc = glGetUniformLocation(m_program, "bboxMax");
    }

    InitProxyGeometry();
    m_initialized = true;

    std::cout << "[OcclusionCuller] Initialized\n";
}

void OcclusionCuller::Shutdown()
{
    // Delete all GL query objects.
    for (auto& [id, data] : m_entityData)
    {
        if (data.queryId != 0)
            glDeleteQueries(1, &data.queryId);
    }
    m_entityData.clear();

    if (m_proxyVAO) { glDeleteVertexArrays(1, &m_proxyVAO); m_proxyVAO = 0; }
    if (m_proxyVBO) { glDeleteBuffers(1, &m_proxyVBO);      m_proxyVBO = 0; }
    if (m_proxyEBO) { glDeleteBuffers(1, &m_proxyEBO);      m_proxyEBO = 0; }
    if (m_program)  { glDeleteProgram(m_program);            m_program  = 0; }

    m_initialized = false;
    std::cout << "[OcclusionCuller] Shutdown\n";
}

void OcclusionCuller::BeginFrame()
{
    if (!m_enabled || !m_initialized) return;

    m_occludedCount = 0;

    for (auto& [entityId, data] : m_entityData)
    {
        if (data.pendingResult && data.queryId != 0)
        {
            // Non-blocking check — only read if the GPU has the result ready.
            GLint available = 0;
            glGetQueryObjectiv(data.queryId, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available)
            {
                GLuint result = 0;
                glGetQueryObjectuiv(data.queryId, GL_QUERY_RESULT, &result);
                data.lastVisible   = (result > 0);
                data.pendingResult = false;
            }
            // If not yet available, keep the previous result (no stall).
        }

        if (!data.lastVisible)
            ++m_occludedCount;
    }
}

bool OcclusionCuller::IsOccluded(uint32_t entityId) const
{
    auto it = m_entityData.find(entityId);
    if (it == m_entityData.end())
        return false; // Unknown entity — assume visible.
    return !it->second.lastVisible;
}

void OcclusionCuller::SubmitQuery(uint32_t entityId,
                                  const AABB& worldBBox,
                                  const glm::mat4& viewProjection)
{
    if (!m_initialized || m_program == 0) return;

    auto& data = m_entityData[entityId];

    // Create the GL query object on first use.
    if (data.queryId == 0)
        glGenQueries(1, &data.queryId);

    // If a result is still pending (GPU not ready), skip this frame's query
    // rather than stalling or issuing a double-query for the same object.
    if (data.pendingResult)
        return;

    glBeginQuery(GL_ANY_SAMPLES_PASSED, data.queryId);
    DrawAABBProxy(worldBBox, viewProjection);
    glEndQuery(GL_ANY_SAMPLES_PASSED);

    data.pendingResult = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private
// ─────────────────────────────────────────────────────────────────────────────
void OcclusionCuller::InitProxyGeometry()
{
    // Unit cube: 8 corners from -0.5 to +0.5.
    static const float vertices[] = {
        -0.5f, -0.5f, -0.5f,  // 0
         0.5f, -0.5f, -0.5f,  // 1
         0.5f,  0.5f, -0.5f,  // 2
        -0.5f,  0.5f, -0.5f,  // 3
        -0.5f, -0.5f,  0.5f,  // 4
         0.5f, -0.5f,  0.5f,  // 5
         0.5f,  0.5f,  0.5f,  // 6
        -0.5f,  0.5f,  0.5f,  // 7
    };

    // 12 triangles (6 faces × 2 triangles), 36 indices.
    static const unsigned int indices[] = {
        0,1,2,  2,3,0,  // -Z face
        5,4,7,  7,6,5,  // +Z face
        4,0,3,  3,7,4,  // -X face
        1,5,6,  6,2,1,  // +X face
        0,4,5,  5,1,0,  // -Y face
        3,2,6,  6,7,3,  // +Y face
    };

    glGenVertexArrays(1, &m_proxyVAO);
    glGenBuffers(1, &m_proxyVBO);
    glGenBuffers(1, &m_proxyEBO);

    glBindVertexArray(m_proxyVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_proxyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_proxyEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

GLuint OcclusionCuller::CompileProgram()
{
    GLuint vert = CompileShader(GL_VERTEX_SHADER,   kVertSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, kFragSrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "[OcclusionCuller] Program link error: " << log << '\n';
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

void OcclusionCuller::DrawAABBProxy(const AABB& bbox,
                                    const glm::mat4& viewProjection)
{
    glUseProgram(m_program);
    glUniformMatrix4fv(m_vpLoc,      1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform3fv(m_bboxMinLoc,       1, glm::value_ptr(bbox.min));
    glUniform3fv(m_bboxMaxLoc,       1, glm::value_ptr(bbox.max));

    glBindVertexArray(m_proxyVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

#endif // !ANDROID
