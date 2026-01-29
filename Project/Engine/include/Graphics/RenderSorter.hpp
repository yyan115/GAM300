#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include "Engine.h"

// Forward declarations
class Shader;
class Material;
class Model;

// ============================================================================
// RENDER LAYER - Separate enum to avoid MSVC issues with nested enum class
// ============================================================================
namespace RenderLayer {
    enum Type : uint8_t {
        LAYER_OPAQUE = 0,
        LAYER_ALPHA_TEST = 1,
        LAYER_TRANSPARENT = 2,
        LAYER_UI = 3
    };
}

// ============================================================================
// RENDER KEY - 64-bit sort key for state-based sorting
// ============================================================================
// Bit layout:
//   [63-56] : Render layer (8 bits)  - opaque/transparent/UI
//   [55-40] : Shader ID (16 bits)    - PRIMARY sort criterion
//   [39-24] : Material ID (16 bits)  - SECONDARY sort criterion
//   [23-8]  : Mesh/Model ID (16 bits)- TERTIARY sort criterion
//   [7-0]   : Depth key (8 bits)     - for transparent sorting

class RenderSortKey {
public:
    uint64_t key;

    // Shift constants
    static const uint64_t LAYER_SHIFT = 56ULL;
    static const uint64_t SHADER_SHIFT = 40ULL;
    static const uint64_t MATERIAL_SHIFT = 24ULL;
    static const uint64_t MESH_SHIFT = 8ULL;

    // Masks
    static const uint64_t SHADER_MASK = 0xFFFFULL << SHADER_SHIFT;
    static const uint64_t MATERIAL_MASK = 0xFFFFULL << MATERIAL_SHIFT;

    RenderSortKey() : key(0) {}

    RenderSortKey(RenderLayer::Type layer, uint16_t shaderId, uint16_t materialId, uint16_t meshId, uint8_t depthKey = 0) {
        uint64_t layerPart = static_cast<uint64_t>(layer) << LAYER_SHIFT;
        uint64_t shaderPart = static_cast<uint64_t>(shaderId) << SHADER_SHIFT;
        uint64_t materialPart = static_cast<uint64_t>(materialId) << MATERIAL_SHIFT;
        uint64_t meshPart = static_cast<uint64_t>(meshId) << MESH_SHIFT;
        uint64_t depthPart = static_cast<uint64_t>(depthKey);

        key = layerPart | shaderPart | materialPart | meshPart | depthPart;
    }

    bool operator<(const RenderSortKey& other) const {
        return key < other.key;
    }

    // Check if shader is different (would require shader switch)
    bool DifferentShader(const RenderSortKey& other) const {
        return (key & SHADER_MASK) != (other.key & SHADER_MASK);
    }

    // Check if material is different
    bool DifferentMaterial(const RenderSortKey& other) const {
        return (key & MATERIAL_MASK) != (other.key & MATERIAL_MASK);
    }
};

// ============================================================================
// RESOURCE ID CACHE - Maps pointers to stable numeric IDs for sorting
// ============================================================================
class ResourceIdCache {
public:
    uint16_t GetShaderId(Shader* shader) {
        if (!shader) return 0;
        std::unordered_map<Shader*, uint16_t>::iterator it = shaderIds.find(shader);
        if (it != shaderIds.end()) return it->second;
        uint16_t id = nextShaderId++;
        shaderIds[shader] = id;
        return id;
    }

    uint16_t GetMaterialId(Material* material) {
        if (!material) return 0;
        std::unordered_map<Material*, uint16_t>::iterator it = materialIds.find(material);
        if (it != materialIds.end()) return it->second;
        uint16_t id = nextMaterialId++;
        materialIds[material] = id;
        return id;
    }

    uint16_t GetModelId(Model* model) {
        if (!model) return 0;
        std::unordered_map<Model*, uint16_t>::iterator it = modelIds.find(model);
        if (it != modelIds.end()) return it->second;
        uint16_t id = nextModelId++;
        modelIds[model] = id;
        return id;
    }

    void Clear() {
        shaderIds.clear();
        materialIds.clear();
        modelIds.clear();
        nextShaderId = 1;
        nextMaterialId = 1;
        nextModelId = 1;
    }

private:
    std::unordered_map<Shader*, uint16_t> shaderIds;
    std::unordered_map<Material*, uint16_t> materialIds;
    std::unordered_map<Model*, uint16_t> modelIds;
    uint16_t nextShaderId = 1;
    uint16_t nextMaterialId = 1;
    uint16_t nextModelId = 1;
};

// ============================================================================
// RENDER STATISTICS
// ============================================================================
struct SortingStats {
    int totalObjects;
    int drawCalls;
    int shaderSwitches;
    int materialSwitches;

    SortingStats() : totalObjects(0), drawCalls(0), shaderSwitches(0), materialSwitches(0) {}

    void Reset() {
        totalObjects = 0;
        drawCalls = 0;
        shaderSwitches = 0;
        materialSwitches = 0;
    }
};