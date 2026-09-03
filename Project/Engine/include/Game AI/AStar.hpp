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
        // Search generation this node was last reset for. Nodes carrying an
        // older generation are treated as untouched, so a request only pays
        // for the cells it visits instead of zero-filling the whole grid.
        uint32_t generation = 0;
        bool closed = false;
        bool open = false;
    };

    static float HeuristicOctile(const GridPos& a, const GridPos& b);

    // Persistent per-cell scratch storage. Sized to the grid on first use and
    // reused across requests; the nav grid is hundreds of thousands of cells,
    // so allocating and clearing it per path request was a multi-megabyte
    // hitch every time an enemy repathed.
    std::vector<Node> m_nodes;
    uint32_t m_generation = 0;
};
