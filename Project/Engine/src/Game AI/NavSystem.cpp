#include "pch.h"
#include "Game AI/NavSystem.hpp"

NavSystem& NavSystem::Get()
{
    static NavSystem instance;
    return instance;
}

void NavSystem::Build(PhysicsSystem& phys, ECSManager& ecsManager)
{
    grid.Build(phys, ecsManager);
    built = true;
}

std::vector<Vector3D> NavSystem::RequestPathXZ(float sx, float sz, float gx, float gz)
{
    if (!built) return {};
    auto [sr, sc] = grid.WorldToCell(sx, sz);
    auto [gr, gc] = grid.WorldToCell(gx, gz);

    /*std::cout << "[Nav] start cell r=" << sr << " c=" << sc
        << " walkable=" << grid.Walkable(sr, sc) << "\n";

    std::cout << "[Nav] goal  cell r=" << gr << " c=" << gc
        << " walkable=" << grid.Walkable(gr, gc) << "\n";*/

    return astar.FindPath(grid, sx, sz, gx, gz);
}
