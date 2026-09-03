#include "pch.h"
#include "Graphics/Instancing/InstanceBatch.hpp"
#include "Graphics/Model/Model.h"
#include "Graphics/Material.hpp"
#include "Graphics/ShaderClass.h"
#include "ECS/ECSRegistry.hpp"
#include "Logging.hpp"
#include <cmath>
#include <cstring>
#ifdef ANDROID
#include <glm/gtc/packing.hpp>
#endif

namespace {
#ifdef ANDROID
std::uint16_t PackInstanceHalf(float value)
{
    return glm::packHalf1x16(glm::clamp(value, -65504.0f, 65504.0f));
}

void PackInstanceSource(
    InstanceData& destination,
    const glm::mat4& modelMatrix,
    const glm::vec4& bloomData,
    std::uint32_t lightMask)
{
    for (int column = 0; column < 3; ++column) {
        destination.modelColumns[column * 4 + 0] = modelMatrix[column][0];
        destination.modelColumns[column * 4 + 1] = modelMatrix[column][1];
        destination.modelColumns[column * 4 + 2] = modelMatrix[column][2];
        destination.modelColumns[column * 4 + 3] = modelMatrix[3][column];
    }
    for (int component = 0; component < 4; ++component) {
        destination.bloomData[component] = PackInstanceHalf(bloomData[component]);
    }
    destination.lightMask = lightMask;
}

bool HasSameInstanceSource(const InstanceData& previous, const InstanceData& candidate)
{
    return std::memcmp(
               previous.modelColumns,
               candidate.modelColumns,
               sizeof(candidate.modelColumns)) == 0 &&
           std::memcmp(
               previous.bloomData,
               candidate.bloomData,
               sizeof(candidate.bloomData)) == 0 &&
           previous.lightMask == candidate.lightMask;
}

void StoreNormalMatrix(InstanceData& destination, const glm::mat3& normalMatrix)
{
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            destination.normalMatrixColumns[column * 3 + row] =
                PackInstanceHalf(normalMatrix[column][row]);
        }
    }
}
#else
void StoreNormalMatrix(InstanceData& destination, const glm::mat3& normalMatrix)
{
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            destination.normalMatrixColumns[column * 3 + row] =
                normalMatrix[column][row];
        }
    }
}
#endif
}

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
    , m_instanceCount(other.m_instanceCount)
    , m_instanceVBO(std::move(other.m_instanceVBO))
    , m_bufferCapacity(other.m_bufferCapacity)
    , m_bufferDirty(other.m_bufferDirty)
    , m_initialized(other.m_initialized)
    , m_depthBucket(other.m_depthBucket)
    , m_minCameraDistanceSq(other.m_minCameraDistanceSq)
    , m_hasBloomEmission(other.m_hasBloomEmission)
{
    other.m_instanceCount = 0;
    other.m_bufferCapacity = 0;
    other.m_initialized = false;
    other.m_hasBloomEmission = false;
}

InstanceBatch& InstanceBatch::operator=(InstanceBatch&& other) noexcept {
    if (this != &other) 
    {
        m_instanceVBO.Delete(); 

        m_model = std::move(other.m_model);
        m_material = std::move(other.m_material);
        m_shader = std::move(other.m_shader);
        m_instances = std::move(other.m_instances);
        m_instanceCount = other.m_instanceCount;
        m_instanceVBO = std::move(other.m_instanceVBO);
        m_bufferCapacity = other.m_bufferCapacity;
        m_bufferDirty = other.m_bufferDirty;
        m_initialized = other.m_initialized;
        m_depthBucket = other.m_depthBucket;
        m_minCameraDistanceSq = other.m_minCameraDistanceSq;
        m_hasBloomEmission = other.m_hasBloomEmission;

        other.m_instanceCount = 0;
        other.m_bufferCapacity = 0;
        other.m_initialized = false;
        other.m_hasBloomEmission = false;
    }
    return *this;
}

void InstanceBatch::Initialize(
    const std::shared_ptr<Model>& model,
    const std::shared_ptr<Material>& material,
    const std::shared_ptr<Shader>& shader)
{
    m_model = model;
    m_material = material;
    m_shader = shader;

    m_initialized = true;
}

void InstanceBatch::Clear()
{   
    // Retain last frame's CPU data so unchanged static batches avoid a GPU
    // upload. AddInstance overwrites only slots whose values changed.
    m_instanceCount = 0;
    m_bufferDirty = false;
    m_depthBucket = std::numeric_limits<int>::max();
    m_minCameraDistanceSq = std::numeric_limits<float>::infinity();
    m_hasBloomEmission = false;
}

