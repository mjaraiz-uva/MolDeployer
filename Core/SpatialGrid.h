#pragma once

#include <vector>
#include <utility>
#include <memory>
#include "Vec3.h"

namespace DABM {

class Agent;

// 3D cell-list spatial hash for efficient neighbor detection.
// The simulation box is divided into cubic cells of side length = cutoff radius.
// At each timestep, agents are binned into cells, then neighbor pairs within
// the cutoff distance are found by checking the 27 neighboring cells (3x3x3).
class SpatialGrid {
public:
    SpatialGrid();

    void Configure(const Vec3& boxSize, double cutoffRadius);

    // Rebuild the grid from current agent positions. O(N).
    void Build(const std::vector<std::unique_ptr<Agent>>& agents);

    // Get all pairs of agent indices within the cutoff radius.
    std::vector<std::pair<int, int>> GetNeighborPairs() const;

    // Get neighbors of a specific agent within cutoff.
    std::vector<int> GetNeighbors(int agentIndex) const;

    double GetCutoffRadius() const { return m_cutoffRadius; }

private:
    int CellIndex(int cx, int cy, int cz) const;
    void PositionToCell(const Vec3& pos, int& cx, int& cy, int& cz) const;

    Vec3 m_boxSize;
    double m_cutoffRadius = 3.0;
    int m_nx = 0, m_ny = 0, m_nz = 0;
    int m_totalCells = 0;
    std::vector<std::vector<int>> m_cells;
    std::vector<Vec3> m_agentPositions;
};

} // namespace DABM
