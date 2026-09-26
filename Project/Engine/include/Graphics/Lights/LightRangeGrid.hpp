#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// Conservative world-space membership for the existing point-light ranges.
// Bits follow the lighting UBO order; no light is removed from that list.
class LightRangeGrid {
public:
    static constexpr int Resolution = 64;
    static constexpr int MaxLights = 16;
    struct Light { float x, y, z, range; };

    bool Build(const Light* lights, size_t count) {
        valid = false;
        if (count == 0 || count > MaxLights) return false;

        std::array<double, 3> lower, upper;
        lower.fill(std::numeric_limits<double>::infinity());
        upper.fill(-std::numeric_limits<double>::infinity());
        uint16_t unbounded = 0;
        bool hasBoundedLight = false;
        for (size_t i = 0; i < count; ++i) {
            const auto& light = lights[i];
            const float position[] = { light.x, light.y, light.z };
            if (!std::isfinite(light.x) || !std::isfinite(light.y) ||
                !std::isfinite(light.z) || !std::isfinite(light.range)) return false;
            if (light.range <= 0.0f) {
                unbounded |= uint16_t(1u << i);
                continue;
            }
            // A shader distance can underflow for extremely small coordinates.
            // Keep its original evaluation instead of making a geometric rejection.
            if (light.range < 16.0f * std::sqrt(std::numeric_limits<float>::min())) return false;
            hasBoundedLight = true;
            for (int axis = 0; axis < 3; ++axis) {
                lower[axis] = std::min(lower[axis], double(position[axis]) - light.range);
                upper[axis] = std::max(upper[axis], double(position[axis]) + light.range);
            }
        }
        if (!hasBoundedLight) return false;

        for (int axis = 0; axis < 3; ++axis) {
            origin[axis] = std::nextafter(float(lower[axis]), -std::numeric_limits<float>::infinity());
            const float end = std::nextafter(float(upper[axis]), std::numeric_limits<float>::infinity());
            const double span = double(end) - origin[axis];
            if (!std::isfinite(origin[axis]) || !std::isfinite(end) || !(span > 0.0)) return false;
            inverseCell[axis] = float(Resolution / span);
            if (!std::isfinite(inverseCell[axis]) || inverseCell[axis] <= 0.0f) return false;
        }

        cells.assign(Resolution * Resolution * Resolution, unbounded);
        for (size_t i = 0; i < count; ++i) {
            const auto& light = lights[i];
            if (light.range <= 0.0f) continue;
            const float position[] = { light.x, light.y, light.z };
            int first[3], last[3];
            for (int axis = 0; axis < 3; ++axis) {
                const double lo = (double(position[axis]) - light.range - origin[axis]) * inverseCell[axis];
                const double hi = (double(position[axis]) + light.range - origin[axis]) * inverseCell[axis];
                // Outward bounds plus neighboring cells cover coordinate rounding
                // at cell boundaries. A false positive only performs extra work.
                first[axis] = std::clamp(int(std::floor(lo)) - 1, 0, Resolution - 1);
                last[axis] = std::clamp(int(std::floor(hi)) + 1, 0, Resolution - 1);
            }
            const uint16_t bit = uint16_t(1u << i);
            for (int z = first[2]; z <= last[2]; ++z)
                for (int y = first[1]; y <= last[1]; ++y)
                    for (int x = first[0]; x <= last[0]; ++x)
                        cells[(z * Resolution + y) * Resolution + x] |= bit;
        }
        valid = true;
        return true;
    }

    bool IsValid() const { return valid; }
    const std::array<float, 3>& Origin() const { return origin; }
    const std::array<float, 3>& InverseCell() const { return inverseCell; }
    const std::vector<uint16_t>& Cells() const { return cells; }

private:
    bool valid = false;
    std::array<float, 3> origin{}, inverseCell{};
    std::vector<uint16_t> cells;
};
