#pragma once
#include "pch.h"

#include "Game AI/NavGrid.hpp"
#include "Game AI/AStar.hpp" // you need to create this
#include "Logging.hpp"

class NavSystem {
public:
    static NavSystem& Get();

    void Build(PhysicsSystem& phys, ECSManager& ecsManager);

    std::vector<Vector3D> RequestPathXZ(float sx, float sz, float gx, float gz, Entity entity);

    float GetGroundY(Entity entity);

    // Optional config passthroughs
    NavGrid& GetGrid() { return grid; }
    const NavGrid& GetGrid() const { return grid; }

private:
    NavGrid grid;
    AStar astar;
    bool built = false;
};
