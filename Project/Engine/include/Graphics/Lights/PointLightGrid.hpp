#pragma once
#include "Graphics/Lights/LightRangeGrid.hpp"
#include "Graphics/OpenGL.h"

class Shader;

// Owned by GraphicsManager, with explicit cleanup while its context is current.
class PointLightGrid {
public:
    PointLightGrid() = default;
    PointLightGrid(const PointLightGrid&) = delete;
    PointLightGrid& operator=(const PointLightGrid&) = delete;
    void Update(const LightRangeGrid::Light* lights, size_t count);
    void Apply(Shader& shader) const;
    void Shutdown();

private:
    // Material textures use 0-7; directional and point shadows use 8-12.
    static constexpr int TextureUnit = 13;
    LightRangeGrid grid;
    std::array<LightRangeGrid::Light, LightRangeGrid::MaxLights> previousLights{};
    size_t previousCount = 0;
    bool prepared = false;
    GLuint texture = 0;
};
