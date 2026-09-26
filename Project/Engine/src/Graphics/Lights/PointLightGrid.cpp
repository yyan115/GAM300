#include "pch.h"
#include "Logging.hpp"
#include "Graphics/Lights/PointLightGrid.hpp"
#include "Graphics/ShaderClass.h"
#include <cstring>

void PointLightGrid::Update(const LightRangeGrid::Light* lights, size_t count)
{
    PROFILE_SCOPED("Lighting::PointLightGrid");
    static_assert(sizeof(LightRangeGrid::Light) == sizeof(float) * 4);
    if (prepared && previousCount == count &&
        (count == 0 || std::memcmp(previousLights.data(), lights, count * sizeof(LightRangeGrid::Light)) == 0)) return;

    prepared = true;
    previousCount = count;
    if (count <= previousLights.size())
        std::copy_n(lights, count, previousLights.begin());
    else
        prepared = false;
    if (!grid.Build(lights, count)) return;

    glActiveTexture(GL_TEXTURE0 + TextureUnit);
    if (!texture) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_R16UI, LightRangeGrid::Resolution,
                       LightRangeGrid::Resolution, LightRangeGrid::Resolution);
    } else {
        glBindTexture(GL_TEXTURE_3D, texture);
    }
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, LightRangeGrid::Resolution,
                    LightRangeGrid::Resolution, LightRangeGrid::Resolution,
                    GL_RED_INTEGER, GL_UNSIGNED_SHORT, grid.Cells().data());
    glActiveTexture(GL_TEXTURE0);
}

void PointLightGrid::Apply(Shader& shader) const
{
    if (!shader.UsesPointLightGrid()) return;
    glActiveTexture(GL_TEXTURE0 + TextureUnit);
    glBindTexture(GL_TEXTURE_3D, texture);
    glActiveTexture(GL_TEXTURE0);
    shader.setInt("u_pointLightGrid", TextureUnit);
    shader.setBool("u_lightGridReady", prepared && texture && grid.IsValid());
    const auto& origin = grid.Origin();
    const auto& inverse = grid.InverseCell();
    shader.setVec3("u_lightGridOrigin", origin[0], origin[1], origin[2]);
    shader.setVec3("u_lightGridInverseCell", inverse[0], inverse[1], inverse[2]);
}

void PointLightGrid::Shutdown()
{
    if (texture) glDeleteTextures(1, &texture);
    texture = 0;
    prepared = false;
    previousCount = 0;
}
