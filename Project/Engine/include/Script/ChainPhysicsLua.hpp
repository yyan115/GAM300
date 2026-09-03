#pragma once

#include <lua.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ChainPhysicsWrappers {
namespace Detail {

constexpr double kEpsilon = 1.0e-6;

struct Particle {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double previousX = 0.0;
    double previousY = 0.0;
    double previousZ = 0.0;
    double inverseMass = 1.0;
    bool hasInverseMass = false;
};

struct Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline bool ReadPoint(lua_State* state, int tableIndex, Point& point) {
    tableIndex = lua_absindex(state, tableIndex);
    if (!lua_istable(state, tableIndex)) return false;

    double values[3]{};
    for (int component = 1; component <= 3; ++component) {
        lua_rawgeti(state, tableIndex, component);
        if (!lua_isnumber(state, -1)) {
            lua_pop(state, 1);
            return false;
        }
        values[component - 1] = lua_tonumber(state, -1);
        lua_pop(state, 1);
    }

    point.x = values[0];
    point.y = values[1];
    point.z = values[2];
    return true;
}

inline bool ReadPointAt(lua_State* state, int outerTableIndex, int pointIndex, Point& point) {
    outerTableIndex = lua_absindex(state, outerTableIndex);
    lua_rawgeti(state, outerTableIndex, pointIndex);
    const bool result = ReadPoint(state, -1, point);
    lua_pop(state, 1);
    return result;
}

inline bool ReadPointField(lua_State* state, int tableIndex, const char* field, Point& point) {
    tableIndex = lua_absindex(state, tableIndex);
    lua_getfield(state, tableIndex, field);
    const bool result = ReadPoint(state, -1, point);
    lua_pop(state, 1);
    return result;
}

inline double NumberField(lua_State* state, int tableIndex, const char* field, double fallback) {
    tableIndex = lua_absindex(state, tableIndex);
    lua_getfield(state, tableIndex, field);
    const double result = lua_isnumber(state, -1) ? lua_tonumber(state, -1) : fallback;
    lua_pop(state, 1);
    return result;
}

inline bool OptionalNumberField(
    lua_State* state,
    int tableIndex,
    const char* field,
    double& value)
{
    tableIndex = lua_absindex(state, tableIndex);
    lua_getfield(state, tableIndex, field);
    const bool present = lua_isnumber(state, -1);
    if (present) value = lua_tonumber(state, -1);
    lua_pop(state, 1);
    return present;
}

inline bool BooleanField(lua_State* state, int tableIndex, const char* field) {
    tableIndex = lua_absindex(state, tableIndex);
    lua_getfield(state, tableIndex, field);
    const bool result = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return result;
}

inline bool LoadParticles(
    lua_State* state,
    int positionsIndex,
    int previousIndex,
    int inverseMassIndex,
    int count,
    std::vector<Particle>& particles)
{
    particles.resize(static_cast<std::size_t>(count));
    for (int index = 1; index <= count; ++index) {
        Point position;
        Point previous;
        if (!ReadPointAt(state, positionsIndex, index, position) ||
            !ReadPointAt(state, previousIndex, index, previous)) {
            return false;
        }

        Particle& particle = particles[static_cast<std::size_t>(index - 1)];
        particle.x = position.x;
        particle.y = position.y;
        particle.z = position.z;
        particle.previousX = previous.x;
        particle.previousY = previous.y;
        particle.previousZ = previous.z;

        lua_rawgeti(state, inverseMassIndex, index);
        particle.hasInverseMass = lua_isnumber(state, -1);
        particle.inverseMass = particle.hasInverseMass ? lua_tonumber(state, -1) : 1.0;
        lua_pop(state, 1);
    }
    return true;
}

inline void WritePointAt(lua_State* state, int outerTableIndex, int pointIndex, const Point& point) {
    outerTableIndex = lua_absindex(state, outerTableIndex);
    lua_rawgeti(state, outerTableIndex, pointIndex);
    if (lua_istable(state, -1)) {
        lua_pushnumber(state, point.x);
        lua_rawseti(state, -2, 1);
        lua_pushnumber(state, point.y);
        lua_rawseti(state, -2, 2);
        lua_pushnumber(state, point.z);
        lua_rawseti(state, -2, 3);
    }
    lua_pop(state, 1);
}

inline void StoreParticles(
    lua_State* state,
    int positionsIndex,
    int previousIndex,
    const std::vector<Particle>& particles)
{
    for (std::size_t index = 0; index < particles.size(); ++index) {
        const Particle& particle = particles[index];
        WritePointAt(state, positionsIndex, static_cast<int>(index + 1),
            Point{particle.x, particle.y, particle.z});
        WritePointAt(state, previousIndex, static_cast<int>(index + 1),
            Point{particle.previousX, particle.previousY, particle.previousZ});
    }
}

inline void PinStart(std::vector<Particle>& particles, const Point& start) {
    Particle& first = particles.front();
    first.x = first.previousX = start.x;
    first.y = first.previousY = start.y;
    first.z = first.previousZ = start.z;
}

inline void PinEnd(std::vector<Particle>& particles, const Point& end) {
    Particle& last = particles.back();
    last.x = last.previousX = end.x;
    last.y = last.previousY = end.y;
    last.z = last.previousZ = end.z;
}

inline void SolvePair(Particle& first, Particle& second, double targetDistance) {
    const double deltaX = second.x - first.x;
    const double deltaY = second.y - first.y;
    const double deltaZ = second.z - first.z;
    double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    if (distance < kEpsilon) distance = kEpsilon;

    const double difference = (distance - targetDistance) / distance;
    const double firstMass = first.hasInverseMass ? first.inverseMass : 0.0;
    const double secondMass = second.hasInverseMass ? second.inverseMass : 0.0;
    const double totalMass = firstMass + secondMass;
    if (totalMass <= kEpsilon) return;

    const double firstFactor = (firstMass / totalMass) * difference;
    const double secondFactor = (secondMass / totalMass) * difference;
    if (firstMass > 0.0) {
        first.x += deltaX * firstFactor;
        first.y += deltaY * firstFactor;
        first.z += deltaZ * firstFactor;
    }
    if (secondMass > 0.0) {
        second.x -= deltaX * secondFactor;
        second.y -= deltaY * secondFactor;
        second.z -= deltaZ * secondFactor;
    }
}

inline void StrictClamp(std::vector<Particle>& particles, double maximumDistance, bool fromEnd) {
    const int count = static_cast<int>(particles.size());
    if (fromEnd) {
        for (int index = count - 2; index >= 0; --index) {
            Particle& anchor = particles[static_cast<std::size_t>(index + 1)];
            Particle& point = particles[static_cast<std::size_t>(index)];
            const double deltaX = point.x - anchor.x;
            const double deltaY = point.y - anchor.y;
            const double deltaZ = point.z - anchor.z;
            const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
            if (distance > maximumDistance && distance > kEpsilon) {
                const double scale = maximumDistance / distance;
                point.x = anchor.x + deltaX * scale;
                point.y = anchor.y + deltaY * scale;
                point.z = anchor.z + deltaZ * scale;
            }
        }
        return;
    }

    for (int index = 1; index < count; ++index) {
        Particle& anchor = particles[static_cast<std::size_t>(index - 1)];
        Particle& point = particles[static_cast<std::size_t>(index)];
        const double deltaX = point.x - anchor.x;
        const double deltaY = point.y - anchor.y;
        const double deltaZ = point.z - anchor.z;
        const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
        if (distance > maximumDistance && distance > kEpsilon) {
            const double scale = maximumDistance / distance;
            point.x = anchor.x + deltaX * scale;
            point.y = anchor.y + deltaY * scale;
            point.z = anchor.z + deltaZ * scale;
        }
    }
}

} // namespace Detail

