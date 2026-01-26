#pragma once
#include "pch.h"
#include <cstdint>

struct GridPos
{
    int row = 0;
    int col = 0;

    GridPos() = default;
    GridPos(int r, int c) : row(r), col(c) {}

    bool operator==(const GridPos& other) const
    {
        return row == other.row && col == other.col;
    }

    bool operator!=(const GridPos& other) const
    {
        return !(*this == other);
    }
};
