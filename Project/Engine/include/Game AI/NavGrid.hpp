#pragma once
#include "pch.h"

#include "Physics/PhysicsSystem.hpp"
#include "Math/Vector3D.hpp"
#include "Game AI/GridPos.hpp"
#include "ECS/ECSManager.hpp"
#include "Physics/ColliderComponent.hpp"

struct NavCell {
    bool walkable = false;
    float groundY = 0.0f;
};

class NavGrid {
public:
    void Build(PhysicsSystem& phys, ECSManager& ecsManager);

    bool InBounds(int r, int c) const;
    bool Walkable(int r, int c) const;

    GridPos WorldToCell(float x, float z) const;
    Vector3D CellToWorld(int r, int c) const;

    int Rows() const { return rows; }
    int Cols() const { return cols; }

    const NavCell& GetNavCell(int row, int col);

    // config
    float cellSize = 0.1f;
    float minX = -20, maxX = 20;
    float minZ = -20, maxZ = 20;

    // agent params
    float agentRadius = 0.2f;
    float agentHalfHeight = 0.4f;
    float groundProbeTop = 10.0f;
    float groundProbeDist = 30.0f;

private:
    int rows = 0, cols = 0;
    std::vector<NavCell> cells;
};