// Native variable-timestep equivalent of extension/verletAdapter.lua. It loads
// the Lua particle arrays once, solves entirely in C++, then writes them back.
inline int Step(lua_State* state) {
    if (!lua_istable(state, 1) || !lua_istable(state, 2) ||
        !lua_istable(state, 3) || !lua_isnumber(state, 4) ||
        !lua_istable(state, 5)) {
        lua_pushboolean(state, 0);
        return 1;
    }

    const int positionsIndex = lua_absindex(state, 1);
    const int previousIndex = lua_absindex(state, 2);
    const int inverseMassIndex = lua_absindex(state, 3);
    const int parametersIndex = lua_absindex(state, 5);
    const double deltaTime = lua_tonumber(state, 4);
    const int availableCount = static_cast<int>(lua_rawlen(state, positionsIndex));
    const int count = static_cast<int>(std::floor(Detail::NumberField(
        state, parametersIndex, "n", static_cast<double>(availableCount))));

    if (deltaTime <= 0.0 || count <= 0 || count > availableCount ||
        count > static_cast<int>(lua_rawlen(state, previousIndex))) {
        lua_pushboolean(state, 0);
        return 1;
    }

    static thread_local std::vector<Detail::Particle> particles;
    if (!Detail::LoadParticles(
            state, positionsIndex, previousIndex, inverseMassIndex, count, particles)) {
        lua_pushboolean(state, 0);
        return 1;
    }

    const double gravity = std::abs(Detail::NumberField(
        state, parametersIndex, "VerletGravity", 9.81));
    const double damping = std::clamp(Detail::NumberField(
        state, parametersIndex, "VerletDamping", 0.02), 0.0, 1.0);
    const double iterationValue = std::min(20.0, Detail::NumberField(
        state, parametersIndex, "ConstraintIterations", 2.0));
    const int iterations = iterationValue >= 1.0
        ? static_cast<int>(std::floor(iterationValue))
        : 0;
    const bool isElastic = Detail::BooleanField(state, parametersIndex, "IsElastic");
    const bool pinnedLast = Detail::BooleanField(state, parametersIndex, "pinnedLast");

    double linkMaximum = 0.0;
    const bool hasLinkMaximum = Detail::OptionalNumberField(
        state, parametersIndex, "LinkMaxDistance", linkMaximum);
    double totalLength = 0.000001;
    Detail::OptionalNumberField(state, parametersIndex, "totalLen", totalLength);
    double segmentLength = count > 1
        ? std::max(totalLength, Detail::kEpsilon) / static_cast<double>(count - 1)
        : 0.0;
    Detail::OptionalNumberField(state, parametersIndex, "segmentLen", segmentLength);
    double clampSegment = 0.0;
    if (Detail::OptionalNumberField(state, parametersIndex, "ClampSegment", clampSegment)) {
        segmentLength = std::min(segmentLength, clampSegment);
    }
    double targetDistance = segmentLength;
    if (!isElastic && hasLinkMaximum && targetDistance > linkMaximum) {
        targetDistance = linkMaximum;
    }

    Detail::Point startPoint;
    Detail::Point endPoint;
    const bool hasStart = Detail::ReadPointField(
        state, parametersIndex, "startPos", startPoint);
    const bool hasEnd = Detail::ReadPointField(
        state, parametersIndex, "endPos", endPoint);
    const bool pinEnd = pinnedLast && hasEnd;
    const bool solveBidirectionally = !isElastic && pinEnd;

    const bool groundClamp = Detail::BooleanField(state, parametersIndex, "GroundClamp");
    double groundHeight = 0.0;
    const bool hasGroundHeight = Detail::OptionalNumberField(
        state, parametersIndex, "groundY", groundHeight);

    const double subStepValue = std::max(1.0, Detail::NumberField(
        state, parametersIndex, "SubSteps", 4.0));
    const int subSteps = static_cast<int>(std::floor(subStepValue));
    const double subDeltaTime = deltaTime / subStepValue;
    const double squaredDeltaTime = subDeltaTime * subDeltaTime;
    const double velocityScale = 1.0 - damping;

    for (int step = 0; step < subSteps; ++step) {
        for (Detail::Particle& particle : particles) {
            const double inverseMass = particle.hasInverseMass ? particle.inverseMass : 1.0;
            if (inverseMass <= 0.0) continue;

            const double x = particle.x;
            const double y = particle.y;
            const double z = particle.z;
            const double velocityX = (x - particle.previousX) * velocityScale;
            const double velocityY = (y - particle.previousY) * velocityScale;
            const double velocityZ = (z - particle.previousZ) * velocityScale;
            particle.previousX = x;
            particle.previousY = y;
            particle.previousZ = z;
            particle.x = x + velocityX;
            particle.y = y + velocityY - gravity * squaredDeltaTime;
            particle.z = z + velocityZ;
        }

        if (hasStart) Detail::PinStart(particles, startPoint);
        if (pinEnd) Detail::PinEnd(particles, endPoint);

        for (int iteration = 0; iteration < iterations; ++iteration) {
            if (solveBidirectionally) {
                for (int index = 1; index < count; ++index) {
                    Detail::SolvePair(
                        particles[static_cast<std::size_t>(index - 1)],
                        particles[static_cast<std::size_t>(index)],
                        targetDistance);
                }
                for (int index = count - 1; index >= 1; --index) {
                    Detail::SolvePair(
                        particles[static_cast<std::size_t>(index - 1)],
                        particles[static_cast<std::size_t>(index)],
                        targetDistance);
                }
            } else {
                for (int index = 1; index < count; ++index) {
                    Detail::SolvePair(
                        particles[static_cast<std::size_t>(index - 1)],
                        particles[static_cast<std::size_t>(index)],
                        targetDistance);
                }
            }

            if (hasStart) Detail::PinStart(particles, startPoint);
            if (pinEnd) Detail::PinEnd(particles, endPoint);
        }

        if (!isElastic && hasLinkMaximum && linkMaximum > 0.0) {
            Detail::StrictClamp(particles, linkMaximum, pinEnd);
        }

        if (groundClamp && hasGroundHeight) {
            for (Detail::Particle& particle : particles) {
                const double inverseMass = particle.hasInverseMass ? particle.inverseMass : 0.0;
                if (inverseMass > 0.0 && particle.y < groundHeight) {
                    particle.y = groundHeight;
                    particle.previousY = groundHeight;
                }
            }
        }
    }

    Detail::StoreParticles(state, positionsIndex, previousIndex, particles);
    lua_pushboolean(state, 1);
    return 1;
}

} // namespace ChainPhysicsWrappers
