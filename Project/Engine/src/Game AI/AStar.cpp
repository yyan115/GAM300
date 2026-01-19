#include "pch.h"
#include "Game AI/AStar.hpp"

static inline bool SamePos(const GridPos& a, const GridPos& b)
{
    return a.row == b.row && a.col == b.col;
}

float AStar::HeuristicOctile(const GridPos& a, const GridPos& b)
{
    float dx = static_cast<float>(std::abs(a.col - b.col));
    float dz = static_cast<float>(std::abs(a.row - b.row));
    float mn = std::min(dx, dz);
    float mx = std::max(dx, dz);
    return mn * 1.41421356f + (mx - mn);
}

std::vector<Vector3D> AStar::FindPath(const NavGrid& grid, float sx, float sz, float gx, float gz)
{
    GridPos start = grid.WorldToCell(sx, sz);
    GridPos goal = grid.WorldToCell(gx, gz);

    if (!grid.Walkable(goal.row, goal.col))
    {
        std::cout
            << "[AStar] Goal cell is NOT walkable: ("
            << goal.row << ", " << goal.col << ")\n";
        return {};
    }

    if (!grid.InBounds(start.row, start.col) || !grid.InBounds(goal.row, goal.col))
        return {};

    if (!grid.Walkable(start.row, start.col) || !grid.Walkable(goal.row, goal.col))
        return {};

    const int R = grid.Rows();
    const int C = grid.Cols();
    std::vector<Node> nodes(R * C);

    auto idx = [C](int r, int c) { return r * C + c; };

    struct PQItem {
        float f;
        int r, c;
        bool operator<(const PQItem& other) const { return f > other.f; } // min-heap
    };

    std::priority_queue<PQItem> open;

    Node& sN = nodes[idx(start.row, start.col)];
    sN.g = 0.0f;
    sN.f = HeuristicOctile(start, goal);
    sN.parent = { -1, -1 };
    sN.open = true;
    open.push({ sN.f, start.row, start.col });

    // 8-neighbors with no-corner-cutting diagonals
    const int dirs8[8][2] = {
        {-1, 0}, { 1, 0}, {0,-1}, {0, 1},
        {-1,-1}, {-1, 1}, {1,-1}, {1, 1}
    };

    while (!open.empty())
    {
        auto cur = open.top(); open.pop();
        Node& curN = nodes[idx(cur.r, cur.c)];
        if (curN.closed) continue;

        curN.closed = true;

        GridPos curPos{ cur.r, cur.c };
        if (SamePos(curPos, goal))
            break;

        for (int i = 0; i < 8; ++i)
        {
            int nr = cur.r + dirs8[i][0];
            int nc = cur.c + dirs8[i][1];
            if (!grid.InBounds(nr, nc)) continue;
            if (!grid.Walkable(nr, nc)) continue;

            // diagonal corner-cut prevention
            bool diag = (dirs8[i][0] != 0 && dirs8[i][1] != 0);
            if (diag)
            {
                int ar1 = cur.r;             int ac1 = nc; // horiz
                int ar2 = nr;                int ac2 = cur.c; // vert
                if (!grid.Walkable(ar1, ac1) || !grid.Walkable(ar2, ac2))
                    continue;
            }

            float stepCost = diag ? 1.41421356f : 1.0f;
            float tentativeG = curN.g + stepCost;

            Node& nN = nodes[idx(nr, nc)];
            if (tentativeG < nN.g)
            {
                nN.g = tentativeG;
                GridPos nPos{ nr, nc };
                nN.f = tentativeG + HeuristicOctile(nPos, goal);
                nN.parent = curPos;
                open.push({ nN.f, nr, nc });
            }
        }
    }

    // reconstruct if reached
    Node& gN = nodes[idx(goal.row, goal.col)];
    if (!std::isfinite(gN.g))
        return {};

    std::vector<Vector3D> path;
    GridPos cur = goal;

    while (!(cur.row == -1 && cur.col == -1))
    {
        path.push_back(grid.CellToWorld(cur.row, cur.col));
        Node& n = nodes[idx(cur.row, cur.col)];
        cur = n.parent;
    }

    std::reverse(path.begin(), path.end());

    // FORCE last waypoint to exact goal cell center
    if (!path.empty())
    {
        GridPos goalCell = grid.WorldToCell(gx, gz);
        path.back() = grid.CellToWorld(goalCell.row, goalCell.col);
    }

    return path;
}