void InstanceBatch::AddInstance(
    const glm::mat4& modelMatrix,
    const glm::vec3& bloomColor,
    float bloomIntensity,
    std::uint32_t lightMask,
    float cameraDistanceSq)
{
    m_hasBloomEmission =
        m_hasBloomEmission || bloomIntensity > 0.01f;

    m_minCameraDistanceSq = std::min(
        m_minCameraDistanceSq, glm::max(cameraDistanceSq, 0.0f));

    const glm::vec4 bloomData(bloomColor, bloomIntensity);
#ifdef ANDROID
    InstanceData data{};
    PackInstanceSource(data, modelMatrix, bloomData, lightMask);
    if (m_instanceCount < m_instances.size() &&
        HasSameInstanceSource(m_instances[m_instanceCount], data))
    {
        ++m_instanceCount;
        return;
    }
#else
    if (m_instanceCount < m_instances.size())
    {
        InstanceData& previous = m_instances[m_instanceCount];
        if (std::memcmp(&previous.modelMatrix, &modelMatrix, sizeof(modelMatrix)) == 0 &&
            std::memcmp(&previous.bloomData, &bloomData, sizeof(bloomData)) == 0)
        {
            if (previous.lightMask != lightMask) {
                previous.lightMask = lightMask;
                m_bufferDirty = true;
            }
            ++m_instanceCount;
            return;
        }
    }

    InstanceData data{};
    data.modelMatrix = modelMatrix;
#endif

    // Normal matrix = transpose(inverse(mat3(model))).
    // glm::inverse is expensive (determinant + adjugate). For the common case of
    // uniform scale we can skip the inverse: the normal matrix is just the rotation
    // matrix, which is the upper-left 3x3 of the model matrix divided by the scale.
    // Non-uniform scale falls back to the full inverse.
    const glm::mat3 m3(modelMatrix);
    float sx2 = glm::dot(m3[0], m3[0]);
    float sy2 = glm::dot(m3[1], m3[1]);
    float sz2 = glm::dot(m3[2], m3[2]);
    const float tolerance = glm::max(1e-6f, sx2 * 1e-4f);
    if (glm::abs(sx2 - sy2) < tolerance &&
        glm::abs(sx2 - sz2) < tolerance &&
        glm::abs(glm::dot(m3[0], m3[1])) < tolerance &&
        glm::abs(glm::dot(m3[0], m3[2])) < tolerance &&
        glm::abs(glm::dot(m3[1], m3[2])) < tolerance &&
        sx2 > 1e-8f)
    {
        // Uniform scale: normal matrix = rotation matrix (normalised columns)
        float invScale = 1.0f / glm::sqrt(sx2);
        const glm::mat3 normalMatrix = m3 * invScale;
        StoreNormalMatrix(data, normalMatrix);
    }
    else
    {
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(m3));
        StoreNormalMatrix(data, normalMatrix);
    }

#ifndef ANDROID
    data.bloomData = bloomData;
    data.lightMask = lightMask;
#endif

    if (m_instanceCount == m_instances.size())
    {
        m_instances.push_back(data);
        m_bufferDirty = true;
    }
    else if (std::memcmp(&m_instances[m_instanceCount], &data, sizeof(InstanceData)) != 0)
    {
        m_instances[m_instanceCount] = data;
        m_bufferDirty = true;
    }
    ++m_instanceCount;
}

void InstanceBatch::FinalizeDepthBucket()
{
    if (m_instanceCount == 0 || !std::isfinite(m_minCameraDistanceSq)) {
        m_depthBucket = std::numeric_limits<int>::max();
        return;
    }

    constexpr float kInverseDepthBucketSize = 1.0f / 5.0f;
    m_depthBucket = static_cast<int>(
        glm::sqrt(m_minCameraDistanceSq) * kInverseDepthBucketSize);
}

void InstanceBatch::Render()
{
    if (m_instanceCount == 0 || !m_model || !m_shader)
    {
        return;
    }

    UpdateInstanceBuffer();

    for (auto& mesh : m_model->meshes)
    {
        mesh.DrawInstanced(
            *m_shader,
            m_instanceVBO,
            static_cast<GLsizei>(m_instanceCount),
            m_material == nullptr);
    }
}

void InstanceBatch::RenderDepthOnly(const glm::mat4& lightSpaceMatrix)
{
    if (m_instanceCount == 0 || !m_model)
    {
        return;
    }

    UpdateInstanceBuffer();

    // Draw each mesh with instancing (depth only)
    for (auto& mesh : m_model->meshes) 
    {
        mesh.DrawInstancedDepthOnly(m_instanceVBO, static_cast<GLsizei>(m_instanceCount));
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
    if (m_bufferCapacity == 0) {
        m_bufferCapacity = INITIAL_CAPACITY;
    }

    if (m_instanceVBO.ID == 0)
    {
        m_instanceVBO.InitializeBuffer(m_bufferCapacity * sizeof(InstanceData), GL_DYNAMIC_DRAW);
    }

    if (!m_bufferDirty || m_instanceCount == 0)
    {
        return;
    }

    // Check if we need to grow the buffer (Buffer Orphaning)
    if (m_instanceCount > m_bufferCapacity)
    {
        size_t newCapacity = m_bufferCapacity;
        while (newCapacity < m_instanceCount)
        {
            newCapacity *= 2; // Assuming your GROWTH_FACTOR is 2
        }
        m_instanceVBO.InitializeBuffer(newCapacity * sizeof(InstanceData), GL_DYNAMIC_DRAW);
        m_bufferCapacity = newCapacity;
    }

    // Upload instance data
    m_instanceVBO.UpdateData(m_instances.data(), m_instanceCount * sizeof(InstanceData), 0);
    m_bufferDirty = false;
}

void InstanceBatch::Prewarm(size_t expectedInstanceCount) {
    size_t requiredCapacity = std::max(
        expectedInstanceCount, INITIAL_CAPACITY);
    size_t targetCapacity = INITIAL_CAPACITY;
    while (targetCapacity < requiredCapacity) {
        targetCapacity *= GROWTH_FACTOR;
    }

    if (m_instances.capacity() < targetCapacity) {
        m_instances.reserve(targetCapacity);
    }

    if (m_instanceVBO.ID == 0 || m_bufferCapacity < targetCapacity) {
        m_instanceVBO.InitializeBuffer(
            targetCapacity * sizeof(InstanceData), GL_DYNAMIC_DRAW);
        m_bufferCapacity = targetCapacity;
        // The GPU store was re-specified, so retained CPU instance data no
        // longer matches it. Force a full upload on the next render.
        m_bufferDirty = true;
    }
}
