#include "pch.h"
#include "Graphics/Instancing/InstanceBatch.hpp"
#include "Graphics/Model/Model.h"
#include "Graphics/Material.hpp"
#include "Graphics/ShaderClass.h"
#include "ECS/ECSRegistry.hpp"
#include "Logging.hpp"

InstanceBatch::InstanceBatch() {
	m_instances.reserve(INITIAL_CAPACITY);
}

InstanceBatch::~InstanceBatch() {
    m_instanceVBO.Delete();
}

InstanceBatch::InstanceBatch(InstanceBatch&& other) noexcept
    : m_model(std::move(other.m_model))
    , m_material(std::move(other.m_material))
    , m_shader(std::move(other.m_shader))
    , m_instances(std::move(other.m_instances))
    , m_instanceVBO(std::move(other.m_instanceVBO))
    , m_bufferCapacity(other.m_bufferCapacity)
    , m_bufferDirty(other.m_bufferDirty)
    , m_initialized(other.m_initialized)
{
    other.m_bufferCapacity = 0;
    other.m_initialized = false;
}

InstanceBatch& InstanceBatch::operator=(InstanceBatch&& other) noexcept {
    if (this != &other) 
    {
        m_instanceVBO.Delete(); 

        m_model = std::move(other.m_model);
        m_material = std::move(other.m_material);
        m_shader = std::move(other.m_shader);
        m_instances = std::move(other.m_instances);
        m_instanceVBO = std::move(other.m_instanceVBO);
        m_bufferCapacity = other.m_bufferCapacity;
        m_bufferDirty = other.m_bufferDirty;
        m_initialized = other.m_initialized;

        other.m_bufferCapacity = 0;
        other.m_initialized = false;
    }
    return *this;
}

void InstanceBatch::Initialize(std::shared_ptr<Model> model, std::shared_ptr<Material> material, std::shared_ptr<Shader> shader)
{
    m_model = model;
    m_material = material;
    m_shader = shader;

    CreateInstanceBuffer();
    m_initialized = true;
}

void InstanceBatch::CreateInstanceBuffer()
{
    m_instanceVBO.InitializeBuffer(INITIAL_CAPACITY * sizeof(InstanceData), GL_DYNAMIC_DRAW);
    m_bufferCapacity = INITIAL_CAPACITY;
}

void InstanceBatch::Clear()
{   
    m_instances.clear();
    m_bufferDirty = false;
}

void InstanceBatch::AddInstance(const glm::mat4& modelMatrix, const glm::vec3& bloomColor, float bloomIntensity)
{
    InstanceData data;
    data.modelMatrix = modelMatrix;
    data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(modelMatrix))));
    data.bloomData = glm::vec4(bloomColor, bloomIntensity);

    m_instances.push_back(data);
    m_bufferDirty = true;
}

void InstanceBatch::Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos)
{
    if (m_instances.empty() || !m_model || !m_shader)
    {
        return;
    }

    UpdateInstanceBuffer();
    m_shader->Activate();
    m_shader->setBool("useInstancing", true);

    if (m_material)
    {
        m_material->ApplyToShader(*m_shader);
    }

    for (auto& mesh : m_model->meshes)
    {
        mesh.DrawInstanced(*m_shader, m_instanceVBO, static_cast<GLsizei>(m_instances.size()));
    }
}

void InstanceBatch::RenderDepthOnly(const glm::mat4& lightSpaceMatrix)
{
    if (m_instances.empty() || !m_model) 
    {
        return;
    }

    UpdateInstanceBuffer();

    // Draw each mesh with instancing (depth only)
    for (auto& mesh : m_model->meshes) 
    {
        mesh.DrawInstancedDepthOnly(m_instanceVBO, static_cast<GLsizei>(m_instances.size()));
    }
}

size_t InstanceBatch::GetSortKey() const
{
    // Create a sort key based on shader, material, and model pointers
    // This allows efficient grouping and comparison
    size_t key = 0;
    if (m_shader) key ^= reinterpret_cast<size_t>(m_shader.get());
    if (m_material) key ^= (reinterpret_cast<size_t>(m_material.get()) << 16);
    if (m_model) key ^= (reinterpret_cast<size_t>(m_model.get()) << 32);
    return key;
}

void InstanceBatch::UpdateInstanceBuffer()
{
    // Force a default capacity so we aren't allocating tiny memory chunks
    if (m_bufferCapacity < 100) m_bufferCapacity = 100;

    if (m_instanceVBO.ID == 0)
    {
        m_instanceVBO.InitializeBuffer(m_bufferCapacity * sizeof(InstanceData), GL_DYNAMIC_DRAW);
    }

    if (!m_bufferDirty || m_instances.empty())
    {
        return;
    }

    // Check if we need to grow the buffer (Buffer Orphaning)
    if (m_instances.size() > m_bufferCapacity)
    {
        size_t newCapacity = m_bufferCapacity;
        while (newCapacity < m_instances.size())
        {
            newCapacity *= 2; // Assuming your GROWTH_FACTOR is 2
        }
        m_instanceVBO.InitializeBuffer(newCapacity * sizeof(InstanceData), GL_DYNAMIC_DRAW);
        m_bufferCapacity = newCapacity;
    }

    // Upload instance data
    m_instanceVBO.UpdateData(m_instances.data(), m_instances.size() * sizeof(InstanceData), 0);
    m_bufferDirty = false;
}

void InstanceBatch::Prewarm() {
    if (m_instanceVBO.ID == 0) {
        if (m_bufferCapacity < 100) m_bufferCapacity = 100;
        m_instanceVBO.InitializeBuffer(m_bufferCapacity * sizeof(InstanceData), GL_DYNAMIC_DRAW);
    }
}