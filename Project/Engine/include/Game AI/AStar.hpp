#pragma once
#include "pch.h"
#include <queue>
#include <limits>

#include "Game AI/NavGrid.hpp"
#include "Math/Vector3D.hpp"
#include "Game AI/GridPos.hpp"

class AStar {
public:
    std::vector<Vector3D> FindPath(const NavGrid& grid, float sx, float sz, float gx, float gz);
    static GridPos FindNearestWalkable(const NavGrid& grid, const GridPos& target);

private:
    struct Node {
        float g = std::numeric_limits<float>::infinity();
        float f = std::numeric_limits<float>::infinity();
        GridPos parent = { -1, -1 };
        bool closed = false;
        bool open = false;
    };

    static float HeuristicOctile(const GridPos& a, const GridPos& b);
};
